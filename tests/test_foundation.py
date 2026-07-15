import os
import unittest
from pathlib import Path

from eps16.bus import BusError, ByteMemory, MemoryBus
from eps16.disk_compare import compare
from eps16.hfe import HFEError, crc16_ccitt, parse_header, validate_eps_geometry
from eps16.inspect import inspect_image
from eps16.scsi import BlockDevice, SCSIDiskTarget
from eps16.filesystem import EPSFilesystem
from eps16.rom import ROMError, combine_roms


class FoundationTests(unittest.TestCase):
    def test_mfm_crc_reference(self):
        # First ID field observed in the supplied image: A1 A1 A1 FE 00 00 00 02.
        self.assertEqual(crc16_ccitt(bytes.fromhex("a1a1a1fe00000002")), 0xF95E)

    def test_rejects_non_eps_hfe_geometry(self):
        image = bytearray(1024)
        image[:8] = b"HXCPICFE"
        image[9:17] = bytes([79, 2, 0, 250, 0, 0, 0, 7])
        image[18:20] = (1).to_bytes(2, "little")
        with self.assertRaises(HFEError):
            validate_eps_geometry(parse_header(bytes(image)))

    @unittest.skipUnless(os.environ.get("EPS16_KNOWN_GOOD_HFE") and os.environ.get("EPS16_REFERENCE_IMG"),
                         "set known-good HFE and reference IMG paths")
    def test_known_good_hfe_matches_reference_img(self):
        hfe_path = Path(os.environ["EPS16_KNOWN_GOOD_HFE"])
        img_path = Path(os.environ["EPS16_REFERENCE_IMG"])
        report = compare(hfe_path.read_bytes(), img_path.read_bytes(), str(hfe_path), str(img_path))
        self.assertEqual(report["matching_sectors"], 1600)
        self.assertIsNone(report["first_mismatch"])
        self.assertEqual(report["hfe_logical_sha256"], report["img_sha256"])

    def test_big_endian_bus(self):
        ram = ByteMemory(16)
        bus = MemoryBus()
        bus.map(0x1000, 16, ram.read, ram.write, "RAM")
        bus.write(0x1002, 0x1234, 2)
        self.assertEqual(bus.read(0x1002, 2), 0x1234)
        self.assertEqual(ram.data[2:4], b"\x12\x34")

    def test_bus_rejects_unaligned_word(self):
        ram = ByteMemory(16)
        bus = MemoryBus()
        bus.map(0, 16, ram.read, ram.write, "RAM")
        with self.assertRaises(BusError):
            bus.read(1, 2)

    def test_inspector_signature(self):
        image = bytearray(4096)
        image[0x21F:0x228] = b"EPS130OID"
        image[0x304:0x308] = b"\x74\x00\x4e\xb8"
        report = inspect_image(bytes(image))
        self.assertEqual(report["disk_identifier"], "EPS130OID")
        self.assertEqual(report["first_68000_code_file_offset"], 0x304)

    def test_scsi_read_capacity_and_read10(self):
        blocks = b"A" * 512 + b"B" * 512
        target = SCSIDiskTarget(BlockDevice(blocks))
        capacity = target.execute(bytes.fromhex("25000000000000000000"))
        self.assertEqual(capacity.status, 0)
        self.assertEqual(capacity.data, bytes.fromhex("0000000100000200"))
        read = target.execute(bytes.fromhex("28000000000100000100"))
        self.assertEqual(read.status, 0)
        self.assertEqual(read.data, b"B" * 512)

    def test_scsi_write10(self):
        device = BlockDevice(bytes(1024))
        target = SCSIDiskTarget(device)
        result = target.execute(bytes.fromhex("2a000000000100000100"), b"X" * 512)
        self.assertEqual(result.status, 0)
        self.assertEqual(device.read_blocks(1, 1), b"X" * 512)

    def test_minimal_ensoniq_filesystem(self):
        image = bytearray(1600 * 512)
        device = memoryview(image)[512:1024]
        device[4:6] = (10).to_bytes(2, "big")
        device[6:8] = (2).to_bytes(2, "big")
        device[8:10] = (80).to_bytes(2, "big")
        device[10:14] = (512).to_bytes(4, "big")
        device[14:18] = (1600).to_bytes(4, "big")
        device[30:40] = b"\xffTEST123ID"
        system = memoryview(image)[1024:1536]
        system[0:4] = (1584).to_bytes(4, "big")
        system[28:30] = b"OS"
        directory = memoryview(image)[3 * 512:5 * 512]
        directory[1] = 3
        directory[2:14] = b"TEST        "
        directory[14:16] = (1).to_bytes(2, "big")
        directory[16:18] = (1).to_bytes(2, "big")
        directory[18:22] = (15).to_bytes(4, "big")
        directory[-2:] = b"DR"
        fat = memoryview(image)[5 * 512:15 * 512]
        for block in range(16):
            fat[block * 3 : block * 3 + 3] = (1).to_bytes(3, "big")
        image[15 * 512 : 16 * 512] = b"Z" * 512
        filesystem = EPSFilesystem(bytes(image))
        self.assertEqual(filesystem.volume.label, "TEST123")
        self.assertEqual(filesystem.entries[0].name, "TEST")
        self.assertEqual(filesystem.read_file(filesystem.entries[0]), b"Z" * 512)

    def test_split_rom_interleave(self):
        # SP=$1000, PC=$C00008 split into high and low byte EPROMs.
        upper = bytes.fromhex("00100000704e")
        lower = bytes.fromhex("0000c0000072")
        combined, report = combine_roms(upper, lower)
        self.assertEqual(combined[:12], bytes.fromhex("0000100000c0000070004e72"))
        self.assertEqual(report.reset_program_counter, 0xC00000)

    def test_rejects_swapped_roms(self):
        with self.assertRaises(ROMError):
            combine_roms(bytes.fromhex("000000c0"), bytes.fromhex("00100000"))


if __name__ == "__main__":
    unittest.main()
