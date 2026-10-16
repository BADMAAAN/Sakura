# ADR 0001: Preserve Bruce history and track upstream directly

## Problem

FLUX must evolve as its own firmware while remaining practical to update from Bruce.

## Codex proposal

Use the cloned Bruce repository as the FLUX repository, rename the canonical remote to `upstream`, and develop on `flux/main` through small additive commits.

## Ollama criticism

Ollama challenged the direct fork and proposed nested source, submodule, and worktree/symlink isolation.

## Codex response

A nested copy or submodule makes ancestry, blame, bisect, and three-way merges less direct. Symlinks weaken Windows portability. Enforce boundaries with remotes, small commits, ADRs, and build checks.

## Alternative options

- Copy Bruce into a nested `source/` directory, losing useful ancestry at the FLUX repository boundary.
- Keep a separate pristine checkout, duplicating storage and making source-of-truth mistakes easier.
- Use a Git submodule, which complicates a fork whose core source must be modified.

## Final decision

Keep direct Bruce ancestry on `flux/main`, with `upstream` pointing at `BruceDevices/firmware`. Use temporary worktrees only for pristine comparisons.

## Why

Direct ancestry makes upstream fetch, comparison, merge and conflict attribution standard Git operations.

## Impact

FLUX-specific files live beside upstream files, while firmware changes must remain narrowly scoped and documented.
