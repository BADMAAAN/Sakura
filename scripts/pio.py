#!/usr/bin/env python3
"""Run PlatformIO through the currently healthy official registry endpoint."""

import platformio


# On 2026-08-26 the default API returned HTTP 502 and the nm1 fallback timed
# out, while the official ns1 endpoint returned valid v3 package metadata.
# Keep this process-local; do not alter DNS, hosts files, TLS, or global config.
platformio.__registry_mirror_hosts__[:] = ["registry.ns1.platformio.org"]

from platformio.registry import mirror  # noqa: E402


_original_mirror_init = mirror.RegistryFileMirrorIterator.__init__


def _prefer_healthy_official_download_mirror(self, download_url):
    download_url = download_url.replace(
        "https://dl.registry.platformio.org/",
        "https://dl.registry.ns1.platformio.org/",
        1,
    )
    _original_mirror_init(self, download_url)


mirror.RegistryFileMirrorIterator.__init__ = _prefer_healthy_official_download_mirror

from platformio.__main__ import main  # noqa: E402  (endpoint must be set first)


if __name__ == "__main__":
    raise SystemExit(main())
