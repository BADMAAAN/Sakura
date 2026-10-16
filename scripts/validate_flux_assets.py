"""Validate the stable FLUX asset manifest without requiring artwork."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "assets" / "flux" / "manifest.example.json"
REQUIRED_IDS = {"branding.firmware_icon", "branding.splash", "branding.secondary"}


def validate(path: Path) -> list[str]:
    data = json.loads(path.read_text(encoding="utf-8"))
    errors = []
    if data.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    missing = REQUIRED_IDS - set(data.get("assets", {}))
    if missing:
        errors.append("missing logical asset IDs: " + ", ".join(sorted(missing)))
    for asset_id, asset in data.get("assets", {}).items():
        if asset.get("status") == "pending" and asset.get("source") is not None:
            errors.append(f"{asset_id}: pending assets must not claim a source")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", nargs="?", type=Path, default=DEFAULT)
    args = parser.parse_args()
    errors = validate(args.manifest)
    if errors:
        print("\n".join(errors))
        return 1
    print(f"FLUX asset manifest valid: {args.manifest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
