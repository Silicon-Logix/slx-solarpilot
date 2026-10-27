# Silicon LogiX / SLX SolarPilot
# Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
# License: Silicon LogiX Evaluation License 1.0. See LICENSE.
"""Fetch an explicit PVGIS historical reference, preserving source and cache.

This tool never supplies live weather or modifies the simulator's solar model.
Failed refreshes retain the previous verified cache. No network in the EMS loop.
"""
from pathlib import Path
import argparse
import datetime as dt
import hashlib
import json
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--lat", type=float, required=True)
    p.add_argument("--lon", type=float, required=True)
    p.add_argument("--tilt", type=float, default=26.565051)
    p.add_argument("--aspect", type=float, default=0, help="0=south, 90=west, 180=north")
    p.add_argument("--refresh", action="store_true")
    p.add_argument("--offline", action="store_true")
    args = p.parse_args()
    if not (35 <= args.lat <= 48 and 6 <= args.lon <= 19 and 0 <= args.tilt <= 90 and -180 <= args.aspect <= 180):
        p.error("Parameters outside the supported Italian reference domain")
    params = dict(lat=args.lat, lon=args.lon, peakpower=1, loss=14, angle=args.tilt,
                  aspect=args.aspect, outputformat="json", raddatabase="PVGIS-SARAH3")
    key = hashlib.sha256(json.dumps(params, sort_keys=True).encode()).hexdigest()[:20]
    cache = ROOT / "workspace/pvgis" / (key + ".json")
    if cache.exists() and (args.offline or not args.refresh):
        record = json.loads(cache.read_text("utf-8"))
        body = json.dumps(record["response"], separators=(",", ":"), ensure_ascii=False).encode()
        if hashlib.sha256(body).hexdigest() != record["sha256"]:
            raise RuntimeError("Cached response integrity check failed")
        print(f"Historical cache: {cache}\nRetrieved: {record['retrieved_utc']}")
        return
    if args.offline:
        raise SystemExit("No matching cache. Included Bologna/Roma/Palermo references remain available in the GUI.")
    url = "https://re.jrc.ec.europa.eu/api/v5_3/PVcalc?" + urllib.parse.urlencode(params)
    try:
        with urllib.request.urlopen(url, timeout=30) as response:
            raw = response.read(4 * 1024 * 1024)
        data = json.loads(raw)
        months = data["outputs"]["monthly"]["fixed"]
        if len(months) != 12 or any(not isinstance(m["E_m"], (float, int)) for m in months):
            raise ValueError("Unexpected PVGIS response")
        body = json.dumps(data, separators=(",", ":"), ensure_ascii=False).encode()
        record = dict(schema="slx.solarpilot.pvgis-cache/1", source=url, parameters=params,
                      retrieved_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
                      data_type="historical modeled AC yield; not a forecast",
                      sha256=hashlib.sha256(body).hexdigest(), response=data)
        cache.parent.mkdir(parents=True, exist_ok=True)
        temp = cache.with_suffix(".pending")
        temp.write_text(json.dumps(record, indent=2, ensure_ascii=False), "utf-8")
        temp.replace(cache)
        print(f"Saved historical reference: {cache}")
    except Exception as exc:
        raise SystemExit(f"PVGIS unavailable: {exc}. Existing cache has been retained.") from exc


if __name__ == "__main__":
    main()
