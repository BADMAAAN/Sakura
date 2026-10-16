# ADR 0003: Additive FLUX firmware identity

## Problem

FLUX needs visible identity without a broad rename that makes Bruce merges expensive or breaks compatibility.

## Codex proposal

Add a target extending the Bruce board environment, a small identity header, and optional artifact naming. Change only status bar and Device Info initially.

## Ollama criticism

Ollama accepted the seam and requested explicit version/build documentation and verification of both targets.

## Codex response

Commands are documented in BUILD.md. Bruce paths, config names, dependencies, classes, Wi-Fi defaults, and file formats remain unchanged.

## Alternative options

- Replace every Bruce string and identifier.
- Rename storage paths and schema immediately.
- Modify the upstream board environment in place.

## Final decision

Use `flux-t-embed-cc1101`, `FLUX_BUILD`, `FLUX_VERSION`, and `firmware_identity.h`. Preserve the upstream environment and compatibility-sensitive internals.

## Why

This makes FLUX visible while retaining a small, merge-friendly delta.

## Impact

FLUX produces its own named artifact. Version updates are centralized in the FLUX environment.
