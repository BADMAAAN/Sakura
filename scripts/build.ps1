[CmdletBinding()]
param(
    [switch]$Clean,
    [ValidateSet('flux-delta', 'flux-t-embed-cc1101', 'lilygo-t-embed-cc1101')]
    [string]$Environment = 'flux-t-embed-cc1101',
    [string]$Python = $env:FLUX_PYTHON
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $Python) { $Python = Join-Path $projectRoot '.venv\Scripts\python.exe' }
if (-not (Test-Path -LiteralPath $Python)) { throw 'Missing Python; use -Python or see BUILD.md.' }
$python = (Resolve-Path -LiteralPath $Python).Path
$venvScripts = Split-Path -Parent $python
$localCore = Join-Path $projectRoot '.tools\platformio-core'
if (-not $env:PLATFORMIO_CORE_DIR -and (Test-Path -LiteralPath $localCore)) {
    $env:PLATFORMIO_CORE_DIR = $localCore
}

$active = Get-CimInstance Win32_Process | Where-Object {
    $_.ProcessId -ne $PID -and
    $_.CommandLine -match '(pio\.exe|platformio run|scripts\\pio\.py)' -and
    $_.CommandLine -notlike '*Get-CimInstance*'
}
if ($active) { throw ('Another PlatformIO process is active (PID: {0}).' -f ($active.ProcessId -join ', ')) }

$env:PATH = [string]::Concat($venvScripts, [IO.Path]::PathSeparator, $env:PATH)
Push-Location $projectRoot
try {
    # Windows PowerShell surfaces native stderr (including compiler warnings)
    # as ErrorRecord objects. PlatformIO's exit code remains authoritative.
    $ErrorActionPreference = 'Continue'
    if ($Clean) {
        & $python -X utf8 'scripts\pio.py' run -e $Environment -t clean
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    & $python -X utf8 'scripts\pio.py' run -e $Environment -t build-firmware
    exit $LASTEXITCODE
}
finally { Pop-Location }
