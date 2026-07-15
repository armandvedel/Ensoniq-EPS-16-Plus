"""Static inspection of an extracted EPS-16 Plus logical disk image."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def ascii_strings(data: bytes, minimum: int = 8) -> list[dict]:
    result = []
    start = None
    for offset, value in enumerate(data + b"\x00"):
        if 32 <= value < 127:
            start = offset if start is None else start
        elif start is not None:
            if offset - start >= minimum:
                result.append({"offset": start, "text": data[start:offset].decode("ascii")})
            start = None
    return result


def inspect_image(data: bytes) -> dict:
    if len(data) % 512:
        raise ValueError("logical image size is not a multiple of 512 bytes")
    strings = ascii_strings(data)
    notable = [
        item
        for item in strings
        if any(
            token in item["text"].upper()
            for token in ("EPS", "SCSI", "FLOPPY", "WAVEBOY", "ENSONIQ", "EFFECT", "EFX")
        )
    ]
    first_code = None
    # The supplied image has a four-byte length field followed by recognizable
    # 68000 MOVEQ/JSR code. Keep this a signature, not a hard-coded load address.
    for offset in range(0, len(data) - 8, 2):
        if data[offset + 4 : offset + 8] == b"\x74\x00\x4e\xb8":
            first_code = offset + 4
            break
    used = [index for index in range(len(data) // 512) if any(data[index * 512 : (index + 1) * 512])]
    return {
        "sha256": hashlib.sha256(data).hexdigest(),
        "bytes": len(data),
        "sectors": len(data) // 512,
        "used_sectors": len(used),
        "disk_identifier": data[0x21F:0x228].rstrip(b"\x00").decode("ascii", "replace"),
        "first_68000_code_file_offset": first_code,
        "notable_strings": notable,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    report = inspect_image(args.image.read_bytes())
    encoded = json.dumps(report, indent=2) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(encoded)
    print(encoded, end="")


if __name__ == "__main__":
    main()

