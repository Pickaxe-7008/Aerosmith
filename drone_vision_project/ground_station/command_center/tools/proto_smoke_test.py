import pathlib
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parents[3]
PROTO_PY_DIR = REPO_ROOT / "common" / "compiled" / "python"
sys.path.insert(0, str(PROTO_PY_DIR))

import messages_pb2


def main() -> None:
    envelope = messages_pb2.Envelope(
        schema_version=1,
        msg_type=messages_pb2.MESSAGE_TYPE_TELEMETRY,
        seq=42,
        timestamp_ms=123456789,
    )
    envelope.telemetry.ping_ms = 37

    payload = envelope.SerializeToString()

    decoded = messages_pb2.Envelope()
    decoded.ParseFromString(payload)

    assert decoded.msg_type == messages_pb2.MESSAGE_TYPE_TELEMETRY
    assert decoded.telemetry.ping_ms == 37
    print("proto_smoke_test: ok")


if __name__ == "__main__":
    main()

