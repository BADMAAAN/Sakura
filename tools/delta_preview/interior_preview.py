"""Capture exact shared firmware renderers; every displayed radio value is a HOST FIXTURE."""
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile
import time
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / "assets/delta/interior-preview"
BASELINE = "e4ba2cd9"
SCENES = ("wifi-main", "wifi-results", "ble-spam-list", "ble-spam-config", "rf-spectrum", "delta-appearance",
          "wifi-back-action", "wifi-empty", "rf-unmeasured", "wifi-utf8-long")
THEMES = ("dark-sakura", "sakura-dawn", "monochrome")
HEADERS = ("core.h", "artwork.h", "widgets.h", "instruments.h", "rf_graph.h")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    zig = next((ROOT / ".tools/zig").rglob("zig.exe"), None)
    if not zig:
        raise SystemExit("Existing .tools/zig compiler required")
    font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 14)
    manifest = {"provenance": "HOST FIXTURES: exact shared C++ pixels; not physical screenshots, detected devices or measured RF samples",
        "baseline_commit": subprocess.check_output(["git", "rev-parse", BASELINE], cwd=ROOT, text=True).strip(),
        "native_size": [320, 170], "capture_sha256": {}, "baseline_source_sha256": {}, "current_source_sha256": {},
        "missing": [], "host_cpu_microseconds": {}, "fixtures": "One identical interior.cpp supplies both revisions. MACs use locally administered 02: prefixes. No radio handlers execute.",
        "timing_limit": "Host CPU render only, excludes LCD transfer and firmware; not device FPS/latency."}
    sources = ["tools/delta_preview/interior.cpp", "include/delta/section_ui.h", "include/delta/ble_surface.h", "include/delta/typography.h", "include/delta/typography_data.h"]
    sources += ["include/delta/" + header for header in HEADERS]
    manifest["current_source_sha256"] = {name: sha(ROOT / name) for name in sources if (ROOT / name).exists()}
    legacy = HERE / "ble_before.h"
    manifest["ble_before"] = "Exact old BLE painter extraction with old TFT font" if legacy.exists() else "Unavailable; missing captures are explicitly recorded"
    if legacy.exists():
        manifest["current_source_sha256"]["tools/delta_preview/ble_before.h"] = sha(legacy)
        manifest["current_source_sha256"]["lib/TFT_eSPI/Fonts/glcdfont.c"] = sha(ROOT / "lib/TFT_eSPI/Fonts/glcdfont.c")
        old_ble = subprocess.check_output(["git", "show", f"{BASELINE}:src/modules/ble/ble_spam.cpp"], cwd=ROOT)
        manifest["baseline_source_sha256"]["src/modules/ble/ble_spam.cpp"] = hashlib.sha256(old_ble).hexdigest()
    started = time.perf_counter()
    images = {}
    with tempfile.TemporaryDirectory(prefix="delta-interior-preview-") as directory:
        temporary = Path(directory)
        baseline_include = temporary / "include/delta"
        baseline_include.mkdir(parents=True)
        for header in HEADERS:
            data = subprocess.check_output(["git", "show", f"{BASELINE}:include/delta/{header}"], cwd=ROOT)
            (baseline_include / header).write_bytes(data)
            manifest["baseline_source_sha256"]["include/delta/" + header] = hashlib.sha256(data).hexdigest()
        for revision in ("before", "after"):
            executable = temporary / (revision + ".exe")
            command = [str(zig), "c++", "-std=c++11", "-O3", "-UNDEBUG"]
            if revision == "before":
                command += ["-DDELTA_INTERIOR_BASELINE", "-I", str(temporary / "include")]
                if legacy.exists():
                    command += ["-DDELTA_BLE_BEFORE_AVAILABLE"]
            else:
                command += ["-I", str(ROOT / "include")]
            command += [str(HERE / "interior.cpp"), "-o", str(executable)]
            subprocess.run(command, check=True, cwd=ROOT)
            for theme, theme_name in enumerate(THEMES):
                for scene, scene_name in enumerate(SCENES):
                    # The old BLE painter predates DELTA theme support; avoid suggesting themed old output.
                    if revision == "before" and theme and scene in (2, 3):
                        continue
                    ppm = temporary / "capture.ppm"
                    result = subprocess.run([str(executable), str(theme), str(scene), str(ppm)], cwd=ROOT)
                    if result.returncode == 2:
                        manifest["missing"].append(f"{revision}/{theme_name}/{scene_name}")
                        continue
                    result.check_returncode()
                    image = Image.open(ppm).convert("RGB")
                    images[(revision, theme, scene)] = image
                    name = f"HOSTFIXTURE_{revision}_{theme_name}_{scene_name}"
                    native = OUT / (name + "_320x170.png")
                    large = OUT / (name + "_4x.png")
                    image.save(native)
                    image.resize((1280, 680), Image.Resampling.NEAREST).save(large)
                    manifest["capture_sha256"][native.name] = sha(native)
                    manifest["capture_sha256"][large.name] = sha(large)
            for scene, scene_name in enumerate(SCENES):
                result = subprocess.run([str(executable), "0", str(scene), "-benchmark", "100"], capture_output=True, text=True, cwd=ROOT)
                if not result.returncode:
                    manifest["host_cpu_microseconds"][f"{revision}/{scene_name}"] = float(result.stdout.strip())
        # Six requested scenes, side by side, at native resolution. Captions are outside the framebuffer.
        sheet = Image.new("RGB", (672, 6 * 215 + 48), (15, 16, 22))
        draw = ImageDraw.Draw(sheet)
        draw.text((10, 6), "HOST FIXTURES / SHARED FIRMWARE C++ / 320 x 170", font=font, fill=(238, 175, 207))
        draw.text((10, 25), "BEFORE e4ba2cd9", font=font, fill=(175, 159, 181))
        draw.text((346, 25), "AFTER / native AA interiors", font=font, fill=(238, 225, 235))
        for scene in range(6):
            y = 51 + scene * 215
            for column, revision in enumerate(("before", "after")):
                image = images.get((revision, 0, scene))
                if image:
                    sheet.paste(image, (8 + column * 336, y))
                else:
                    draw.text((15 + column * 336, y + 70), "BEFORE SOURCE NOT AVAILABLE", font=font, fill=(210, 180, 180))
                draw.text((10 + column * 336, y + 175), SCENES[scene], font=font, fill=(235, 226, 235))
                draw.text((10 + column * 336, y + 193), "HOST FIXTURE", font=font, fill=(165, 151, 174))
        sheet_path = OUT / "HOSTFIXTURE_BEFORE_AFTER_dark-sakura.png"
        sheet.save(sheet_path)
        sheet.resize((sheet.width * 4, sheet.height * 4), Image.Resampling.NEAREST).save(OUT / "HOSTFIXTURE_BEFORE_AFTER_dark-sakura_4x.png")
        manifest["capture_sha256"][sheet_path.name] = sha(sheet_path)
        for theme, theme_name in enumerate(THEMES):
            contact = Image.new("RGB", (1000, 4 * 208 + 48), (15, 16, 22))
            draw = ImageDraw.Draw(contact)
            draw.text((10, 7), "HOST FIXTURES / AFTER / " + theme_name, font=font, fill=(238, 175, 207))
            draw.text((10, 26), "Synthetic inputs, exact shared renderer. Unsupported Japanese/Arabic/Hebrew glyphs use '?'.", font=font, fill=(175, 159, 181))
            for scene, scene_name in enumerate(SCENES):
                x, y = 8 + scene % 3 * 332, 51 + scene // 3 * 208
                contact.paste(images[("after", theme, scene)], (x, y))
                draw.text((x, y + 174), scene_name + " / HOST FIXTURE", font=font, fill=(235, 226, 235))
            path = OUT / f"HOSTFIXTURE_CONTACT_{theme_name}.png"
            contact.save(path)
            manifest["capture_sha256"][path.name] = sha(path)
    manifest["generation_seconds"] = round(time.perf_counter() - started, 3)
    manifest["capture_sha256"] = {path.name: sha(path) for path in sorted(OUT.glob("*.png"))}
    (OUT / "HOSTFIXTURE_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Captured {len(images)} exact native frames; missing={len(manifest['missing'])}; output={OUT}")


if __name__ == "__main__":
    main()
