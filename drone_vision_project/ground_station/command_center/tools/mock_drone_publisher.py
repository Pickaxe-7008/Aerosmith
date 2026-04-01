import argparse
import base64
import pathlib
import random
import time
import sys

import zmq

REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
PROTO_PY_DIR = REPO_ROOT / "common" / "compiled" / "python"
sys.path.insert(0, str(PROTO_PY_DIR))

import messages_pb2


# Known-valid 1x1 JPEG (black pixel), used to verify UI decode/render path deterministically.
DEFAULT_TINY_JPEG = base64.b64decode(
    b"/9j/4AAQSkZJRgABAQAAAQABAAD/2wCEAAkGBxAQEBAQEA8PEA8QDw8PDw8PDw8QFREWFhURFRUYHSggGBolGxUVITEhJSkrLi4uFx8zODMtNygtLisBCgoKDg0OFQ8QFSsdFR0rKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrKysrK//AABEIAAEAAQMBIgACEQEDEQH/xAAXAAEBAQEAAAAAAAAAAAAAAAABAgAD/8QAFhEBAQEAAAAAAAAAAAAAAAAAAQAC/9oADAMBAAIQAxAAAAHbM//EABYQAQEBAAAAAAAAAAAAAAAAAAEAEv/aAAgBAQABBQKf/8QAFhEBAQEAAAAAAAAAAAAAAAAAABEB/9oACAEDAQE/Aaf/xAAVEQEBAAAAAAAAAAAAAAAAAAABEP/aAAgBAgEBPwGn/8QAGhAAAwEAAwAAAAAAAAAAAAAAAQIREiExQf/aAAgBAQAGPwK4rmd//8QAGxABAQADAQEBAAAAAAAAAAAAAREAITFBUWH/2gAIAQEAAT8hN3VQ2V8fM8o1f//aAAwDAQACAAMAAAAQ8//EABYRAQEBAAAAAAAAAAAAAAAAABEAIf/aAAgBAwEBPxBqf//EABYRAQEBAAAAAAAAAAAAAAAAABEAIf/aAAgBAgEBPxBqf//EABsQAQADAQADAQAAAAAAAAAAAAEAESExQVFh/9oACAEBAAE/EGxA7fCqJ6mN7A7Vx2xWQ9r5e1nQ4//Z"
)


def now_ms() -> int:
    return int(time.time() * 1000)


def main() -> None:
    parser = argparse.ArgumentParser(description="Mock drone publisher for command_center subscriber tests")
    parser.add_argument("--endpoint", default="tcp://127.0.0.1:5060")
    parser.add_argument("--hz", type=float, default=10.0)
    parser.add_argument("--count", type=int, default=0, help="number of publish cycles; 0 means run forever")
    parser.add_argument("--jpeg-fixture", default="", help="optional path to a JPEG fixture file")
    parser.add_argument("--width", type=int, default=1, help="frame width metadata sent in VideoFrame")
    parser.add_argument("--height", type=int, default=1, help="frame height metadata sent in VideoFrame")
    args = parser.parse_args()

    period_s = 1.0 / max(args.hz, 1.0)
    jpeg_bytes = DEFAULT_TINY_JPEG
    if args.jpeg_fixture:
        fixture_path = pathlib.Path(args.jpeg_fixture).expanduser().resolve()
        if not fixture_path.exists():
            raise FileNotFoundError(f"JPEG fixture not found: {fixture_path}")
        jpeg_bytes = fixture_path.read_bytes()

    context = zmq.Context.instance()
    socket = context.socket(zmq.PUB)
    socket.connect(args.endpoint)

    frame_id = 0
    print(
        f"[mock_drone_publisher] connected to {args.endpoint} at {args.hz:.1f} Hz "
        f"using {'embedded tiny JPEG' if not args.jpeg_fixture else args.jpeg_fixture}"
    )

    try:
        loop_count = 0
        while True:
            heartbeat = messages_pb2.Envelope(
                schema_version=1,
                msg_type=messages_pb2.MESSAGE_TYPE_HEARTBEAT,
                seq=frame_id,
                timestamp_ms=now_ms(),
            )
            heartbeat.heartbeat.preferred_link = messages_pb2.LINK_TYPE_WIFI
            heartbeat.heartbeat.uptime_s = int(time.time())
            socket.send(heartbeat.SerializeToString())

            ping_ms = random.randint(25, 90)
            telemetry = messages_pb2.Envelope(
                schema_version=1,
                msg_type=messages_pb2.MESSAGE_TYPE_TELEMETRY,
                seq=frame_id,
                timestamp_ms=now_ms(),
            )
            telemetry.telemetry.ping_ms = ping_ms
            telemetry.telemetry.active_mode = messages_pb2.SCAN_MODE_OBSTACLE
            socket.send(telemetry.SerializeToString())

            video = messages_pb2.Envelope(
                schema_version=1,
                msg_type=messages_pb2.MESSAGE_TYPE_VIDEO_FRAME,
                seq=frame_id,
                timestamp_ms=now_ms(),
            )
            video.video_frame.frame_id = frame_id
            video.video_frame.width = max(1, args.width)
            video.video_frame.height = max(1, args.height)
            video.video_frame.codec = "jpeg"
            video.video_frame.payload = jpeg_bytes
            video.video_frame.capture_timestamp_ms = now_ms()
            socket.send(video.SerializeToString())

            frame_id += 1
            loop_count += 1
            if args.count > 0 and loop_count >= args.count:
                break
            time.sleep(period_s)
    except KeyboardInterrupt:
        pass
    finally:
        socket.close(0)


if __name__ == "__main__":
    main()

