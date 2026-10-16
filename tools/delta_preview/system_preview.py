"""Capture native shared widgets. All inputs are HOST SIMULATION, not device evidence."""
from pathlib import Path
import ctypes
import hashlib
import json
import subprocess
import sys
import tempfile
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
OUT = ROOT / "assets/delta/system-preview"
SCENES = ("settings", "files-long", "files-empty", "dialog", "progress", "keyboard-masked",
          "wifi", "ble", "rfid", "rf-pulse-timing", "rf-square-wave", "terminal-style",
          "long-disabled-checked", "radio-empty", "storage-error", "keyboard-hex", "keyboard-numeric", "keyboard-shifted", "rf-rssi-history", "rf-frequency-bins")
THEMES = ("dark-sakura", "sakura-dawn", "monochrome")

def extracted_keyboard():
    source = (ROOT / "src/core/mykeyboard.cpp").read_text(encoding="utf-8")
    parts = []
    for name in ("qwerty", "hex", "numpad"):
        start = source.index("const int " + name + "_keyboard_width")
        end = source.index("\n};", start) + 3
        parts.append(source[start:end])
    start = source.index("template <int KeyboardHeight, int KeyboardWidth>\nstatic bool drawDeltaKeyboard(")
    end = source.index("\n#endif", start)
    parts.append(source[start:end])
    display = (ROOT / "src/core/display.cpp").read_text(encoding="utf-8")
    def block_at(text, start):
        opening = text.index("{", start)
        depth = 1
        end = opening + 1
        while depth:
            depth += (text[end] == "{") - (text[end] == "}")
            end += 1
        return text[start:end]
    start = display.index("int8_t displayMessage(")
    redraw = display.index("if (redraw)", start)
    parts.append("static void drawPreviewConfirmation(const char*message,const char*leftButton,const char*centerButton,const char*rightButton,int selected){\n"
                 "const int totalButtons=(leftButton?1:0)+(centerButton?1:0)+(rightButton?1:0);bool redraw=true;uint16_t*frame=canvas;\n"
                 + block_at(display, redraw) + "\n}")
    start = display.index("void displayRedStripe(")
    branch = display.index("if (auto *frame = deltaUiCanvas())", start)
    parts.append("static void drawPreviewStatus(const String&text,uint16_t bgcolor){const uint16_t TFT_RED=0xf800,TFT_YELLOW=0xffe0;\n"
                 + block_at(display, branch) + "\n}")
    return "\n".join(parts)

def load():
    zig = next((ROOT / ".tools/zig").rglob("zig.exe"), None)
    if not zig:
        raise SystemExit("Use the existing portable Zig compiler under .tools/zig")
    dll = HERE / "delta_system.dll"
    with tempfile.TemporaryDirectory(prefix="delta-system-preview-") as directory:
        include = Path(directory) / "delta_keyboard_preview.inc"
        include.write_text(extracted_keyboard(), encoding="utf-8")
        subprocess.run([str(zig), "c++", "-std=c++11", "-O3", "-shared", "-I" + directory, str(HERE / "system.cpp"), "-o", str(dll)], check=True, cwd=ROOT)
    lib = ctypes.CDLL(str(dll))
    lib.delta_system_frame.argtypes = [ctypes.POINTER(ctypes.c_uint16), ctypes.c_int, ctypes.c_int]
    lib.delta_system_benchmark.argtypes = [ctypes.c_int, ctypes.c_int]
    lib.delta_system_benchmark.restype = ctypes.c_double
    return lib

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    lib = load()
    font_path = Path("C:/Windows/Fonts/segoeui.ttf")
    font = ImageFont.truetype(str(font_path), 14) if font_path.exists() else ImageFont.load_default()
    images = {}
    evidence = {"data_provenance": "HOST SIMULATION / PREVIEW fixtures; not device screenshots or detected devices", "native_size": [320, 170], "keyboard": "production drawDeltaKeyboard template and keysets extracted verbatim", "confirmation_and_error": "production rendering blocks extracted verbatim", "cpu_timings": {}, "captures": []}
    sources = ["include/delta/widgets.h", "include/delta/instruments.h", "include/delta/rf_graph.h", "include/delta/core.h", "src/core/mykeyboard.cpp", "src/core/display.cpp", "src/core/delta_ui.cpp", "src/modules/rf/rf_spectrum.cpp", "src/core/tftLogger/tftLogger.cpp", "tools/delta_preview/system.cpp"]
    evidence["source_sha256"] = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in sources}
    for theme, theme_name in enumerate(THEMES):
        sheet = Image.new("RGB", (6 * 336, ((len(SCENES) + 5) // 6) * 231 + 58), (16, 16, 22))
        draw = ImageDraw.Draw(sheet)
        draw.text((12, 7), "HOST SIMULATION / PREVIEW FIXTURES - " + theme_name, fill=(244, 184, 207), font=font)
        draw.text((12, 29), "320x170 native C++ renders. No radio, NFC, storage or device activity.", fill=(190, 184, 197), font=font)
        for scene, scene_name in enumerate(SCENES):
            buf = (ctypes.c_uint16 * (320 * 170))()
            lib.delta_system_frame(buf, theme, scene)
            im = Image.frombytes("RGB", (320, 170), bytes(buf), "raw", "BGR;16")
            name = f"PREVIEW_{theme_name}_{scene_name}"
            native = OUT / (name + "_320x170.png")
            im.save(native)
            im.resize((1280, 680), Image.Resampling.NEAREST).save(OUT / (name + "_4x.png"))
            images[(theme, scene)] = im
            evidence["captures"].append({"file": native.name, "sha256": hashlib.sha256(native.read_bytes()).hexdigest(), "provenance": "HOST SIMULATION"})
            x, y = scene % 6 * 336 + 8, scene // 6 * 231 + 63
            sheet.paste(im, (x, y))
            draw.text((x, y + 174), scene_name, fill=(236, 225, 233), font=font)
            draw.text((x, y + 193), "HOST SIMULATION", fill=(175, 157, 177), font=font)
        sheet.save(OUT / f"PREVIEW_CONTACT_SHEET_{theme_name}.png")
    for scene, name in enumerate(SCENES):
        evidence["cpu_timings"][name] = {"mean_microseconds": round(lib.delta_system_benchmark(scene, 500), 3), "iterations": 500}
    evidence["timing_limit"] = "Host CPU rendering only. Excludes firmware, LCD transfer, image encoding. Not device FPS or latency."
    evidence["python"] = sys.version.split()[0]
    (OUT / "PREVIEW_manifest.json").write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    print("HOST SIMULATION captures:", OUT)
    print("Native images:", len(evidence["captures"]), "; matching 4x images:", len(evidence["captures"]), "; contact sheets: 3")

if __name__ == "__main__":
    main()
