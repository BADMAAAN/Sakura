# ADR 0002: Process-local official PlatformIO mirror workaround

## Problem

On 2026-08-26 PlatformIO Core 6.1.19 could not install `tool-scons`: the default API returned HTTP 502 and default/nm1 paths timed out. Three attempts failed with `HTTPClientError`.

## Codex proposal

Use official PlatformIO/pioarduino artifacts only, verify metadata and hashes, and redirect mirrors only inside the build process.

## Ollama criticism

Ollama requested version pinning, provenance, hash verification, and reproducibility. It suggested Python 3.14 as a possible cause; direct HTTP evidence and successful Core execution disproved that.

## Codex response

The official pioarduino descriptor references SCons 4.8.1 with SHA-256 `289b1abf389b730ab5a1018ed9fc84e22d301e0d16c64548eb9db5bf8ba59d06`. The artifact matched. Installed metadata is `tool-scons @ 4.40801.0`; reinstall is unnecessary.

## Alternative options

- Wait for the default registry.
- Change machine-wide networking.
- Use an unofficial archive.

## Final decision

Use `scripts/pio.py` to select working official ns1 endpoints process-locally and `scripts/build.ps1` as the guarded entry point. Retire it when defaults recover.

## Why

This preserves official provenance without machine-wide changes.

## Impact

Builds currently use the wrapper; failure, version/hash, and removal condition are auditable.
