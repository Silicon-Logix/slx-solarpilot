# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Install a pinned portable UCRT compiler inside build/, without system changes."""
from pathlib import Path
import hashlib
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NAME = "llvm-mingw-20261020-ucrt-x86_64"
SHA256 = "fd5462b7f5d93b2ff981fcd3f9c7e427e47f3203c64ea91c27bcc444eebd74cd"


def main():
    folder = ROOT / "build/tools"
    compiler = folder / NAME / "bin/clang++.exe"
    if compiler.exists():
        print(compiler.parent)
        return
    folder.mkdir(parents=True, exist_ok=True)
    archive = folder / (NAME + ".zip")
    if not archive.exists():
        url = f"https://github.com/mstorsjo/llvm-mingw/releases/download/20261020/{NAME}.zip"
        print("Downloading pinned UCRT toolchain (190 MB)", flush=True)
        with urllib.request.urlopen(url, timeout=60) as response, archive.open("wb") as out:
            while block := response.read(1024 * 1024):
                out.write(block)
    if hashlib.file_digest(archive.open("rb"), "sha256").hexdigest() != SHA256:
        raise RuntimeError("Toolchain SHA256 mismatch")
    with zipfile.ZipFile(archive) as source:
        for entry in source.infolist():
            if not (folder / entry.filename).resolve().is_relative_to(folder.resolve()):
                raise RuntimeError("Unsafe archive path")
        source.extractall(folder)
    print(compiler.parent)


if __name__ == "__main__":
    main()
