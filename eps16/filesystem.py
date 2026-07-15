"""Read-only parser for the Ensoniq EPS/EPS-16 floppy filesystem."""

from __future__ import annotations

import argparse
import json
from dataclasses import asdict, dataclass
from pathlib import Path


BLOCK_SIZE = 512
DIRECTORY_ENTRY_SIZE = 26
DIRECTORY_BLOCKS = (3, 4)
FAT_BLOCKS = range(5, 15)

FILE_TYPES = {
    0x01: "EPS operating system",
    0x02: "subdirectory",
    0x03: "instrument",
    0x04: "bank",
    0x05: "sequence",
    0x06: "song",
    0x07: "system exclusive",
    0x08: "parent directory",
    0x09: "macro",
    0x17: "EPS-16 Plus bank",
    0x18: "EPS-16 Plus effect",
    0x19: "EPS-16 Plus sequence",
    0x1A: "EPS-16 Plus song",
    0x1B: "EPS-16 Plus operating system",
}


class FilesystemError(ValueError):
    pass


@dataclass(frozen=True)
class VolumeInfo:
    label: str
    sectors_per_track: int
    heads: int
    cylinders: int
    block_size: int
    block_count: int
    free_blocks: int
    os_major: int
    os_minor: int
    minimum_rom_major: int
    minimum_rom_minor: int


@dataclass(frozen=True)
class DirectoryEntry:
    index: int
    file_type: int
    type_name: str
    name: str
    size_blocks: int
    contiguous_blocks: int
    first_block: int
    multi_file_index: int
    byte_count_field: int


class EPSFilesystem:
    def __init__(self, image: bytes):
        if len(image) % BLOCK_SIZE:
            raise FilesystemError("image length must be a multiple of 512 bytes")
        self.image = image
        self.physical_blocks = len(image) // BLOCK_SIZE
        self.volume = self._parse_volume()
        if self.volume.block_size != BLOCK_SIZE:
            raise FilesystemError(f"unsupported block size: {self.volume.block_size}")
        if self.volume.block_count > self.physical_blocks:
            raise FilesystemError("declared block count exceeds image size")
        self.fat = self._parse_fat()
        self.entries = self._parse_directory()

    def block(self, number: int) -> bytes:
        if number < 0 or number >= self.volume.block_count:
            raise FilesystemError(f"block {number} is outside the volume")
        start = number * BLOCK_SIZE
        return self.image[start : start + BLOCK_SIZE]

    def _parse_volume(self) -> VolumeInfo:
        device = self.image[BLOCK_SIZE : 2 * BLOCK_SIZE]
        system = self.image[2 * BLOCK_SIZE : 3 * BLOCK_SIZE]
        if device[38:40] != b"ID":
            raise FilesystemError("missing Ensoniq device ID signature")
        if system[28:30] != b"OS":
            raise FilesystemError("missing Ensoniq system block signature")
        label_raw = device[31:38] if device[30] == 0xFF else b""
        return VolumeInfo(
            label=label_raw.rstrip(b"\x00 ").decode("ascii", "replace"),
            sectors_per_track=int.from_bytes(device[4:6], "big"),
            heads=int.from_bytes(device[6:8], "big"),
            cylinders=int.from_bytes(device[8:10], "big"),
            block_size=int.from_bytes(device[10:14], "big"),
            block_count=int.from_bytes(device[14:18], "big"),
            free_blocks=int.from_bytes(system[0:4], "big"),
            os_major=system[4],
            os_minor=system[5],
            minimum_rom_major=system[6],
            minimum_rom_minor=system[7],
        )

    def _parse_fat(self) -> list[int]:
        raw = b"".join(self.image[number * BLOCK_SIZE : (number + 1) * BLOCK_SIZE] for number in FAT_BLOCKS)
        entries = []
        for block_offset in range(0, len(raw), BLOCK_SIZE):
            block = raw[block_offset : block_offset + BLOCK_SIZE]
            entries.extend(int.from_bytes(block[pos : pos + 3], "big") for pos in range(0, 510, 3))
        if len(entries) < self.volume.block_count:
            raise FilesystemError("FAT is too short for declared volume")
        return entries[: self.volume.block_count]

    def _parse_directory(self) -> list[DirectoryEntry]:
        raw = b"".join(self.block(number) for number in DIRECTORY_BLOCKS)
        if raw[-2:] != b"DR":
            raise FilesystemError("main directory is missing DR signature")
        entries = []
        for index in range(39):
            item = raw[index * DIRECTORY_ENTRY_SIZE : (index + 1) * DIRECTORY_ENTRY_SIZE]
            file_type = item[1]
            if file_type == 0:
                continue
            entries.append(
                DirectoryEntry(
                    index=index,
                    file_type=file_type,
                    type_name=FILE_TYPES.get(file_type, "unknown"),
                    name=item[2:14].rstrip(b"\x00 ").decode("ascii", "replace"),
                    size_blocks=int.from_bytes(item[14:16], "big"),
                    contiguous_blocks=int.from_bytes(item[16:18], "big"),
                    first_block=int.from_bytes(item[18:22], "big"),
                    multi_file_index=item[22],
                    byte_count_field=int.from_bytes(item[23:26], "big"),
                )
            )
        return entries

    def chain(self, entry: DirectoryEntry) -> list[int]:
        if entry.size_blocks == 0:
            return []
        result = []
        seen = set()
        current = entry.first_block
        while len(result) < entry.size_blocks:
            if current in seen:
                raise FilesystemError(f"FAT loop while reading {entry.name!r} at block {current}")
            if current < 0 or current >= self.volume.block_count:
                raise FilesystemError(f"invalid FAT block {current} in {entry.name!r}")
            seen.add(current)
            result.append(current)
            next_block = self.fat[current]
            if next_block == 1:
                break
            if next_block in (0, 2):
                raise FilesystemError(f"broken FAT chain for {entry.name!r} at block {current}")
            current = next_block
        if len(result) != entry.size_blocks:
            raise FilesystemError(
                f"directory says {entry.size_blocks} blocks for {entry.name!r}, FAT yielded {len(result)}"
            )
        return result

    def read_file(self, entry: DirectoryEntry) -> bytes:
        return b"".join(self.block(number) for number in self.chain(entry))


def safe_filename(entry: DirectoryEntry) -> str:
    cleaned = "".join(char if char.isalnum() or char in "._+-" else "_" for char in entry.name)
    return f"{entry.index:02d}_{cleaned or 'unnamed'}_{entry.file_type:02x}.bin"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--output-dir", type=Path, help="extract all directory files")
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    filesystem = EPSFilesystem(args.image.read_bytes())
    report = {
        "volume": asdict(filesystem.volume),
        "files": [asdict(entry) | {"chain": filesystem.chain(entry)} for entry in filesystem.entries],
    }
    if args.output_dir:
        args.output_dir.mkdir(parents=True, exist_ok=True)
        for entry in filesystem.entries:
            (args.output_dir / safe_filename(entry)).write_bytes(filesystem.read_file(entry))
    encoded = json.dumps(report, indent=2) + "\n"
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(encoded)
    print(encoded, end="")


if __name__ == "__main__":
    main()
