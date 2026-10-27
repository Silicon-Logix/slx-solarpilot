# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Verify public scope, authorship, licensing, references and release integrity."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import subprocess
import xml.etree.ElementTree as ET

from public_files import ROOT, public_files

TEXT = {'.cpp','.hpp','.h','.in','.py','.ps1','.cmd','.md','.svg','.json','.txt','.yml','.yaml'}
SOURCE = {'.cpp','.hpp','.h','.in','.py','.ps1','.cmd','.md','.svg','.yml','.yaml'}
PHRASES = re.compile(r'\b(?:CODEX|ChatGPT|TODO|FIXME|WIP|placeholder)\b|note interne|[A-Za-z]:[\\/]Users[\\/][^\\/\s]+[\\/]|/home/[a-z0-9_.-]+/',re.I)


def audit(root=ROOT, index=False):
    names = public_files(root)
    missing = [name for name in names if not (root/name).is_file()]
    if missing:
        raise RuntimeError('Missing public files: '+str(missing))
    ownership = json.loads((root/'FILE-LICENSES.json').read_text(encoding='utf-8'))['files']
    assert set(ownership) == set(names), 'Incomplete file license inventory'
    issues = []
    sources = 0
    for name in names:
        path = root/name
        parts = Path(name).parts
        assert not set(parts).intersection({'private','embedded','workspace','spot','publishing','build','design','diagnostics','validation','.git'})
        assert path.suffix.lower() not in {'.key','.pem','.pdb','.debug','.blend','.img','.mov','.mp4'}
        assert path.stat().st_size < 100*1024*1024, f'File too large: {name}'
        own = ownership[name].get('author') == 'Marco Pezzullo / Silicon LogiX'
        text_file = path.suffix.lower() in TEXT or path.name in {'CMakeLists.txt','.gitignore','.gitattributes','.editorconfig','.clang-format','LICENSE'}
        if text_file:
            text = path.read_text(encoding='utf-8-sig')
            if own and (path.suffix.lower() in SOURCE or path.name in {'CMakeLists.txt','.gitignore','.gitattributes','.editorconfig','.clang-format'}):
                sources += 1
                if 'Marco Pezzullo' not in text[:700] or 'Silicon LogiX Evaluation License 1.0' not in text[:700]:
                    issues.append((name,'Missing author or SLX license header'))
            if own:
                scanned = '\n'.join(line for line in text.splitlines() if not (name == 'scripts/audit-publication.py' and line.startswith('PHRASES =')))
                for match in PHRASES.finditer(scanned):
                    issues.append((name,'Publication text: '+scanned[max(0,match.start()-40):match.end()+75].replace('\n',' ')))
                if re.search(r'-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----',text):
                    issues.append((name,'Private key found'))
            if path.suffix.lower() == '.json':
                json.loads(text)
            if path.suffix.lower() == '.svg':
                ET.fromstring(text)
            if path.suffix.lower() == '.md':
                for link in re.findall(r'\]\(([^)]+)\)',text):
                    link = link.split('#')[0]
                    if not link or re.match(r'^[a-zA-Z]+:',link):
                        continue
                    target = (path.parent/link).resolve()
                    if not target.is_relative_to(root) or target.relative_to(root).as_posix() not in names:
                        issues.append((name,'Unpublished link: '+link))
    expected = json.loads((root/'release-manifest.json').read_text(encoding='utf-8'))['files']
    for name, sha in expected.items():
        if hashlib.sha256((root/name).read_bytes()).hexdigest() != sha:
            issues.append((name,'Release checksum mismatch'))
    if index:
        listed = subprocess.check_output(['git','ls-files','-z'],cwd=root).decode().split('\0')
        assert {name for name in listed if name} == set(names), 'Git index differs from publication inventory'
        assert not subprocess.check_output(['git','diff','--name-only'],cwd=root).strip(), 'Unstaged public changes'
    if issues:
        print(json.dumps(issues,indent=2,ensure_ascii=False))
        raise RuntimeError(f'Publication audit failed: {len(issues)} issues')
    print(json.dumps({'result':'PASS','public_files':len(names),'slx_source_headers':sources,
                      'license_entries':len(ownership),'private_sources':0,'device_credentials':0,
                      'firmware_files':0,'video_files':0},indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--index',action='store_true')
    args = parser.parse_args()
    audit(args.root.resolve(),args.index)
