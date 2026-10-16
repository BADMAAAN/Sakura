"""Render production C++ at 320x170. All radio inputs are explicit HOST FIXTURES."""
from pathlib import Path
import hashlib
import json
import subprocess
import tempfile
import time
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
OUT = ROOT / 'assets/delta/cyber-preview'
BASELINE = '4003f2cb'
SCENES = ('home', 'wifi-main', 'wifi-results', 'ble-main', 'ble-spam-list', 'ble-spam-config',
          'rf-main', 'rf-analyzer', 'settings', 'delta-appearance', 'files', 'scripts',
          'wifi-empty', 'wifi-unmeasured', 'rf-unmeasured')
THEMES = ('dark-sakura', 'sakura-dawn', 'monochrome')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
    font = ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf', 14)
    sources = [HERE / 'cyber.cpp', HERE / 'cyber_preview.py'] + sorted((ROOT / 'include/delta').glob('*.h'))
    source_hashes = {path.relative_to(ROOT).as_posix(): sha(path) for path in sources}
    baseline = subprocess.check_output(['git', 'rev-parse', BASELINE], cwd=ROOT, text=True).strip()
    manifest = {
        'provenance': 'HOST FIXTURES / exact shared production C++ pixels, not physical screenshots or detected signals',
        'baseline_commit': baseline, 'native_size': [320, 170], 'scenes': list(SCENES),
        'current_source_sha256': source_hashes, 'baseline_source_sha256': {}, 'capture_sha256': {},
        'missing': [], 'host_cpu_microseconds': {},
        'fixtures': 'One cyber.cpp supplies both revisions. FIXTURE_AP names and 02: addresses are synthetic. RF bins/RSSI are layout fixtures. No handlers, radio, network, storage or serial operations execute.',
        'home': 'Identical fixed timestamp 4260 ms and default 14 petals; unknown battery and --:-- time. Native artwork renderer, no resampled wallpaper.',
        'timing_limit': 'Host CPU render only. Excludes LCD transfers, input polling and firmware work; these timings are not LilyGO FPS or response latency.'}
    images = {}
    started = time.perf_counter()
    with tempfile.TemporaryDirectory(prefix='delta-cyber-preview-') as directory:
        temporary = Path(directory)
        baseline_files = subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', baseline, 'include/delta'], cwd=ROOT, text=True).splitlines()
        for name in baseline_files:
            if not name.endswith('.h'):
                continue
            data = subprocess.check_output(['git', 'show', f'{baseline}:{name}'], cwd=ROOT)
            target = temporary / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            manifest['baseline_source_sha256'][name] = hashlib.sha256(data).hexdigest()
        for revision in ('before', 'after'):
            executable = temporary / (revision + '.exe')
            command = [str(zig), 'c++', '-std=c++11', '-O3', '-UNDEBUG']
            if revision == 'before':
                command += ['-DDELTA_CYBER_BASELINE', '-I', str(temporary / 'include')]
            else:
                command += ['-I', str(ROOT / 'include')]
            command += [str(HERE / 'cyber.cpp'), '-o', str(executable)]
            subprocess.run(command, check=True, cwd=ROOT)
            for theme, theme_name in enumerate(THEMES):
                for scene, scene_name in enumerate(SCENES):
                    ppm = temporary / 'capture.ppm'
                    subprocess.run([str(executable), str(theme), str(scene), str(ppm)], check=True, cwd=ROOT)
                    picture = Image.open(ppm).convert('RGB')
                    images[(revision, theme, scene)] = picture
                    name = f'HOSTFIXTURE_{revision}_{theme_name}_{scene_name}'
                    picture.save(OUT / (name + '_320x170.png'))
                    picture.resize((1280, 680), Image.Resampling.NEAREST).save(OUT / (name + '_4x.png'))
            for scene, scene_name in enumerate(SCENES):
                output = subprocess.check_output([str(executable), '0', str(scene), '-benchmark', '100'], cwd=ROOT, text=True)
                manifest['host_cpu_microseconds'][f'{revision}/{scene_name}'] = float(output.strip())
        for theme, theme_name in enumerate(THEMES):
            contact = Image.new('RGB', (1000, 5 * 205 + 51), (9, 10, 13))
            draw = ImageDraw.Draw(contact)
            draw.text((10, 6), 'HOST FIXTURES / SHARED FIRMWARE C++ / AFTER / ' + theme_name, font=font, fill=(239, 170, 200))
            draw.text((10, 25), 'Native 320 x 170; synthetic radio inputs; not device photographs or measured signals.', font=font, fill=(177, 153, 169))
            for scene, scene_name in enumerate(SCENES):
                x, y = 8 + scene % 3 * 332, 51 + scene // 3 * 205
                contact.paste(images[('after', theme, scene)], (x, y))
                draw.text((x, y + 174), scene_name + ' / HOST FIXTURE', font=font, fill=(235, 226, 235))
            contact.save(OUT / f'HOSTFIXTURE_CONTACT_{theme_name}.png')
            comparison = Image.new('RGB', (672, 12 * 207 + 51), (9, 10, 13))
            draw = ImageDraw.Draw(comparison)
            draw.text((10, 6), 'HOST FIXTURES / NATIVE 320 x 170 / ' + theme_name, font=font, fill=(239, 170, 200))
            draw.text((10, 26), 'BEFORE ' + BASELINE, font=font, fill=(177, 153, 169))
            draw.text((346, 26), 'AFTER / CYBER MODE UI', font=font, fill=(238, 225, 235))
            for scene in range(12):
                y = 51 + scene * 207
                for column, revision in enumerate(('before', 'after')):
                    comparison.paste(images[(revision, theme, scene)], (8 + column * 336, y))
                    draw.text((10 + column * 336, y + 174), SCENES[scene] + ' / HOST FIXTURE', font=font, fill=(235, 226, 235))
            comparison.save(OUT / f'HOSTFIXTURE_BEFORE_AFTER_{theme_name}.png')
        # Compact four-screen panels keep the complete native comparisons easy to open.
        for group in range(3):
            panel = Image.new('RGB', (672, 4 * 207 + 51), (9, 10, 13))
            draw = ImageDraw.Draw(panel)
            draw.text((10, 6), 'HOST FIXTURES / BEFORE ' + BASELINE + ' / AFTER CYBER UI', font=font, fill=(239, 170, 200))
            draw.text((10, 26), 'Native 320 x 170 / dark-sakura / synthetic inputs only', font=font, fill=(177, 153, 169))
            for row in range(4):
                scene = group * 4 + row
                for column, revision in enumerate(('before', 'after')):
                    x, y = 8 + column * 336, 51 + row * 207
                    panel.paste(images[(revision, 0, scene)], (x, y))
                    draw.text((x, y + 174), SCENES[scene] + ' / ' + revision, font=font, fill=(235, 226, 235))
            panel.save(OUT / f'HOSTFIXTURE_BEFORE_AFTER_dark-sakura_part{group+1}.png')
    manifest['capture_sha256'] = {path.name: sha(path) for path in sorted(OUT.glob('*.png'))}
    changed = [name for name, digest in source_hashes.items() if sha(ROOT / name) != digest]
    manifest['sources_changed_during_capture'] = changed
    manifest['generation_seconds'] = round(time.perf_counter() - started, 3)
    (OUT / 'HOSTFIXTURE_manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    # Validate the deliverable that was actually written, including enlarged PNGs and sheets.
    for name, digest in manifest['capture_sha256'].items():
        if sha(OUT / name) != digest:
            raise SystemExit('Capture hash mismatch: ' + name)
    print(f'Captured {len(images)} native frames; PNGs={len(manifest["capture_sha256"])}; changed_sources={changed}; output={OUT}')


if __name__ == '__main__':
    main()
