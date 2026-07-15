"""Small deterministic SCSI-1 disk target for the EPS emulator.

This module deliberately implements only the block commands needed to begin
OS bring-up. It has no wall-clock sleeps: command timing belongs to the
emulator scheduler, which can run in real-time or fast-forward mode.
"""

from __future__ import annotations

from dataclasses import dataclass


class SCSIError(RuntimeError):
    pass


@dataclass(frozen=True)
class SCSIResponse:
    status: int = 0
    data: bytes = b""


class BlockDevice:
    def __init__(self, data: bytes | bytearray, block_size: int = 512, readonly: bool = False):
        if block_size <= 0 or len(data) % block_size:
            raise ValueError("device size must be a multiple of block size")
        self.data = bytearray(data)
        self.block_size = block_size
        self.readonly = readonly

    @property
    def block_count(self) -> int:
        return len(self.data) // self.block_size

    def read_blocks(self, lba: int, count: int) -> bytes:
        if lba < 0 or count < 0 or lba + count > self.block_count:
            raise SCSIError("read outside block device")
        start = lba * self.block_size
        return bytes(self.data[start : start + count * self.block_size])

    def write_blocks(self, lba: int, payload: bytes) -> None:
        if self.readonly:
            raise SCSIError("write to read-only block device")
        if len(payload) % self.block_size:
            raise SCSIError("partial-block write")
        count = len(payload) // self.block_size
        if lba < 0 or lba + count > self.block_count:
            raise SCSIError("write outside block device")
        start = lba * self.block_size
        self.data[start : start + len(payload)] = payload


class SCSIDiskTarget:
    GOOD = 0x00
    CHECK_CONDITION = 0x02

    def __init__(self, device: BlockDevice, vendor: str = "CODEX", product: str = "EPS VIRTUAL DISK"):
        self.device = device
        self.vendor = vendor.encode("ascii")[:8].ljust(8)
        self.product = product.encode("ascii")[:16].ljust(16)
        self.last_sense = bytes(18)

    def _check(self, key: int, asc: int) -> SCSIResponse:
        sense = bytearray(18)
        sense[0] = 0x70
        sense[2] = key
        sense[7] = 10
        sense[12] = asc
        self.last_sense = bytes(sense)
        return SCSIResponse(self.CHECK_CONDITION)

    def execute(self, cdb: bytes, payload: bytes = b"") -> SCSIResponse:
        if not cdb:
            return self._check(0x05, 0x20)
        opcode = cdb[0]
        try:
            if opcode == 0x00:  # TEST UNIT READY
                return SCSIResponse()
            if opcode == 0x03:  # REQUEST SENSE
                allocation = cdb[4] if len(cdb) >= 5 else len(self.last_sense)
                return SCSIResponse(data=self.last_sense[:allocation])
            if opcode == 0x12:  # INQUIRY
                allocation = cdb[4] if len(cdb) >= 5 else 36
                data = bytearray(36)
                data[0] = 0x00  # direct-access device
                data[2] = 0x01  # SCSI-1
                data[4] = 31
                data[8:16] = self.vendor
                data[16:32] = self.product
                data[32:36] = b"1.0 "
                return SCSIResponse(data=bytes(data[:allocation]))
            if opcode == 0x25:  # READ CAPACITY (10)
                last_lba = self.device.block_count - 1
                return SCSIResponse(data=last_lba.to_bytes(4, "big") + self.device.block_size.to_bytes(4, "big"))
            if opcode in (0x08, 0x0A):  # READ(6), WRITE(6)
                if len(cdb) < 6:
                    return self._check(0x05, 0x24)
                lba = ((cdb[1] & 0x1F) << 16) | (cdb[2] << 8) | cdb[3]
                count = cdb[4] or 256
                return self._transfer(opcode == 0x0A, lba, count, payload)
            if opcode in (0x28, 0x2A):  # READ(10), WRITE(10)
                if len(cdb) < 10:
                    return self._check(0x05, 0x24)
                lba = int.from_bytes(cdb[2:6], "big")
                count = int.from_bytes(cdb[7:9], "big")
                return self._transfer(opcode == 0x2A, lba, count, payload)
        except SCSIError:
            return self._check(0x05, 0x21)
        return self._check(0x05, 0x20)

    def _transfer(self, write: bool, lba: int, count: int, payload: bytes) -> SCSIResponse:
        if write:
            expected = count * self.device.block_size
            if len(payload) != expected:
                return self._check(0x05, 0x24)
            self.device.write_blocks(lba, payload)
            return SCSIResponse()
        return SCSIResponse(data=self.device.read_blocks(lba, count))

