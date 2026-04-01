# Protobuf Generation

This directory contains source contracts and generation helpers for shared wire messages.

## Inputs

- `messages.proto`

## Outputs

- C++ bindings: `common/compiled/cpp/messages.pb.h`, `common/compiled/cpp/messages.pb.cc`
- Python bindings: `common/compiled/python/messages_pb2.py`

## Regenerate

```powershell
powershell -ExecutionPolicy Bypass -File "A:\CodeWorkspaces\Aerosmith\drone_vision_project\common\proto\gen_protos.ps1"
```

Or run Python directly:

```powershell
python -u "A:\CodeWorkspaces\Aerosmith\drone_vision_project\common\proto\gen_protos.py" --repo-root "A:\CodeWorkspaces\Aerosmith\drone_vision_project"
```

