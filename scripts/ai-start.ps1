$root = Split-Path -Parent $PSScriptRoot
$python = Join-Path $root 'ai\.venv\Scripts\python.exe'
if (-not (Test-Path $python)) { $python = 'python' }
Remove-Item (Join-Path $root 'ai\state\stop.requested') -Force -ErrorAction SilentlyContinue
$p = Start-Process -FilePath $python -ArgumentList (Join-Path $root 'ai\watchdog.py') -WorkingDirectory $root -PassThru -WindowStyle Hidden
$p.Id | Set-Content (Join-Path $root 'ai\state\watchdog.pid')
Write-Output "FLUX AI watchdog started: PID $($p.Id)"
