param(
    [string]$RepoRoot = "",
    [string]$Proto = "messages.proto"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}

$scriptPath = Join-Path $PSScriptRoot "gen_protos.py"

python -u "$scriptPath" --repo-root "$RepoRoot" --proto "$Proto"

