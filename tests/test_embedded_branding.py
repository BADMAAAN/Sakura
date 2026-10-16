import sys
import tempfile
import unittest
from pathlib import Path
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from embed_flux_branding import TARGETS, generate, jpeg_bytes, prepare, private_sources


class EmbeddedBrandingTests(unittest.TestCase):
    def test_private_artwork_fills_screen_without_stretch(self):
        for source in private_sources(ROOT):
            for _, size in TARGETS:
                with self.subTest(source=source.name, screen=size):
                    frame = prepare(source, size, fill=True)
                    with Image.open(source) as original:
                        original = ImageOps.exif_transpose(original).convert("RGB")
                        reference = ImageOps.fit(original, size, Image.Resampling.LANCZOS, centering=(0.5, 0.45))
                    self.assertEqual(frame.size, size)
                    self.assertEqual(frame.tobytes(), reference.tobytes())

    def test_generated_pack_fits_existing_application_headroom_and_is_reproducible(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "pack.h"
            fixture_root = Path(folder) / "fixture"
            private = fixture_root / "flux" / "private"
            private.mkdir(parents=True)
            Image.new("RGB", (480, 640), (64, 32, 48)).save(private / "fixture.png")
            generate(fixture_root, output)
            content = output.read_bytes()
            timestamp = output.stat().st_mtime_ns
            generate(fixture_root, output)
            self.assertEqual(output.read_bytes(), content)
            self.assertEqual(output.stat().st_mtime_ns, timestamp)
            self.assertEqual(content.count(b"PROGMEM"), 5)
            self.assertEqual(content.count(b"static_assert"), 10)
            self.assertIn(b"FLUX_PRIVATE_IMAGE_COUNT = 5", content)
            self.assertLess(len(content), 5 * 320 * 170 * 2)

    def test_public_clone_builds_without_personal_artwork(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / "public.h"
            generate(ROOT, output)
            content = output.read_text()
            self.assertEqual(content.count("PROGMEM"), 1)
            self.assertIn("FLUX_PRIVATE_IMAGE_COUNT = 1", content)

    def test_every_private_asset_has_exact_jpeg_dimensions_and_metadata(self):
        for source in private_sources(ROOT):
            for _, size in TARGETS:
                with self.subTest(source=source.name, screen=size):
                    frame = prepare(source, size, fill=True)
                    self.assertEqual(frame.size, size)
                    encoded = jpeg_bytes(frame)
                    with Image.open(__import__("io").BytesIO(encoded)) as decoded:
                        self.assertEqual(decoded.size, size)
                    self.assertGreater(len(encoded), 0)
                    self.assertLess(len(encoded), size[0] * size[1] * 2)


if __name__ == "__main__":
    unittest.main()
