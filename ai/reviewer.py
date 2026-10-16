#!/usr/bin/env python3
"""Local Ollama engineering reviewer for the FLUX project."""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import pathlib
import subprocess
import sys
import urllib.error
import urllib.request


ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_MODEL = os.environ.get("FLUX_OLLAMA_MODEL", "qwen2.5:14b")
OLLAMA_URL = os.environ.get("OLLAMA_URL", "http://127.0.0.1:11434/api/chat")
MAX_FILE_BYTES = 256_000


def read_text(path: pathlib.Path) -> str:
    resolved = path.resolve()
    try:
        resolved.relative_to(ROOT)
    except ValueError as exc:
        raise ValueError(f"Context file must be inside {ROOT}: {path}") from exc
    data = resolved.read_bytes()
    if len(data) > MAX_FILE_BYTES:
        raise ValueError(f"Context file exceeds {MAX_FILE_BYTES} bytes: {path}")
    return data.decode("utf-8", errors="replace")


def git_diff() -> str:
    result = subprocess.run(
        ["git", "diff", "--no-ext-diff", "--"],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    sections = [result.stdout or "(no tracked diff)"]
    untracked = subprocess.run(
        ["git", "ls-files", "--others", "--exclude-standard"],
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    for relative in untracked.stdout.splitlines():
        path = ROOT / relative
        if path.parts[:2] in (("ai", "context"), ("ai", "logs")):
            continue
        if not path.is_file() or path.stat().st_size > MAX_FILE_BYTES:
            continue
        try:
            content = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        sections.append(f"UNTRACKED FILE: {relative}\n{content}")
    return "\n\n".join(sections)


def build_prompt(args: argparse.Namespace) -> str:
    sections = [f"MODE: {args.mode}", f"TASK:\n{args.task}"]
    for item in args.file:
        path = (ROOT / item).resolve()
        sections.append(f"FILE: {path.relative_to(ROOT)}\n{read_text(path)}")
    if args.diff:
        sections.append(f"GIT DIFF:\n{git_diff()}")
    if args.compiler_output:
        path = (ROOT / args.compiler_output).resolve()
        sections.append(f"COMPILER OUTPUT: {path.relative_to(ROOT)}\n{read_text(path)}")
    if args.context:
        sections.append(f"ADDITIONAL CONTEXT:\n{args.context}")
    return "\n\n".join(sections)


def call_ollama(model: str, system_prompt: str, user_prompt: str) -> str:
    body = json.dumps(
        {
            "model": model,
            "stream": False,
            "messages": [
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_prompt},
            ],
            "options": {"temperature": 0.2},
        }
    ).encode("utf-8")
    request = urllib.request.Request(
        OLLAMA_URL,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=600) as response:
            payload = json.load(response)
    except (urllib.error.URLError, TimeoutError) as exc:
        raise RuntimeError(f"Ollama request failed: {exc}") from exc
    return payload["message"]["content"]


def save_log(mode: str, model: str, prompt: str, response: str) -> pathlib.Path:
    timestamp = dt.datetime.now().astimezone().strftime("%Y%m%d-%H%M%S%z")
    path = ROOT / "ai" / "logs" / f"{timestamp}-{mode}.json"
    path.write_text(
        json.dumps(
            {"timestamp": timestamp, "mode": mode, "model": model, "prompt": prompt, "response": response},
            ensure_ascii=False,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("architecture", "review", "debug", "research"))
    parser.add_argument("--task", required=True, help="The single shared engineering task")
    parser.add_argument("--file", action="append", default=[], help="Project-relative context file; repeatable")
    parser.add_argument("--diff", action="store_true", help="Include the current tracked git diff")
    parser.add_argument("--compiler-output", help="Project-relative compiler log")
    parser.add_argument("--context", help="Additional facts, links or Codex proposal")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--no-log", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    system_prompt = read_text(ROOT / "ai" / "prompts" / "reviewer.md")
    prompt = build_prompt(args)
    response = call_ollama(args.model, system_prompt, prompt)
    print(response)
    if not args.no_log:
        log_path = save_log(args.mode, args.model, prompt, response)
        print(f"\n[review log: {log_path.relative_to(ROOT)}]", file=sys.stderr)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nReviewer interrupted.", file=sys.stderr)
        raise SystemExit(130)
    except (OSError, RuntimeError, ValueError, KeyError) as exc:
        print(f"reviewer error: {exc}", file=sys.stderr)
        raise SystemExit(1)
