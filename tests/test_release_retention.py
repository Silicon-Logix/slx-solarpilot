# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Verify cleanup selection and refusal paths in an isolated fixture workspace.

Uses an isolated fixture, including one migration before recycling fixture files.
"""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    base = ROOT / "build"
    base.mkdir(exist_ok=True)
    # Windows PowerShell must resolve its own modules when Python was launched
    # from PowerShell 7, which supplies a different PSModulePath.
    powershell_environment = {key: value for key, value in os.environ.items()
                              if key.upper() != "PSMODULEPATH"}
    with tempfile.TemporaryDirectory(prefix="retention-test-", dir=base) as directory:
        fixture = Path(directory).resolve()
        assert fixture.is_relative_to(base.resolve())
        (fixture / "scripts").mkdir()
        dist = fixture / "dist"
        dist.mkdir()
        script = fixture / "scripts/retain_latest_release.ps1"
        shutil.copy2(ROOT / "scripts/retain_latest_release.ps1", script)
        keep = "SLX-SolarPilot-0.10.0-windows-x64"
        (dist / keep).mkdir()
        (dist / keep / "release-manifest.json").write_text(json.dumps(dict(product="SLX SolarPilot", version="0.10.0")), "utf-8")
        archive = dist / (keep + ".zip")
        with zipfile.ZipFile(archive, "w") as zipped:
            zipped.writestr("fixture.txt", "validated release fixture")
        checksum = archive.with_suffix(".zip.sha256")
        checksum.write_text(hashlib.sha256(archive.read_bytes()).hexdigest() + "  " + archive.name + "\n", "utf-8")
        old = set()
        for name in ("SLX-SolarPilot-0.9.0-windows-x64", "SLX-SolarPilot-0.10.0-validation-windows-x64"):
            (dist / name).mkdir()
            for suffix in ("", ".zip", ".zip.sha256"):
                old.add(name + suffix)
                if suffix:
                    (dist / (name + suffix)).write_text("old fixture", "utf-8")
        (dist / "operator-notes.txt").write_text("keep", "utf-8")
        (dist / (keep + ".zip.tmp")).write_text("unrecognized artifact", "utf-8")
        old_runtime=dist/'SLX-SolarPilot-0.9.0-windows-x64/workspace'
        old_runtime.mkdir();(old_runtime/'network-peer-plant.json').write_text('{"ski":"fixture"}',encoding='utf-8')
        before = {p.name for p in dist.iterdir()}

        def run():
            return subprocess.run(["powershell.exe", "-NoProfile", "-File", str(script), "-KeepName", keep, "-PlanOnly"], capture_output=True, text=True, timeout=20, env=powershell_environment)

        valid = run()
        assert valid.returncode == 0, valid.stderr
        assert set(json.loads(valid.stdout)["Removed"]) == old
        assert {p.name for p in dist.iterdir()} == before
        original = checksum.read_text("utf-8")
        checksum.write_text("0" * 64 + "  " + archive.name, "utf-8")
        assert run().returncode != 0, "Corrupt retained bundle must preserve previous releases"
        checksum.write_text(original, "utf-8")
        manifest = dist / keep / "release-manifest.json"
        manifest.write_text(json.dumps(dict(product="SLX SolarPilot", version="0.9.0")), "utf-8")
        assert run().returncode != 0, "Mismatched manifest must prevent cleanup"
        manifest.write_text(json.dumps(dict(product="SLX SolarPilot", version="0.10.0")), "utf-8")
        (dist / "SLX-SolarPilot-0.11.0-windows-x64").mkdir()
        assert run().returncode != 0, "A newer release must prevent a downgrade"
        assert all((dist / name).exists() for name in old)
        assert not (dist/keep/'workspace').exists()
        (dist/'SLX-SolarPilot-0.11.0-windows-x64').rmdir()
        migrated=subprocess.run(['powershell.exe','-NoProfile','-File',str(script),'-KeepName',keep],capture_output=True,text=True,timeout=20,env=powershell_environment)
        assert migrated.returncode==0,migrated.stderr
        assert (dist/keep/'workspace/network-peer-plant.json').read_text()=='{"ski":"fixture"}'
        assert not old_runtime.exists() and (dist/'operator-notes.txt').exists()
    print(json.dumps(dict(result="passed", checks=["numeric version ordering", "same-version draft cleanup", "unknown files preserved", "plan mode has no side effects", "corrupt archive refused", "mismatched manifest refused", "downgrade refused"])))


if __name__ == "__main__":
    main()
