"""Compare CRC-validated HFE sectors with an EPS logical IMG sector by sector."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from .hfe import EPS_SECTOR_SIZE, EPS_SECTORS, EPS_SIDES, EPS_TRACKS, HFEError, extract


def _sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def compare(hfe_data: bytes, img_data: bytes, hfe_name: str = "<hfe>", img_name: str = "<img>") -> dict:
    logical, extraction = extract(hfe_data, hfe_name)
    if len(img_data) != len(logical):
        raise HFEError(f"IMG size is {len(img_data)} bytes; expected {len(logical)}")

    sectors = []
    first_mismatch = None
    for block in range(EPS_TRACKS * EPS_SIDES * len(EPS_SECTORS)):
        offset = block * EPS_SECTOR_SIZE
        hfe_sector = logical[offset : offset + EPS_SECTOR_SIZE]
        img_sector = img_data[offset : offset + EPS_SECTOR_SIZE]
        cylinder = block // (EPS_SIDES * len(EPS_SECTORS))
        within_track = block % (EPS_SIDES * len(EPS_SECTORS))
        side = within_track // len(EPS_SECTORS)
        sector = within_track % len(EPS_SECTORS)
        entry = {
            "block": block,
            "cylinder": cylinder,
            "side": side,
            "sector": sector,
            "hfe_sha256": _sha256(hfe_sector),
            "img_sha256": _sha256(img_sector),
            "match": hfe_sector == img_sector,
        }
        sectors.append(entry)
        if not entry["match"] and first_mismatch is None:
            byte = next(i for i, pair in enumerate(zip(hfe_sector, img_sector)) if pair[0] != pair[1])
            first_mismatch = {
                **entry,
                "byte_offset_in_sector": byte,
                "logical_byte_offset": offset + byte,
                "hfe_byte": hfe_sector[byte],
                "img_byte": img_sector[byte],
            }

    return {
        "hfe": hfe_name,
        "img": img_name,
        "hfe_source_sha256": extraction.source_sha256,
        "hfe_logical_sha256": extraction.image_sha256,
        "img_sha256": _sha256(img_data),
        "sector_count": len(sectors),
        "matching_sectors": sum(entry["match"] for entry in sectors),
        "first_mismatch": first_mismatch,
        "sectors": sectors,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("hfe", type=Path)
    parser.add_argument("img", type=Path)
    parser.add_argument("--report", type=Path, help="JSON report containing hashes for all sectors")
    args = parser.parse_args()
    report = compare(args.hfe.read_bytes(), args.img.read_bytes(), str(args.hfe), str(args.img))
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n")
    summary = {key: value for key, value in report.items() if key != "sectors"}
    print(json.dumps(summary, indent=2))
    raise SystemExit(0 if report["first_mismatch"] is None else 1)


if __name__ == "__main__":
    main()
