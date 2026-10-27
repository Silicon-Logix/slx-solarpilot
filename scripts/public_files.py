# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Define the Windows application, public interface and SDK distribution."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FILES = [
    '.clang-format', '.editorconfig', '.gitattributes', '.gitignore',
    '.github/workflows/validate.yml',
    'CMakeLists.txt', 'LICENSE', 'README.md', 'THIRD_PARTY_NOTICES.md',
    'FILE-LICENSES.json', 'release-manifest.json', 'SHA256SUMS',
    'Run-SLX-SolarPilot.cmd', 'Run-SLX-EMS-Reference.cmd',
    'SLX-SolarPilot.exe', 'slx-solarpilot-cli.exe', 'slx-solarpilot-core.dll', 'slx-eebus.exe',
    'core/windows-x64/slx-solarpilot-core.dll',
    'core/windows-x64/slx-eebus.exe', 'core/windows-x64/libslx-solarpilot-core.dll.a',
    'examples/policy.cpp', 'examples/energy_service_client.py',
    'docs/API.md', 'docs/ARCHITECTURE.md', 'docs/MODEL.md', 'docs/EMS.md',
    'docs/PROTOCOL.md', 'docs/INTEROPERABILITY.md', 'docs/ENERGY_SERVICE.md', 'docs/VALIDATION.md',
    'docs/RELEASE_NOTES_0.4.5.md', 'docs/images/workspace.png',
    'scripts/bootstrap.py', 'scripts/bootstrap_toolchain.py', 'scripts/bootstrap_eebus.py',
    'scripts/build-windows.ps1', 'scripts/dependencies.lock.json',
    'scripts/eebus.dependencies.lock.json', 'scripts/sunspec.models.lock.json',
    'scripts/verify_stairs.py', 'scripts/pvgis.py', 'scripts/retain_latest_release.ps1',
    'scripts/public_files.py', 'scripts/audit-publication.py', 'scripts/package-windows.py',
    'tests/core_tests.cpp', 'tests/host_tests.cpp', 'tests/ems_tests.cpp', 'tests/numerical_tests.cpp',
    'tests/test_network.py', 'tests/test_eebus.py', 'tests/test_external_controller.py',
    'tests/sunspec_fixture.py', 'tests/test_release_retention.py',
    'SLX_SolarPilot/scene/SLX_SolarPilot_Runtime.glb',
    'SLX_SolarPilot/components/SLX_SolarPilot_PV_North16.glb',
    'SLX_SolarPilot/components/SLX_SolarPilot_PV_South10_Wiring.glb',
    'SLX_SolarPilot/config/scene_contract.json',
    'SLX_SolarPilot/config/appliance_asset_contract.json',
    'SLX_SolarPilot/config/equipment_asset_contract.json',
    'SLX_SolarPilot/config/components_manifest.json',
    'SLX_SolarPilot/config/pv_configuration_presets.json',
    'SLX_SolarPilot/config/house_contract.json',
    'SLX_SolarPilot/config/pvgis_sources.json',
    'SLX_SolarPilot/config/public_data/bologna_pvgis_reference.json',
    'SLX_SolarPilot/config/public_data/roma_pvgis_reference.json',
    'SLX_SolarPilot/config/public_data/palermo_pvgis_reference.json',
    'SLX_SolarPilot/docs/NOTICE.txt', 'SLX_SolarPilot/docs/DejaVu_font_notice.txt',
]
TREES = ['desktop', 'sdk/include', 'assets/branding', 'config', 'licenses']


def public_files(root=ROOT):
    names = set(FILES)
    for tree in TREES:
        names.update(p.relative_to(root).as_posix() for p in (root/tree).rglob('*')
                     if p.is_file() and not p.is_symlink() and '__pycache__' not in p.parts
                     and p.suffix.lower() not in {'.pyc', '.pdb', '.debug', '.blend', '.blend1', '.pem', '.key'})
    return sorted(names)


def gitignore(names):
    lines = ['# Silicon LogiX / SLX SolarPilot',
             '# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.',
             '# License: Silicon LogiX Evaluation License 1.0. See LICENSE.', '*', '!*/']
    lines += ['!/'+name for name in names]
    lines += ['/'+name+'/' for name in ['private','build','dist','embedded','design','publishing',
                                       'spot','workspace','diagnostics','validation','.vs','.vscode']]
    lines += ['/AGENTS.md', '*.pdb', '*.debug', '*.pyc', '__pycache__/']
    return '\n'.join(lines)+'\n'
