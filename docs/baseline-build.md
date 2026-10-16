# Bruce baseline build — LILYGO T-Embed CC1101 Plus

## Source and target

- Commit: `ba519c936c87b89c9c667c15b932d7faca5360d5`
- Branch: `flux/main`
- Environment: `lilygo-t-embed-cc1101`
- Target: `build-firmware`

## Results

| Run | Result | Duration | RAM | Flash | Factory image |
| --- | --- | ---: | ---: | ---: | ---: |
| Initial complete build | SUCCESS | 148.84 s | 130,784 / 327,680 B | 4,200,098 / 16,777,216 B | 4,266,176 B |
| Clean + repeat build | SUCCESS | 58.74 s | 130,784 / 327,680 B | 4,200,098 / 16,777,216 B | 4,266,176 B |
| Cold build (target + object cache removed) | SUCCESS | 320.13 s | 130,784 / 327,680 B | 4,200,098 / 16,777,216 B | 4,266,176 B |

The first two cache-assisted runs produced SHA-256 `07e3c6f1f9f8d91315abb6e3d2db930c49a742a4d9e8e72c0d5305e24fd38808`. The cold build produced `32ac4b2f710a82677714039d715e0abe60a91ac9ca0526c3ab37c0019bc696db`. Thus the source/toolchain build is repeatable in success, size, and memory metrics, but is not currently byte-for-byte deterministic across cache state.

## Dependency incident

After three `tool-scons` failures: default API HTTP 502, nm1 timeout, empty `HTTPClientError`. Official ns1 endpoints were healthy. Official SCons 4.8.1 matched SHA-256 `289b1abf389b730ab5a1018ed9fc84e22d301e0d16c64548eb9db5bf8ba59d06`; installed metadata is `tool-scons @ 4.40801.0`.

## Baseline warnings

- overflow in `karma_attack.cpp` (`180000` to `uint16_t`);
- always-false unsigned comparison in `config.cpp`;
- format mismatch in `ram_profile.cpp`;
- vendor LTO ODR, narrowing, deprecation, and string-operation warnings.

These predate FLUX changes. Repeat log: `ai/context/baseline-build-clean.log`.

The cold log is `ai/context/baseline-build-cold.log` (180 warning lines, zero error lines). Two wrapper-only failures (`PATH` truncation, then Windows PowerShell treating compiler stderr as terminating) were fixed before the successful cold build. PlatformIO exit codes remain authoritative.
