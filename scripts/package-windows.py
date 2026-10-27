# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Export the Windows simulator and public SDK from the publication inventory."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import zipfile
from public_files import ROOT, public_files


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'dist/pc-public-candidate')
    args = parser.parse_args()
    output = args.output.resolve()
    archive = output.parent/(output.name+'.zip')
    sidecar = archive.parent/(archive.name+'.sha256')
    if not output.is_relative_to(ROOT/'dist') or any(path.exists() for path in [output, archive, sidecar]):
        raise RuntimeError('Choose a new directory under dist')
    subprocess.run(['python', str(ROOT/'scripts/audit-publication.py')], check=True)
    output.mkdir(parents=True)
    for name in public_files():
        target = output/name
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(ROOT/name,target)
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as stream:
        for name in public_files():
            stream.write(output/name,'SLX-SolarPilot-0.4.5-windows-x64/'+name)
    with zipfile.ZipFile(archive) as stream:
        if stream.testzip() is not None:
            raise RuntimeError('Archive verification failed')
    with archive.open('rb') as stream:
        checksum = hashlib.file_digest(stream,'sha256').hexdigest()
    sidecar.write_text(checksum+'  '+archive.name+'\n',encoding='ascii')
    print(json.dumps({'folder':str(output),'archive':str(archive),'sha256':checksum},indent=2))


if __name__ == '__main__':
    main()
