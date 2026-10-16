# Native Windows Codex, Aider and Ollama workflow

The required executor order is Codex, then Aider. Aider is installed by
`ai\.venv\Scripts\aider-install.exe` and discovered at `%USERPROFILE%\.local\bin\aider.exe`.
It talks directly to `http://127.0.0.1:11434` using `ollama_chat/qwen2.5-coder:14b`,
then `qwen2.5:14b` and `qwen2.5:latest`. Reviews always use `qwen2.5:14b`.
OpenHands is an optional future backend and is never in the active executor order.

The reviewer is a critical second engineer on the same task, not an independent implementer.

```powershell
python ai\reviewer.py architecture --task "Review the proposed FLUX fork architecture" --file ARCHITECTURE.md
python ai\reviewer.py review --task "Review the current implementation" --diff
python ai\reviewer.py debug --task "Diagnose the failed baseline build" --compiler-output ai\context\build.log
python ai\reviewer.py research --task "Challenge these hardware conclusions" --context "Facts and source URLs"
```

The default model is `qwen2.5:14b`. Override it with `FLUX_OLLAMA_MODEL`. Each completed exchange is stored under `ai/logs/`; curated conclusions belong in ADRs and project documentation rather than raw chat transcripts.
