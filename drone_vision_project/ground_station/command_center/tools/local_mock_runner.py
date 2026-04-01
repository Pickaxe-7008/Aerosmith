import argparse
import queue
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Dict, List, Tuple


def _stream_reader(prefix: str, pipe, out_queue: "queue.Queue[Tuple[str, str]]") -> None:
    try:
        for line in iter(pipe.readline, ""):
            if not line:
                break
            out_queue.put((prefix, line.rstrip()))
    finally:
        try:
            pipe.close()
        except Exception:
            pass


def _start_child(script: Path, args: List[str], name: str) -> Tuple[subprocess.Popen, "queue.Queue[Tuple[str, str]]", threading.Thread]:
    cmd = [sys.executable, "-u", str(script)] + args
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    assert proc.stdout is not None
    q: "queue.Queue[Tuple[str, str]]" = queue.Queue()
    t = threading.Thread(target=_stream_reader, args=(name, proc.stdout, q), daemon=True)
    t.start()
    return proc, q, t


def _drain_queues(queues: List["queue.Queue[Tuple[str, str]]"], deadline: float, print_logs: bool) -> List[Tuple[str, str]]:
    captured: List[Tuple[str, str]] = []
    while time.time() < deadline:
        had_data = False
        for q in queues:
            try:
                item = q.get_nowait()
                captured.append(item)
                had_data = True
                if print_logs:
                    print(f"[{item[0]}] {item[1]}")
            except queue.Empty:
                pass
        if not had_data:
            time.sleep(0.02)
    return captured


def _terminate(proc: subprocess.Popen, timeout_s: float = 3.0) -> None:
    if proc.poll() is not None:
        return
    proc.terminate()
    try:
        proc.wait(timeout=timeout_s)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.wait(timeout=timeout_s)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run mock inference worker and drone publisher together")
    parser.add_argument("--duration", type=float, default=3.0, help="seconds to observe logs and liveness")
    parser.add_argument("--publisher-hz", type=float, default=10.0)
    parser.add_argument("--publisher-endpoint", default="tcp://127.0.0.1:5060")
    parser.add_argument("--worker-endpoint", default="tcp://127.0.0.1:5050")
    parser.add_argument("--quiet", action="store_true", help="suppress child log streaming")
    args = parser.parse_args()

    tools_dir = Path(__file__).resolve().parent
    worker_script = tools_dir / "mock_inference_worker.py"
    publisher_script = tools_dir / "mock_drone_publisher.py"

    if not worker_script.exists() or not publisher_script.exists():
        print("local_mock_runner: missing mock scripts in tools directory", file=sys.stderr)
        return 2

    pub_count = max(1, int(args.duration * max(args.publisher_hz, 1.0)))
    worker_count = max(1, int(args.duration / 0.25) + 1)

    start = time.time()
    worker_proc, worker_q, worker_t = _start_child(
        worker_script,
        ["--endpoint", args.worker_endpoint, "--count", str(worker_count)],
        "worker",
    )
    publisher_proc, publisher_q, publisher_t = _start_child(
        publisher_script,
        ["--endpoint", args.publisher_endpoint, "--hz", str(args.publisher_hz), "--count", str(pub_count)],
        "publisher",
    )

    captured = _drain_queues([worker_q, publisher_q], deadline=time.time() + args.duration, print_logs=not args.quiet)

    # Allow finite-count processes to exit naturally before forcing shutdown.
    for proc in (publisher_proc, worker_proc):
        try:
            proc.wait(timeout=3.0)
        except subprocess.TimeoutExpired:
            _terminate(proc)

    worker_t.join(timeout=1.0)
    publisher_t.join(timeout=1.0)

    # Drain final lines.
    captured.extend(_drain_queues([worker_q, publisher_q], deadline=time.time() + 0.2, print_logs=not args.quiet))

    lines_by_name: Dict[str, List[str]] = {"worker": [], "publisher": []}
    for name, line in captured:
        lines_by_name.setdefault(name, []).append(line)

    worker_started = any("[mock_inference_worker] connected" in line for line in lines_by_name.get("worker", []))
    publisher_started = any("[mock_drone_publisher] connected" in line for line in lines_by_name.get("publisher", []))

    worker_rc = worker_proc.returncode
    publisher_rc = publisher_proc.returncode
    elapsed = time.time() - start

    ok = worker_started and publisher_started and worker_rc == 0 and publisher_rc == 0

    print("\n=== local_mock_runner summary ===")
    print(f"elapsed_s={elapsed:.2f}")
    print(f"worker_started={worker_started} worker_rc={worker_rc}")
    print(f"publisher_started={publisher_started} publisher_rc={publisher_rc}")
    print(f"status={'PASS' if ok else 'FAIL'}")

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())

