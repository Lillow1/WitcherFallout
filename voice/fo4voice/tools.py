"""External tools: ffmpeg (shaping and resampling), xWMAEncode (game audio
format) and the Creation Kit's FaceFXWrapper (lip sync).

Paths and arguments come from a tools file (see tools.example.toml), so a
wrong guess about a tool's command line is a config fix, not a code change.
"""

from __future__ import annotations

import subprocess
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

from .recipe import Recipe

LIP_GENERATE = "generate"  # FaceFXWrapper from the Creation Kit
LIP_VANILLA = "vanilla"    # reuse the original line's lip file (timing won't match exactly)
LIP_NONE = "none"          # no lip movement
LIP_MODES = (LIP_GENERATE, LIP_VANILLA, LIP_NONE)

OUTPUT_RATE = 44100


class ToolError(Exception):
    pass


@dataclass
class Tools:
    ffmpeg: str = "ffmpeg"
    xwma: list[str] = field(default_factory=lambda: ["xWMAEncode.exe", "-b", "48000", "{wav}", "{out}"])
    lipgen: list[str] = field(default_factory=list)
    lip_mode: str = LIP_VANILLA


def load(path: str | Path | None) -> Tools:
    if path is None:
        return Tools()
    with open(path, "rb") as f:
        doc = tomllib.load(f)
    t = Tools(
        ffmpeg=str(doc.get("ffmpeg", "ffmpeg")),
        xwma=[str(a) for a in doc.get("xwma", Tools().xwma)],
        lipgen=[str(a) for a in doc.get("lipgen", [])],
        lip_mode=str(doc.get("lip_mode", LIP_VANILLA)),
    )
    if t.lip_mode not in LIP_MODES:
        raise ValueError(f"lip_mode must be one of {', '.join(LIP_MODES)}")
    if t.lip_mode == LIP_GENERATE and not t.lipgen:
        raise ValueError("lip_mode = 'generate' needs a lipgen command")
    return t


def run(args: list[str]) -> None:
    try:
        proc = subprocess.run(args, capture_output=True, text=True)
    except FileNotFoundError as e:
        raise ToolError(f"{args[0]} not found") from e
    if proc.returncode != 0:
        detail = (proc.stderr or proc.stdout).strip().splitlines()[-3:]
        raise ToolError(f"{Path(args[0]).name} failed ({proc.returncode}): {' | '.join(detail)}")


def fill(template: list[str], **values: str) -> list[str]:
    # Each argument is passed to the process as-is (no shell), so quotes or
    # other characters in a dialogue line can't break the command.
    return [a.format(**values) for a in template]


def shape(tools: Tools, recipe: Recipe, src: Path, dst: Path, src_rate: int) -> None:
    """Pitch, EQ and resample to 44.1 kHz mono 16-bit, the format the game's tools expect."""
    chain = []
    if abs(recipe.pitch - 1.0) > 1e-3:
        # Lower/raise pitch without changing duration.
        chain += [
            f"asetrate={src_rate * recipe.pitch:.0f}",
            f"aresample={src_rate}",
            f"atempo={1.0 / recipe.pitch:.5f}",
        ]
    if recipe.filters:
        chain.append(recipe.filters)
    args = [tools.ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", str(src)]
    if chain:
        args += ["-af", ",".join(chain)]
    args += ["-ar", str(OUTPUT_RATE), "-ac", "1", "-sample_fmt", "s16", str(dst)]
    run(args)


def encode_xwm(tools: Tools, wav: Path, out: Path) -> None:
    run(fill(tools.xwma, wav=str(wav), out=str(out)))


def generate_lip(tools: Tools, wav: Path, text: str, lip: Path, work: Path) -> None:
    run(fill(tools.lipgen, wav=str(wav), lip=str(lip), text=text, resampled=str(work / "resampled.wav")))
