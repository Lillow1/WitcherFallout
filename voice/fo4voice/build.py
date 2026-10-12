"""Generate the replacement voice files from a manifest.

For each line: TTS -> shape (pitch/EQ/44.1 kHz) -> xWMA -> lip -> .fuz,
written as a loose file under the output folder. Existing outputs are
skipped, so an interrupted run picks up where it stopped.
"""

from __future__ import annotations

import csv
import os
import tempfile
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Iterable

from . import fuz, lines, tools, tts
from .gamedata import GameData
from .manifest import Line
from .recipe import Recipe


@dataclass
class Report:
    built: int = 0
    existing: int = 0
    skipped: int = 0
    failed: list[tuple[str, str]] = field(default_factory=list)


def build(
    rows: Iterable[Line],
    recipe: Recipe,
    tool_cfg: tools.Tools,
    out_dir: Path,
    backend: tts.Backend,
    data: GameData | None = None,
    include_review: bool = False,
    limit: int | None = None,
    force: bool = False,
    progress: Callable[[str], None] = print,
) -> Report:
    if tool_cfg.lip_mode == tools.LIP_VANILLA and data is None:
        raise ValueError("lip_mode 'vanilla' needs the game's Data folder (--data)")

    allowed = {lines.OK, lines.REVIEW} if include_review else {lines.OK}
    report = Report()
    rows = list(rows)
    todo = [r for r in rows if r.status in allowed and r.text.strip()]
    report.skipped = len(rows) - len(todo)
    if limit is not None:
        todo = todo[:limit]

    started = time.monotonic()
    for i, row in enumerate(todo, 1):
        target = out_dir / row.file
        if target.exists() and not force:
            report.existing += 1
            continue
        try:
            _build_one(row, recipe, tool_cfg, backend, data, target)
            report.built += 1
        except Exception as e:  # keep going; failures are listed in the report
            report.failed.append((row.file, str(e)))
        if i % 25 == 0 or i == len(todo):
            elapsed = time.monotonic() - started
            eta = elapsed / i * (len(todo) - i)
            progress(f"{i}/{len(todo)}  built {report.built}  failed {len(report.failed)}  ~{eta / 60:.0f} min left")
    return report


def _build_one(row: Line, recipe: Recipe, cfg: tools.Tools, backend: tts.Backend, data: GameData | None, target: Path) -> None:
    with tempfile.TemporaryDirectory(prefix="fo4voice-") as tmp:
        work = Path(tmp)
        raw = work / "raw.wav"
        shaped = work / "line.wav"
        xwm = work / "line.xwm"
        lip_path = work / "line.lip"

        tts.write_wav(raw, backend.synthesize(row.text), backend.sample_rate)
        tools.shape(cfg, recipe, raw, shaped, backend.sample_rate)
        tools.encode_xwm(cfg, shaped, xwm)

        if cfg.lip_mode == tools.LIP_GENERATE:
            tools.generate_lip(cfg, shaped, row.text, lip_path, work)
            lip = lip_path.read_bytes()
        elif cfg.lip_mode == tools.LIP_VANILLA:
            assert data is not None
            lip, _ = fuz.unpack(data.read(row.file))
        else:
            lip = b""

        target.parent.mkdir(parents=True, exist_ok=True)
        partial = target.with_suffix(".fuz.partial")
        partial.write_bytes(fuz.pack(lip, xwm.read_bytes()))
        os.replace(partial, target)  # never leave a half-written .fuz behind


def write_report(path: Path, report: Report) -> None:
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["file", "error"])
        w.writerows(report.failed)


def audition(
    texts: list[tuple[str, str]],
    recipe: Recipe,
    tool_cfg: tools.Tools,
    out_dir: Path,
    backend: tts.Backend,
) -> list[Path]:
    """Render (name, text) pairs to shaped .wav files for listening. No game tools needed."""
    out_dir.mkdir(parents=True, exist_ok=True)
    written = []
    with tempfile.TemporaryDirectory(prefix="fo4voice-") as tmp:
        raw = Path(tmp) / "raw.wav"
        for name, text in texts:
            dst = out_dir / f"{name}.wav"
            tts.write_wav(raw, backend.synthesize(text), backend.sample_rate)
            tools.shape(tool_cfg, recipe, raw, dst, backend.sample_rate)
            written.append(dst)
    return written
