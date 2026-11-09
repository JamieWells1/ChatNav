"""Binary fix batch: 10-byte header, then N fix records of 28 bytes (little-endian).

Header: seq u32, fw version u16, drop count u16, record count u16.
Record layout matches firmware/src/gps/fix_record.h.
"""

import struct
from dataclasses import dataclass
from typing import NamedTuple

HEADER = struct.Struct("<IHHH")
RECORD = struct.Struct("<IIHiiHHhBBBB")
MAX_RECORDS = 4000

FLAG_TIME_VALID = 0x01
FLAG_RMC_VALID = 0x02
FLAG_GGA = 0x04
FLAG_POS = 0x08


class Fix(NamedTuple):
    device_ms: int
    utc_s: int
    utc_ms: int
    lat_e7: int
    lon_e7: int
    speed_cmps: int
    course_deg10: int
    alt_dm: int
    sats: int
    quality: int
    hdop10: int
    flags: int


@dataclass(frozen=True)
class Batch:
    seq: int
    fw: int
    drops: int
    fixes: list[Fix]


class BatchError(ValueError):
    pass


def parse_batch(body: bytes) -> Batch:
    if len(body) < HEADER.size:
        raise BatchError("short header")
    seq, fw, drops, count = HEADER.unpack_from(body)
    if count > MAX_RECORDS:
        raise BatchError("too many records")
    if len(body) != HEADER.size + count * RECORD.size:
        raise BatchError("length does not match record count")
    fixes = [Fix(*RECORD.unpack_from(body, HEADER.size + i * RECORD.size)) for i in range(count)]
    return Batch(seq, fw, drops, fixes)


def pack_batch(seq: int, fw: int, drops: int, fixes: list[Fix]) -> bytes:
    out = bytearray(HEADER.pack(seq, fw, drops, len(fixes)))
    for f in fixes:
        out += RECORD.pack(*f)
    return bytes(out)
