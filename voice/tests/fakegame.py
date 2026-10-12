"""Builders for tiny synthetic Fallout 4 files, so tests run without the game."""

from __future__ import annotations

import struct
import zlib
from pathlib import Path


def ba2(path: Path, files: dict[str, bytes], compress: bool = True) -> None:
    names = list(files)
    header_size = 24
    records_size = 36 * len(names)
    blobs = []
    offset = header_size + records_size
    records = b""
    for name in names:
        raw = files[name]
        packed = zlib.compress(raw) if compress else b""
        stored = packed or raw
        ext = Path(name).suffix.lstrip(".").encode()[:4].ljust(4, b"\0")
        records += struct.pack("<I4sIIQIII", 0, ext, 0, 0, offset, len(packed), len(raw), 0xBAADF00D)
        blobs.append(stored)
        offset += len(stored)
    name_table = b"".join(struct.pack("<H", len(n)) + n.encode() for n in names)
    header = struct.pack("<4sI4sIQ", b"BTDX", 1, b"GNRL", len(names), offset)
    path.write_bytes(header + records + b"".join(blobs) + name_table)


def sub(tag: str, payload: bytes) -> bytes:
    if len(payload) > 0xFFFF:
        return struct.pack("<4sHI", b"XXXX", 4, len(payload)) + tag.encode() + struct.pack("<H", 0) + payload
    return tag.encode() + struct.pack("<H", len(payload)) + payload


def record(tag: str, form_id: int, payload: bytes, flags: int = 0, compress: bool = False) -> bytes:
    if compress:
        payload = struct.pack("<I", len(payload)) + zlib.compress(payload)
        flags |= 0x00040000
    return struct.pack("<4sIIIIHH", tag.encode(), len(payload), flags, form_id, 0, 131, 0) + payload


def group(label: bytes, group_type: int, contents: bytes) -> bytes:
    return struct.pack("<4sI4siHHHH", b"GRUP", 24 + len(contents), label, group_type, 0, 0, 131, 0) + contents


def info(form_id: int, responses: list[tuple[int, object]], localized: bool, compress: bool = False, extra: bytes = b"") -> bytes:
    """responses: (number, text or string id)."""
    payload = extra
    for number, value in responses:
        payload += sub("TRDA", struct.pack("<IB3xI", 0x000A1B2C, number, 0))
        if localized:
            payload += sub("NAM1", struct.pack("<I", value))
        else:
            payload += sub("NAM1", str(value).encode("cp1252") + b"\0")
        payload += sub("NAM2", b"\0")
    return record("INFO", form_id, payload, compress=compress)


def plugin(path: Path, infos: list[bytes], localized: bool, masters: list[str] = ()) -> None:
    hedr = sub("HEDR", struct.pack("<fII", 0.95, len(infos), 0x800))
    header_payload = hedr + b"".join(sub("MAST", m.encode() + b"\0") + sub("DATA", b"\0" * 8) for m in masters)
    tes4 = record("TES4", 0, header_payload, flags=0x81 if localized else 0x01)
    other = group(b"QUST", 0, record("QUST", 0x00001000, sub("EDID", b"SomeQuest\0")))
    dial = record("DIAL", 0x00002000, sub("EDID", b"SomeTopic\0"))
    topic_children = group(struct.pack("<I", 0x00002000), 7, b"".join(infos))
    path.write_bytes(tes4 + other + group(b"DIAL", 0, dial + topic_children))


def ilstrings(table: dict[int, str]) -> bytes:
    directory = b""
    data = b""
    for sid, text in table.items():
        directory += struct.pack("<II", sid, len(data))
        raw = text.encode("cp1252") + b"\0"
        data += struct.pack("<I", len(raw)) + raw
    return struct.pack("<II", len(table), len(data)) + directory + data
