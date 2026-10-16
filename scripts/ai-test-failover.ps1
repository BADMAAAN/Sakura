$root = Split-Path -Parent $PSScriptRoot
$python = Join-Path $root 'ai\.venv\Scripts\python.exe'; if(-not(Test-Path $python)){$python='python'}
& $python (Join-Path $root 'ai\supervisor.py') --simulate-codex-unavailable --prompt 'CURRENT_TASK=ai-infrastructure-001. Create ai/e2e/aider_failover_probe.txt containing exactly: AIDER_OLLAMA_FAILOVER_OK' --probe-file 'ai/e2e/aider_failover_probe.txt' --probe-contents 'AIDER_OLLAMA_FAILOVER_OK'
exit $LASTEXITCODE
