import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import validate_flux_assets


class FluxAssetContractTests(unittest.TestCase):
    def test_example_manifest_is_valid(self):
        self.assertEqual(validate_flux_assets.validate(validate_flux_assets.DEFAULT), [])

    def test_all_v01_branding_assets_are_ready_and_packaged(self):
        manifest = json.loads(validate_flux_assets.DEFAULT.read_text(encoding="utf-8"))
        expected = {"flux.icon.png", "flux.splash.png", "flux.secondary.png"}
        self.assertTrue(all(asset["status"] == "ready" for asset in manifest["assets"].values()))
        self.assertEqual(
            {Path(asset["target"]).name for asset in manifest["assets"].values()}, expected
        )
        for filename in expected:
            self.assertTrue((ROOT / "sd_files/flux/assets/branding" / filename).is_file())

    def test_source_replacement_directories_exist(self):
        source = ROOT / "assets" / "flux" / "source"
        for relative in ("branding/icon", "branding/splash"):
            self.assertTrue((source / relative).is_dir())

    def test_runtime_binds_all_assets_with_sd_then_littlefs_fallback(self):
        main = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
        utils = (ROOT / "src/core/utils.cpp").read_text(encoding="utf-8")
        runtime = main + utils
        for filename in ("flux.icon.png", "flux.splash.png", "flux.secondary.png"):
            self.assertIn(filename, runtime)
        self.assertLess(utils.index("sdcardMounted && SD.exists(path)"),
                        utils.index("LittleFS.exists(path)"))
        self.assertNotIn("Desktop", runtime)


if __name__ == "__main__":
    unittest.main()
