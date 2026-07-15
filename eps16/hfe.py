"""HxC HFE v1 reader and CRC-validating IBM MFM sector extractor."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


HFE_SIGNATURE = b"HXCPICFE"
MFM_SYNC = b"\x44\x89" * 3
BIT_REVERSE = bytes(int(f"{value:08b}"[::-1], 2) for value in range(256))
EPS_TRACKS = 80
EPS_SIDES = 2
EPS_SECTORS = tuple(range(10))
EPS_SECTOR_SIZE = 512


class HFEError(ValueError):
    """Raised when an HFE image is malformed or fails validation."""


@dataclass(frozen=True)
class HFEHeader:
    revision: int
    tracks: int
    sides: int
    track_encoding: int
    bitrate_kbps: int
    rpm: int
    interface_mode: int
    track_list_offset_blocks: int


@dataclass(frozen=True)
class SectorID:
    cylinder: int
    head: int
    sector: int
    size_code: int

    @property
    def byte_size(self) -> int:
        return 128 << self.size_code


@dataclass
class ExtractionReport:
    source: str
    source_sha256: str
    image_sha256: str
    header: dict
    sectors: int
    id_crc_valid: int
    data_crc_valid: int
    missing: list[list[int]]
    sector_size: int
    image_size: int


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def parse_header(image: bytes) -> HFEHeader:
    if len(image) < 1024:
        raise HFEError("image is too small to contain an HFE header and track table")
    if image[:8] != HFE_SIGNATURE:
        raise HFEError(f"unsupported HFE signature: {image[:8]!r}")
    return HFEHeader(
        revision=image[8],
        tracks=image[9],
        sides=image[10],
        track_encoding=image[11],
        bitrate_kbps=struct.unpack_from("<H", image, 12)[0],
        rpm=struct.unpack_from("<H", image, 14)[0],
        interface_mode=image[16],
        track_list_offset_blocks=struct.unpack_from("<H", image, 18)[0],
    )


def validate_eps_geometry(header: HFEHeader) -> None:
    actual = (
        header.revision,
        header.tracks,
        header.sides,
        header.track_encoding,
        header.bitrate_kbps,
    )
    expected = (0, EPS_TRACKS, EPS_SIDES, 0, 250)
    if actual != expected or header.rpm not in (0, 300):
        raise HFEError(
            "unsupported EPS HFE geometry: "
            f"revision={header.revision}, tracks={header.tracks}, sides={header.sides}, "
            f"encoding={header.track_encoding}, bitrate={header.bitrate_kbps}, rpm={header.rpm}"
        )
    if header.track_list_offset_blocks < 1:
        raise HFEError("invalid HFE track lookup-table offset")


def _decode_mfm_word(encoded: bytes) -> int:
    if len(encoded) != 2:
        raise HFEError("truncated MFM word")
    word = int.from_bytes(encoded, "big")
    value = 0
    for bit in range(14, -1, -2):
        value = (value << 1) | ((word >> bit) & 1)
    return value


def _decode_words(data: bytes, offset: int) -> bytes:
    end = len(data) - ((len(data) - offset) & 1)
    return bytes(_decode_mfm_word(data[pos : pos + 2]) for pos in range(offset, end, 2))


def _side_stream(track: bytes, side: int) -> bytes:
    chunks = []
    for block in range(0, len(track), 512):
        start = block + side * 256
        if start < len(track):
            chunks.append(track[start : min(start + 256, len(track))])
    return b"".join(chunks).translate(BIT_REVERSE)


def _track_data(image: bytes, header: HFEHeader, track_index: int) -> bytes:
    table = header.track_list_offset_blocks * 512
    entry = table + track_index * 4
    if entry + 4 > len(image):
        raise HFEError(f"track {track_index} has no lookup-table entry")
    offset_blocks, length = struct.unpack_from("<HH", image, entry)
    start = offset_blocks * 512
    end = start + length
    if end > len(image):
        raise HFEError(f"track {track_index} extends past end of image")
    return image[start:end]


def _marks(stream: bytes) -> Iterable[bytes]:
    position = 0
    while True:
        found = stream.find(MFM_SYNC, position)
        if found < 0:
            return
        yield _decode_words(stream, found + len(MFM_SYNC))
        position = found + len(MFM_SYNC)


def extract(image: bytes, source_name: str = "<memory>") -> tuple[bytes, ExtractionReport]:
    header = parse_header(image)
    validate_eps_geometry(header)

    sectors: dict[tuple[int, int, int], bytes] = {}
    id_crc_valid = 0
    data_crc_valid = 0

    for track_index in range(header.tracks):
        track = _track_data(image, header, track_index)
        for side in range(header.sides):
            pending: SectorID | None = None
            for decoded in _marks(_side_stream(track, side)):
                if len(decoded) >= 7 and decoded[0] == 0xFE:
                    sector_id = SectorID(*decoded[1:5])
                    expected = int.from_bytes(decoded[5:7], "big")
                    actual = crc16_ccitt(b"\xA1" * 3 + decoded[:5])
                    if actual != expected:
                        raise HFEError(
                            f"bad ID CRC at track {track_index}, side {side}: "
                            f"expected {expected:04x}, calculated {actual:04x}"
                        )
                    id_crc_valid += 1
                    if (
                        sector_id.cylinder != track_index
                        or sector_id.head != side
                        or sector_id.sector not in EPS_SECTORS
                        or sector_id.byte_size != EPS_SECTOR_SIZE
                    ):
                        raise HFEError(
                            f"invalid EPS sector ID on track {track_index}, side {side}: {sector_id}"
                        )
                    pending = sector_id
                elif pending is not None and decoded and decoded[0] in (0xF8, 0xFB):
                    size = pending.byte_size
                    if len(decoded) < size + 3:
                        raise HFEError(f"truncated data field for sector {pending}")
                    payload = decoded[1 : 1 + size]
                    expected = int.from_bytes(decoded[1 + size : 3 + size], "big")
                    actual = crc16_ccitt(b"\xA1" * 3 + decoded[: 1 + size])
                    if actual != expected:
                        raise HFEError(
                            f"bad data CRC for sector {pending}: "
                            f"expected {expected:04x}, calculated {actual:04x}"
                        )
                    key = (pending.cylinder, pending.head, pending.sector)
                    if key in sectors:
                        raise HFEError(f"duplicate sector {key}")
                    sectors[key] = payload
                    data_crc_valid += 1
                    pending = None

    if not sectors:
        raise HFEError("no MFM sectors found")
    sector_sizes = {len(payload) for payload in sectors.values()}
    if len(sector_sizes) != 1:
        raise HFEError(f"mixed sector sizes are not supported: {sorted(sector_sizes)}")
    sector_size = sector_sizes.pop()
    sector_numbers = list(EPS_SECTORS)
    missing = [
        [cylinder, head, sector]
        for cylinder in range(header.tracks)
        for head in range(header.sides)
        for sector in sector_numbers
        if (cylinder, head, sector) not in sectors
    ]
    if missing:
        raise HFEError(f"missing {len(missing)} sectors; first missing sector is {missing[0]}")

    logical = b"".join(
        sectors[(cylinder, head, sector)]
        for cylinder in range(header.tracks)
        for head in range(header.sides)
        for sector in sector_numbers
    )
    report = ExtractionReport(
        source=source_name,
        source_sha256=hashlib.sha256(image).hexdigest(),
        image_sha256=hashlib.sha256(logical).hexdigest(),
        header=asdict(header),
        sectors=len(sectors),
        id_crc_valid=id_crc_valid,
        data_crc_valid=data_crc_valid,
        missing=missing,
        sector_size=sector_size,
        image_size=len(logical),
    )
    return logical, report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="source HFE image")
    parser.add_argument("--output", type=Path, required=True, help="logical sector image")
    parser.add_argument("--report", type=Path, help="optional JSON extraction report")
    args = parser.parse_args()

    source = args.source.read_bytes()
    logical, report = extract(source, str(args.source))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(logical)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(asdict(report), indent=2) + "\n")
    print(json.dumps(asdict(report), indent=2))


if __name__ == "__main__":
    main()
