"""Combine and validate split upper/lower EPS-16 Plus boot ROM images."""

from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import asdict, dataclass
from pathlib import Path


class ROMError(ValueError):
    pass


@dataclass(frozen=True)
class ROMReport:
    upper_sha256: str
    lower_sha256: str
    combined_sha256: str
    size: int
    base_address: int
    end_address: int
    initial_stack_pointer: int
    reset_program_counter: int


def combine_roms(upper: bytes, lower: bytes, base_address: int = 0xC00000) -> tuple[bytes, ROMReport]:
    if not upper or len(upper) != len(lower):
        raise ROMError("upper and lower ROMs must have the same non-zero size")
    combined = bytearray(len(upper) * 2)
    combined[0::2] = upper
    combined[1::2] = lower
    initial_sp = int.from_bytes(combined[0:4], "big")
    reset_pc = int.from_bytes(combined[4:8], "big")
    end_address = base_address + len(combined) - 1
    if initial_sp & 1:
        raise ROMError(f"unaligned initial stack pointer: 0x{initial_sp:08x}")
    if not base_address <= reset_pc <= end_address:
        raise ROMError(
            f"reset PC 0x{reset_pc:08x} is outside expected ROM mapping "
            f"0x{base_address:08x}-0x{end_address:08x}; upper/lower order may be wrong"
        )
    result = bytes(combined)
    return result, ROMReport(
        upper_sha256=hashlib.sha256(upper).hexdigest(),
        lower_sha256=hashlib.sha256(lower).hexdigest(),
        combined_sha256=hashlib.sha256(result).hexdigest(),
        size=len(result),
        base_address=base_address,
        end_address=end_address,
        initial_stack_pointer=initial_sp,
        reset_program_counter=reset_pc,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upper", type=Path, required=True)
    parser.add_argument("--lower", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--base", type=lambda value: int(value, 0), default=0xC00000)
    args = parser.parse_args()

    combined, report = combine_roms(args.upper.read_bytes(), args.lower.read_bytes(), args.base)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(combined)
    encoded = json.dumps(asdict(report), indent=2) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(encoded)
    print(encoded, end="")


if __name__ == "__main__":
    main()

