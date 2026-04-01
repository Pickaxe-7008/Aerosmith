import argparse
import pathlib
import time
import sys

import zmq
from google.protobuf.message import DecodeError

REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
PROTO_PY_DIR = REPO_ROOT / "common" / "compiled" / "python"
sys.path.insert(0, str(PROTO_PY_DIR))

import messages_pb2


def now_ms() -> int:
    return int(time.time() * 1000)


def main() -> None:
    parser = argparse.ArgumentParser(description="Mock local inference worker")
    parser.add_argument("--endpoint", default="tcp://127.0.0.1:5050")
    parser.add_argument("--count", type=int, default=0, help="number of inference messages; 0 means run forever")
    args = parser.parse_args()

    context = zmq.Context.instance()
    socket = context.socket(zmq.DEALER)
    socket.connect(args.endpoint)

    print(f"[mock_inference_worker] connected to {args.endpoint}")

    frame_id = 0
    last_emit = 0.0
    current_mode = messages_pb2.SCAN_MODE_NONE

    try:
        sent_count = 0
        while True:
            if socket.poll(timeout=50, flags=zmq.POLLIN):
                message = socket.recv()
                envelope = messages_pb2.Envelope()
                try:
                    envelope.ParseFromString(message)
                except DecodeError:
                    envelope = None

                if envelope and envelope.msg_type == messages_pb2.MESSAGE_TYPE_CONTROL_COMMAND and envelope.HasField("control_command"):
                    current_mode = envelope.control_command.requested_mode

            now = time.time()
            if now - last_emit >= 0.25:
                result = messages_pb2.Envelope(
                    schema_version=1,
                    msg_type=messages_pb2.MESSAGE_TYPE_INFERENCE_RESULT,
                    seq=frame_id,
                    timestamp_ms=now_ms(),
                )
                result.inference_result.frame_id = frame_id
                result.inference_result.mode = current_mode
                result.inference_result.inference_latency_ms = 22
                bbox = result.inference_result.detections.add()
                bbox.label = "tree"
                bbox.confidence = 0.89
                bbox.x = 120.0
                bbox.y = 80.0
                bbox.w = 64.0
                bbox.h = 160.0

                socket.send(result.SerializeToString())
                frame_id += 1
                sent_count += 1
                if args.count > 0 and sent_count >= args.count:
                    break
                last_emit = now
    except KeyboardInterrupt:
        pass
    finally:
        socket.close(0)


if __name__ == "__main__":
    main()

