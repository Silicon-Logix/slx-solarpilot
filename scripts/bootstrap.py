# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Recreate pinned public dependencies without any neighbouring SLX repository."""

from pathlib import Path
import argparse
import hashlib
import json
import shutil
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk-only", action="store_true")
    args = parser.parse_args()
    lock = json.loads((ROOT / "scripts/dependencies.lock.json").read_text())
    cache = ROOT / "build/downloads"
    cache.mkdir(parents=True, exist_ok=True)
    licenses = ROOT / "licenses"
    licenses.mkdir(exist_ok=True)
    for name, entry in lock.items():
        if args.sdk_only and name not in {"json.hpp", "json-LICENSE.MIT"}:
            continue
        archive = name in {"glfw", "imgui", "cgltf", "stb", "implot", "nng"}
        file = cache / (name + ".zip" if archive else name)
        if not file.exists():
            print("Download", name, flush=True)
            with urllib.request.urlopen(entry["url"], timeout=60) as response:
                file.write_bytes(response.read(80_000_000))
        if hashlib.sha256(file.read_bytes()).hexdigest() != entry["sha256"]:
            raise RuntimeError("Dependency SHA256 mismatch: " + name)
        if archive:
            destination = ROOT / "build/deps" / name
            with zipfile.ZipFile(file) as source:
                for info in source.infolist():
                    parts = Path(info.filename).parts[1:]
                    if info.is_dir() or not parts:
                        continue
                    target = destination.joinpath(*parts).resolve()
                    if not target.is_relative_to(destination.resolve()):
                        raise RuntimeError("Unsafe dependency archive path")
                    target.parent.mkdir(parents=True, exist_ok=True)
                    content = source.read(info)
                    if not target.exists() or target.read_bytes() != content:
                        target.write_bytes(content)
            notice = next(
                (
                    destination / n
                    for n in ("LICENSE.md", "LICENSE.txt", "LICENSE")
                    if (destination / n).exists()
                ),
                None,
            )
            if notice:
                shutil.copy2(notice, licenses / (name + "-LICENSE.txt"))
        else:
            target = (
                ROOT / "build/deps/include/nlohmann/json.hpp"
                if name == "json.hpp"
                else licenses / name
            )
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(file, target)
    print(
        "Pinned public SDK dependency ready."
        if args.sdk_only
        else "Pinned Windows desktop dependencies ready."
    )


if __name__ == "__main__":
    main()
