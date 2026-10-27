# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Check the actual GLB tread planes and their configured finish separation."""
from pathlib import Path
import argparse
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parents[1]


def verify(root=ROOT):
    raw = (root / "SLX_SolarPilot/scene/SLX_SolarPilot_Runtime.glb").read_bytes()
    size = struct.unpack_from("<I", raw, 12)[0]
    doc = json.loads(raw[20:20 + size])
    buffer = memoryview(raw)[28 + size:]
    parents = {child: i for i, node in enumerate(doc["nodes"]) for child in node.get("children", [])}
    index = next(i for i, node in enumerate(doc["nodes"]) if node.get("name") == "Rigid_house_staircase_root")
    node = doc["nodes"][index]
    # These are world-space rigid vertices in the supplied asset. Fail if a new
    # asset changes that basis rather than silently measuring the wrong planes.
    ancestor = index
    while True:
        assert not any(key in doc["nodes"][ancestor] for key in ("translation", "rotation", "scale", "matrix"))
        if ancestor not in parents:
            break
        ancestor = parents[ancestor]

    def vec3(accessor):
        a = doc["accessors"][accessor]
        view = doc["bufferViews"][a["bufferView"]]
        assert a["componentType"] == 5126 and a["type"] == "VEC3"
        offset = view.get("byteOffset", 0) + a.get("byteOffset", 0)
        return [struct.unpack_from("<3f", buffer, offset + i * view.get("byteStride", 12)) for i in range(a["count"])]

    planes = {}
    vertex_counts = {}
    for primitive in doc["meshes"][node["mesh"]]["primitives"]:
        material = doc["materials"][primitive["material"]]["name"]
        if material not in ("SP_House_UpperIvory", "SP_House_OakFloor"):
            continue
        positions = vec3(primitive["attributes"]["POSITION"])
        normals = vec3(primitive["attributes"]["NORMAL"])
        heights = sorted(p[1] for p, n in zip(positions, normals) if n[1] > .99)
        distinct = []
        for y in heights:
            if not distinct or abs(y - distinct[-1]) > .00005:
                distinct.append(y)
        planes[material] = distinct
        vertex_counts[material] = len(positions)
    body, finish = planes["SP_House_UpperIvory"], planes["SP_House_OakFloor"]
    expected = json.loads((root / "SLX_SolarPilot/config/house_contract.json").read_text("utf-8"))["stairs"]["risers"]
    assert len(body) == len(finish) == expected == 17
    original_gaps = [oak - concrete for oak, concrete in zip(finish, body)]
    assert max(abs(gap) for gap in original_gaps) < .00001, "Source asset changed; reassess the repair"
    overrides = json.loads((root / "config/visuals.json").read_text("utf-8"))["surface_repairs"]
    shift = sum(repair.get("translation_y_m", 0) for repair in overrides
                if repair["node"] == node["name"] and repair["material"] == "SP_House_OakFloor")
    corrected = [gap + shift for gap in original_gaps]
    assert all(.0039 < gap < .0041 for gap in corrected), "Stair finish is not separated by 4 mm"
    return dict(result="passed", source_glb_sha256=hashlib.sha256(raw).hexdigest(),
                treads=expected, coincident_original_treads=len(original_gaps),
                minimum_repaired_gap_m=min(corrected), maximum_repaired_gap_m=max(corrected),
                repaired_vertices=vertex_counts["SP_House_OakFloor"])


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    result = verify()
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(result, indent=2) + "\n", "utf-8")
    print(json.dumps(result, indent=2))
