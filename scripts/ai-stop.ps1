$root = Split-Path -Parent $PSScriptRoot
New-Item -ItemType File -Force (Join-Path $root 'ai\state\stop.requested') | Out-Null
$pidFile=Join-Path $root 'ai\state\watchdog.pid'
if (Test-Path $pidFile) { $id=[int](Get-Content $pidFile); Stop-Process -Id $id -ErrorAction SilentlyContinue }
Write-Output 'FLUX AI stop requested; current child exits at checkpoint.'
