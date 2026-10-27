# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Prepare the optional protocol toolchain when licensed sources are available.

The public SDK uses the supplied native bridge.
"""
from pathlib import Path
import hashlib
import subprocess
import urllib.request
import zipfile

ROOT = Path(__file__).resolve().parents[1]
VERSION = 'go1.27.3'
SHA256 = 'f26081f438319757f8a1b46d7d0b2c5ec7583d4038e485e0aa725cfde7885fff'


def main():
    if not (ROOT/'private/eebus/go.mod').exists():
        print('Public build: supplied EEBUS binary')
        return
    folder = ROOT/'build/tools'
    if (folder/'go/bin/go.exe').exists():
        version=subprocess.check_output([str(folder/'go/bin/go.exe'),'version'],text=True)
        if VERSION not in version:
            raise RuntimeError('Unexpected Go compiler in build/tools/go')
        return
    folder.mkdir(parents=True, exist_ok=True)
    archive = folder/(VERSION+'.windows-amd64.zip')
    if not archive.exists():
        with urllib.request.urlopen('https://go.dev/dl/'+archive.name, timeout=60) as source, archive.open('wb') as out:
            while block := source.read(1024*1024):
                out.write(block)
    with archive.open('rb') as source:
        if hashlib.file_digest(source,'sha256').hexdigest()!=SHA256:
            raise RuntimeError('Go archive SHA256 mismatch')
    with zipfile.ZipFile(archive) as source:
        for entry in source.infolist():
            if not (folder/entry.filename).resolve().is_relative_to(folder.resolve()):
                raise RuntimeError('Unsafe Go archive path')
        source.extractall(folder)


if __name__ == '__main__':
    main()
