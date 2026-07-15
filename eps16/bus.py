"""Minimal big-endian memory bus for a future Motorola 68000 core."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable


Read = Callable[[int, int], int]
Write = Callable[[int, int, int], None]


class BusError(RuntimeError):
    pass


@dataclass
class Region:
    start: int
    end: int
    read: Read
    write: Write
    name: str

    def contains(self, address: int, width: int) -> bool:
        return self.start <= address and address + width - 1 <= self.end


class MemoryBus:
    def __init__(self, address_mask: int = 0xFFFFFF):
        self.address_mask = address_mask
        self.regions: list[Region] = []

    def map(self, start: int, size: int, read: Read, write: Write, name: str) -> None:
        if size <= 0:
            raise ValueError("region size must be positive")
        candidate = Region(start, start + size - 1, read, write, name)
        if any(not (candidate.end < region.start or candidate.start > region.end) for region in self.regions):
            raise ValueError(f"region {name!r} overlaps an existing mapping")
        self.regions.append(candidate)
        self.regions.sort(key=lambda region: region.start)

    def _region(self, address: int, width: int) -> tuple[Region, int]:
        address &= self.address_mask
        if width in (2, 4) and address & 1:
            raise BusError(f"unaligned {width * 8}-bit access at 0x{address:06x}")
        for region in self.regions:
            if region.contains(address, width):
                return region, address - region.start
        raise BusError(f"unmapped {width * 8}-bit access at 0x{address:06x}")

    def read(self, address: int, width: int) -> int:
        region, offset = self._region(address, width)
        return region.read(offset, width) & ((1 << (width * 8)) - 1)

    def write(self, address: int, value: int, width: int) -> None:
        region, offset = self._region(address, width)
        region.write(offset, value & ((1 << (width * 8)) - 1), width)


class ByteMemory:
    def __init__(self, size_or_data: int | bytes, readonly: bool = False):
        self.data = bytearray(size_or_data) if isinstance(size_or_data, bytes) else bytearray(size_or_data)
        self.readonly = readonly

    def read(self, offset: int, width: int) -> int:
        return int.from_bytes(self.data[offset : offset + width], "big")

    def write(self, offset: int, value: int, width: int) -> None:
        if self.readonly:
            raise BusError("write to read-only memory")
        self.data[offset : offset + width] = value.to_bytes(width, "big")

