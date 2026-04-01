# Command Center (Ground Station)

This module hosts the Dear ImGui command UI and local ZeroMQ transport bridge.

## Current thread model

- UI thread: GLFW + ImGui render loop in `main.cpp`
- `DroneSubscriberThread`: receives serialized `Envelope` messages from the drone link on `tcp://*:5060`
- `InferenceWorkerThread`: exchanges serialized `Envelope` messages with worker on `tcp://127.0.0.1:5050`

Shared runtime state lives in `app_state.h` and is synchronized with atomics/mutexes.

## Transport contract (protobuf)

- Wire format: serialized `aerosmith.v1.Envelope`
- Drone link message types:
  - `MESSAGE_TYPE_HEARTBEAT`
  - `MESSAGE_TYPE_TELEMETRY`
  - `MESSAGE_TYPE_VIDEO_FRAME`
- Local worker message types:
  - `MESSAGE_TYPE_CONTROL_COMMAND`
  - `MESSAGE_TYPE_INFERENCE_RESULT`

Schema file: `common/proto/messages.proto`.

Generated bindings used by this module:

- C++: `common/compiled/cpp/messages.pb.h`, `common/compiled/cpp/messages.pb.cc`
- Python: `common/compiled/python/messages_pb2.py`

To regenerate bindings locally (preferred):

```powershell
powershell -ExecutionPolicy Bypass -File "A:\CodeWorkspaces\Aerosmith\drone_vision_project\common\proto\gen_protos.ps1"
```

Manual fallback:

```powershell
protoc -I "A:\CodeWorkspaces\Aerosmith\drone_vision_project\common\proto" --cpp_out="A:\CodeWorkspaces\Aerosmith\drone_vision_project\common\compiled\cpp" --python_out="A:\CodeWorkspaces\Aerosmith\drone_vision_project\common\compiled\python" "A:\CodeWorkspaces\Aerosmith\drone_vision_project\common\proto\messages.proto"
```

## Video rendering notes

- If OpenCV is available at configure time, JPEG payloads are decoded and rendered in the Scan Feed window.
- If OpenCV is not found, the UI shows a diagnostic message instead of a frame.

## Quick local smoke test

1. Start the command center binary (build steps depend on your local CMake/deps setup).
2. Start the mock worker.
3. Start the mock drone publisher.

```powershell
python -u "A:\CodeWorkspaces\Aerosmith\drone_vision_project\ground_station\command_center\tools\mock_inference_worker.py"
python -u "A:\CodeWorkspaces\Aerosmith\drone_vision_project\ground_station\command_center\tools\mock_drone_publisher.py"
```

The mock publisher emits a known-valid embedded 1x1 JPEG by default. You can override with your own fixture:

```powershell
python -u "A:\CodeWorkspaces\Aerosmith\drone_vision_project\ground_station\command_center\tools\mock_drone_publisher.py" --jpeg-fixture "C:\path\to\frame.jpg" --width 1280 --height 720
```

Expected behavior:

- UI status changes to connected.
- Ping value updates.
- Latest inference JSON appears in the Scan Feed window.
- Clicking mode buttons sends mode-change messages to the local worker.

## One-command mock health check

Run both mock services together and print a final PASS/FAIL summary:

```powershell
python -u "A:\CodeWorkspaces\Aerosmith\drone_vision_project\ground_station\command_center\tools\local_mock_runner.py"
```

Example summary fields:

- `worker_started` / `publisher_started`: startup log marker seen.
- `worker_rc` / `publisher_rc`: child process exit code.
- `status=PASS`: both services started and exited cleanly.

