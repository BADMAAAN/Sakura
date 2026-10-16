from __future__ import annotations

import pathlib
import re
import unittest
import ast
import glob
import gzip
import hashlib
import os
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]


def source(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


class FirmwareIdentityTests(unittest.TestCase):
    def test_sakura_identity_preserves_flux_compatibility(self) -> None:
        header = source("include/firmware_identity.h")
        self.assertIn("defined(FLUX_BUILD)", header)
        self.assertIn('#define FIRMWARE_NAME "FLUX"', header)
        self.assertIn("#define FIRMWARE_VERSION FLUX_VERSION", header)
        self.assertEqual(header.count('#define FIRMWARE_NAME "FLUX"'), 2)
        self.assertEqual(header.count("#define FIRMWARE_VERSION FLUX_VERSION"), 3)
        self.assertIn('defined(SAKURA_BUILD)', header)
        self.assertIn('#define FIRMWARE_NAME "SAKURA"', header)
        self.assertLess(header.index('defined(SAKURA_BUILD)'), header.index('defined(FLUX_BUILD)'))
        self.assertIn('-DSAKURA_BUILD=1', source('boards/flux-delta.ini'))

    def test_flux_environment_extends_pinned_board_environment(self) -> None:
        config = source("boards/lilygo-t-embed-cc1101/flux-t-embed-cc1101.ini")
        self.assertIn("[env:flux-t-embed-cc1101]", config)
        self.assertIn("extends = env:lilygo-t-embed-cc1101", config)
        self.assertIn("-DFLUX_BUILD=1", config)
        self.assertRegex(config, r"-DFLUX_VERSION=.*0\.1\.0-dev")
        self.assertIn("custom_firmware_name = FLUX", config)


class TargetPinTests(unittest.TestCase):
    def test_all_t_embed_spi_aliases_match_their_macros(self) -> None:
        pins = source("boards/lilygo-t-embed-cc1101/pins_arduino.h")
        self.assertEqual(pins.count("static const uint8_t SCK = SPI_SCK_PIN;"), 2)
        self.assertEqual(pins.count("static const uint8_t MISO = SPI_MISO_PIN;"), 2)
        self.assertNotIn("static const uint8_t SCK = SPI_MISO_PIN;", pins)
        self.assertNotIn("static const uint8_t MISO = SPI_SCK_PIN;", pins)


class BuildSafetyTests(unittest.TestCase):
    def test_web_resources_generate_locally_and_roundtrip(self) -> None:
        tree = ast.parse(source("patch.py"))
        functions = [node for node in tree.body if isinstance(node, ast.FunctionDef)]
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            web = root / "embedded_resources/web_interface"
            web.mkdir(parents=True)
            payload = b"/* Preserve source bytes */\nconst text = '<hello>';\n"
            (web / "index.js").write_bytes(payload)
            namespace = dict(vars(os.path), glob=glob, gzip=gzip, hashlib=hashlib,
                             makedirs=os.makedirs, remove=os.remove,
                             env={"PROJECT_DIR": folder})
            exec(compile(ast.Module(body=functions, type_ignores=[]), "patch.py", "exec"), namespace)
            # Even a matching checksum must regenerate a missing ignored header.
            namespace["save_checksum_file"](
                namespace["hash_files"]([str(web / "index.js")]), str(web / "checksum.sha256")
            )
            namespace["prepare_www_files"]()
            header = root / "include/webFiles.h"
            generated = header.read_text()
            packed = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-F]{2})", generated))
            self.assertEqual(gzip.decompress(packed), payload)
            initial_mtime = header.stat().st_mtime_ns
            namespace["prepare_www_files"]()
            self.assertEqual(header.stat().st_mtime_ns, initial_mtime)

    def test_wrapper_restricts_environment_and_guards_concurrency(self) -> None:
        wrapper = source("scripts/build.ps1")
        validate_set = re.search(r"ValidateSet\(([^)]*)\)", wrapper)
        self.assertIsNotNone(validate_set)
        self.assertEqual(
            set(re.findall(r"'([^']+)'", validate_set.group(1))),
            {"flux-delta", "flux-t-embed-cc1101", "lilygo-t-embed-cc1101"},
        )
        self.assertIn("Get-CimInstance Win32_Process", wrapper)
        self.assertIn("Another PlatformIO process is active", wrapper)

    def test_regression_artifact_name_is_flux_namespaced(self) -> None:
        merge_script = source("build.py")
        self.assertIn('GetProjectOption("custom_firmware_name", default="FLUX-Upstream-Regression")', merge_script)
        self.assertIn('f"{firmware_name}-{pioenv}.bin"', merge_script)


if __name__ == "__main__":
    unittest.main()
