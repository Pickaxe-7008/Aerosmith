import argparse
import pathlib
import subprocess
import sys


def run_protoc(repo_root: pathlib.Path, proto_file: pathlib.Path) -> None:
    proto_dir = repo_root / "common" / "proto"
    cpp_out = repo_root / "common" / "compiled" / "cpp"
    py_out = repo_root / "common" / "compiled" / "python"

    cpp_out.mkdir(parents=True, exist_ok=True)
    py_out.mkdir(parents=True, exist_ok=True)

    cmd = [
        "protoc",
        "-I",
        str(proto_dir),
        "--cpp_out",
        str(cpp_out),
        "--python_out",
        str(py_out),
        str(proto_file),
    ]

    print("[gen_protos] running:", " ".join(cmd))
    subprocess.run(cmd, check=True)
    print("[gen_protos] generated C++ and Python bindings")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate protobuf bindings for this repo")
    parser.add_argument(
        "--repo-root",
        default=str(pathlib.Path(__file__).resolve().parents[2]),
        help="Repository root path",
    )
    parser.add_argument(
        "--proto",
        default="messages.proto",
        help="Proto filename under common/proto",
    )
    args = parser.parse_args()

    repo_root = pathlib.Path(args.repo_root).resolve()
    proto_file = repo_root / "common" / "proto" / args.proto

    if not proto_file.exists():
        print(f"[gen_protos] missing proto file: {proto_file}", file=sys.stderr)
        return 1

    try:
        run_protoc(repo_root, proto_file)
    except FileNotFoundError:
        print("[gen_protos] protoc not found. Install protoc-wheel-0 or protobuf compiler.", file=sys.stderr)
        return 2
    except subprocess.CalledProcessError as exc:
        print(f"[gen_protos] protoc failed with exit code {exc.returncode}", file=sys.stderr)
        return exc.returncode

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

