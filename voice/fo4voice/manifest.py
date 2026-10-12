"""The line manifest: one row per player voice file, with the text to speak.

It's a CSV so you can open it in a spreadsheet, fix lines, and change
status before building. Build reads `text` and `status` back from it.
"""

from __future__ import annotations

import csv
import re
from dataclasses import asdict, dataclass, fields
from pathlib import Path
from typing import Iterable

from . import esm, lines, strings
from .gamedata import GameData

DEFAULT_VOICE_TYPE = "PlayerVoiceFemale01"
_FILE_NAME = re.compile(r"([0-9a-f]{8})_(\d+)\.fuz$")


@dataclass
class Line:
    status: str
    file: str        # path inside Data, e.g. sound/voice/fallout4.esm/playervoicefemale01/0001a4e8_1.fuz
    plugin: str
    info_id: str     # INFO form ID as 8 hex digits, as in the file name
    response: int
    emotion: str     # emotion keyword form ID, hex
    raw_text: str
    text: str
    reasons: str


def voice_dir(plugin: str, voice_type: str) -> str:
    return f"sound/voice/{plugin}/{voice_type}/".lower()


def build(
    data: GameData,
    plugin_paths: Iterable[Path],
    voice_type: str = DEFAULT_VOICE_TYPE,
    language: str = "en",
    pronounce: dict[str, str] | None = None,
) -> list[Line]:
    out: list[Line] = []
    for plugin_path in plugin_paths:
        plugin = esm.Plugin(plugin_path)
        table = _load_strings(data, plugin, language) if plugin.localized else {}

        # Voice file names carry the low 24 bits of the INFO's form ID.
        infos: dict[int, esm.Info] = {}
        for info in plugin.infos():
            if (info.form_id >> 24) == plugin.own_index:
                infos[info.form_id & 0xFFFFFF] = info

        for path in data.list(voice_dir(plugin.name, voice_type), ".fuz"):
            m = _FILE_NAME.search(path)
            if not m:
                continue
            info_hex, number = m.group(1), int(m.group(2))
            info = infos.get(int(info_hex, 16) & 0xFFFFFF)
            response = next((r for r in info.responses if r.number == number), None) if info else None

            raw = None
            if response is not None:
                raw = response.text if response.string_id is None else table.get(response.string_id)

            if response is None:
                prepared = lines.Prepared("", lines.SKIP, ["no dialogue record for this file"])
            elif raw is None:
                prepared = lines.Prepared("", lines.SKIP, ["text not found in string table"])
            else:
                prepared = lines.prepare(raw, pronounce)

            out.append(
                Line(
                    status=prepared.status,
                    file=path,
                    plugin=plugin.name,
                    info_id=info_hex.upper(),
                    response=number,
                    emotion=f"{response.emotion:08X}" if response else "",
                    raw_text=raw or "",
                    text=prepared.text,
                    reasons="; ".join(prepared.reasons),
                )
            )
    return out


def _load_strings(data: GameData, plugin: esm.Plugin, language: str) -> dict[int, str]:
    stem = Path(plugin.name).stem
    path = f"strings/{stem}_{language}.ilstrings"
    if not data.exists(path):
        raise FileNotFoundError(
            f"{plugin.name} is localized but {path} wasn't found in loose files or archives"
        )
    return strings.parse(data.read(path), "ilstrings")


_COLUMNS = [f.name for f in fields(Line)]


def write(path: str | Path, rows: Iterable[Line]) -> None:
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=_COLUMNS)
        w.writeheader()
        for row in rows:
            w.writerow(asdict(row))


def read(path: str | Path) -> list[Line]:
    with open(path, newline="", encoding="utf-8") as f:
        return [
            Line(**{**row, "response": int(row["response"])})
            for row in csv.DictReader(f)
        ]
