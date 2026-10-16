"""Prepare local FLUX artwork once at build time; no SD or runtime scaling.

The private directory is intentionally local-only (see .gitignore).  Artwork is
stored as display-sized JPEG data so each frame is contained in the complete
display canvas without cropping and still fits in the application partition.
"""
from io import BytesIO
from pathlib import Path
from PIL import Image, ImageOps

SOURCES = {
    "splash": "photo_2026-09-15_01-51-18.jpg",
    "secondary": "photo_2026-09-15_01-51-21.jpg",
    "logo": "photo_2026-09-15_01-51-23.jpg",
}

PRIVATE_DIR = "private"
PRIVATE_SPLASH = "private/home.jpg"  # kept for compatibility with older tests/tools
PRIVATE_BOOT_FRAME_COUNT = 5
MAIN_FALLBACKS = {
    "splash": "assets/flux/source/branding/flux.boot.source.png",
    "secondary": "assets/flux/source/branding/flux.secondary.source.png",
    "logo": "assets/flux/source/branding/flux.icon.source.png",
}
# Bruce's T-Embed target is a fixed 320x170 landscape panel. Keeping one
# orientation also keeps the factory image below Bruce's 0x470000 app slot.
TARGETS = (("screen", (320, 170)),)

# Each portrait has a different safe subject area. ImageOps.fit keeps the
# aspect ratio but uses this focal point when it must crop to 320x170.
PRIVATE_FOCAL = {
    "photo_2026-09-11_16-20-13.jpg": (0.50, 0.60),
    "photo_2026-09-15_04-03-46.jpg": (0.52, 0.42),
    "photo_2026-09-15_04-04-30.jpg": (0.52, 0.66),
    "photo_2026-09-15_04-05-18.jpg": (0.68, 0.34),
    "photo_2026-09-15_04-05-21.jpg": (0.68, 0.28),
}


def prepare(source, size, fill=False, centering=(0.5, 0.45)):
    with Image.open(source) as image:
        image = ImageOps.exif_transpose(image).convert("RGB")
        if fill:
            return ImageOps.fit(image, size, Image.Resampling.LANCZOS, centering=centering)
        fitted = ImageOps.contain(image, size, Image.Resampling.LANCZOS)
        canvas = Image.new("RGB", size, (0, 0, 0))
        canvas.paste(fitted, ((size[0] - fitted.width) // 2, (size[1] - fitted.height) // 2))
        return canvas


def resolve_main_source(root, role):
    candidate = root / "flux" / SOURCES[role]
    if candidate.exists():
        return candidate
    fallback = root / MAIN_FALLBACKS[role]
    if fallback.exists():
        return fallback
    available = sorted((root / "flux").glob("*"))
    if available:
        return available[0]
    raise FileNotFoundError(f"No artwork available for FLUX role: {role}")


def private_sources(root):
    folder = root / "flux" / PRIVATE_DIR
    if not folder.exists():
        # Public clones do not contain device-only artwork. In that case the
        # build must use the public FLUX fallback below instead of failing.
        return []
    return sorted(
        path for path in folder.iterdir()
        if path.is_file() and path.suffix.lower() in {".jpg", ".jpeg", ".png", ".webp"}
    )


def jpeg_bytes(image):
    data = BytesIO()
    image.save(data, format="JPEG", quality=90, subsampling=0, optimize=True)
    return data.getvalue()


def append_array(lines, name, data, width, height):
    lines.append(f"static const uint8_t {name}[] PROGMEM = {{")
    lines.extend(",".join(f"0x{value:02x}" for value in data[i:i + 128]) + ","
                 for i in range(0, len(data), 128))
    lines.append("};")
    lines.append(f"static constexpr uint32_t {name}_data_size = {len(data)};")
    lines.append(f"static constexpr uint16_t {name}_width = {width};")
    lines.append(f"static constexpr uint16_t {name}_height = {height};")
    lines.append(f"static constexpr uint32_t {name}_pixel_count = {width * height};")
    lines.append(f"static_assert(sizeof({name}) == {name}_data_size, \"{name} data size mismatch\");")
    lines.append(
        f"static_assert({name}_pixel_count == static_cast<uint32_t>({name}_width) * {name}_height, "
        f"\"{name} pixel metadata mismatch\");"
    )


def generate(root, output):
    lines = [
        "// Generated locally from FLUX artwork by embed_flux_branding.py",
        "#pragma once",
        "struct FluxImageAsset {",
        "    const uint8_t *pixels;",
        "    uint32_t data_size;",
        "    uint16_t width;",
        "    uint16_t height;",
        "    uint32_t pixel_count;",
        "};",
    ]
    private = private_sources(root)
    if private:
        # Keep the established five-frame boot sequence when a smaller local
        # photo set is supplied. Reuse sources in their existing sorted order
        # instead of creating duplicate private files on disk.
        supplied = tuple(private)
        while len(private) < PRIVATE_BOOT_FRAME_COUNT:
            private.append(supplied[len(private) % len(supplied)])
    else:
        private = [resolve_main_source(root, "splash")]
    lines.append(f"static constexpr uint8_t FLUX_PRIVATE_IMAGE_COUNT = {len(private)};")
    for index, source in enumerate(private):
        for orientation, size in TARGETS:
            name = f"flux_private_{index}_{orientation}"
            focal = PRIVATE_FOCAL.get(source.name, (0.5, 0.45))
            append_array(lines, name, jpeg_bytes(prepare(source, size, fill=True, centering=focal)), *size)
    lines.append("static constexpr FluxImageAsset flux_private_screen[] = {")
    lines.extend(
        f"    {{flux_private_{index}_screen, flux_private_{index}_screen_data_size, flux_private_{index}_screen_width, "
        f"flux_private_{index}_screen_height, flux_private_{index}_screen_pixel_count}},"
        for index in range(len(private))
    )
    lines.append("};")
    output.parent.mkdir(parents=True, exist_ok=True)
    content = "\n".join(lines) + "\n"
    if not output.exists() or output.read_text() != content:
        output.write_text(content)


if __name__ == "__main__":
    generate(Path(__file__).resolve().parents[1], Path(".pio/flux_branding/flux_branding_data.h"))
elif "Import" in globals():
    Import("env")
    root = Path(env.subst("$PROJECT_DIR"))
    output = Path(env.subst("$BUILD_DIR")) / "generated" / "flux_branding_data.h"
    generate(root, output)
    env.Append(CPPPATH=[str(output.parent)])
