"""fo4voice: replace the Fallout 4 player voice with an original TTS voice.

  fo4voice extract   --data DATA --recipe R -o lines.csv
  fo4voice audition  --recipe R [--manifest lines.csv --count 20]
  fo4voice build     --data DATA --recipe R --tools T --manifest lines.csv --out OUT
"""

from __future__ import annotations

import argparse
import random
import sys
from collections import Counter
from pathlib import Path

from . import build as builder
from . import manifest, recipe as recipes, tools, tts
from .gamedata import GameData

# Original sample lines for tuning the voice before touching the game files.
SAMPLES = [
    ("01_greeting", "Morning. Whatever you're selling, I'm not buying. Unless it's ammunition."),
    ("02_dry", "Another raider camp. Wonderful. I was starting to miss the smell."),
    ("03_question", "Who sent you? And don't tell me nobody. Somebody always sends someone."),
    ("04_threat", "Walk away. Now. I won't ask twice."),
    ("05_soft", "It's all right. You're safe now. I've got you."),
    ("06_tired", "I've seen too many towns like this one. They never end well."),
    ("07_short", "Fine."),
    ("08_long", "Listen to me. I don't care what the Institute promised you, or what the Brotherhood thinks it's owed. People here are starving, and that's the only thing that matters right now."),
]


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="fo4voice", description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)

    e = sub.add_parser("extract", help="list the player's voice lines and their text")
    e.add_argument("--data", required=True, type=Path, help="Fallout 4 Data folder")
    e.add_argument("--plugin", action="append", help="plugin to read (default Fallout4.esm; repeat for DLC)")
    e.add_argument("--voice-type", default=manifest.DEFAULT_VOICE_TYPE)
    e.add_argument("--language", default="en")
    e.add_argument("--recipe", type=Path, help="recipe whose [pronounce] fixes to apply")
    e.add_argument("-o", "--out", type=Path, default=Path("lines.csv"))

    a = sub.add_parser("audition", help="render sample lines to .wav to tune the voice")
    a.add_argument("--recipe", required=True, type=Path)
    a.add_argument("--tools", type=Path, help="tools file (only ffmpeg is used here)")
    a.add_argument("--manifest", type=Path, help="pick lines from a manifest instead of the built-in samples")
    a.add_argument("--count", type=int, default=20)
    a.add_argument("--seed", type=int, default=1)
    a.add_argument("--text", action="append", help="a line to render (repeatable)")
    a.add_argument("--device", help="cuda or cpu (default: auto)")
    a.add_argument("-o", "--out", type=Path, default=Path("audition"))

    b = sub.add_parser("build", help="generate the replacement .fuz files")
    b.add_argument("--recipe", required=True, type=Path)
    b.add_argument("--tools", required=True, type=Path)
    b.add_argument("--manifest", required=True, type=Path)
    b.add_argument("--data", type=Path, help="Fallout 4 Data folder (needed for lip_mode = 'vanilla')")
    b.add_argument("--out", required=True, type=Path, help="output folder; install it as an MO2 mod")
    b.add_argument("--include-review", action="store_true", help="also build lines marked 'review'")
    b.add_argument("--limit", type=int, help="stop after this many lines (for a test run)")
    b.add_argument("--force", action="store_true", help="rebuild lines that already exist")
    b.add_argument("--device", help="cuda or cpu (default: auto)")

    args = p.parse_args(argv)
    try:
        return {"extract": _extract, "audition": _audition, "build": _build}[args.cmd](args)
    except (FileNotFoundError, ValueError, RuntimeError, tools.ToolError) as err:
        print(f"error: {err}", file=sys.stderr)
        return 1


def _extract(args) -> int:
    pronounce = recipes.load(args.recipe).pronounce if args.recipe else {}
    plugins = [args.data / name for name in (args.plugin or ["Fallout4.esm"])]
    with GameData(args.data) as data:
        rows = manifest.build(data, plugins, args.voice_type, args.language, pronounce)
    manifest.write(args.out, rows)
    counts = Counter(r.status for r in rows)
    print(f"{len(rows)} voice files -> {args.out}")
    for status in ("ok", "review", "skip"):
        print(f"  {status:6} {counts.get(status, 0)}")
    if counts.get("review"):
        print("Lines marked 'review' had text changed; check them in the CSV, then set status to ok or skip.")
    return 0


def _audition(args) -> int:
    recipe = recipes.load(args.recipe)
    if args.text:
        texts = [(f"line_{i:02}", t) for i, t in enumerate(args.text, 1)]
    elif args.manifest:
        rows = [r for r in manifest.read(args.manifest) if r.status != "skip" and r.text]
        random.Random(args.seed).shuffle(rows)
        texts = [(f"{r.info_id}_{r.response}", r.text) for r in rows[: args.count]]
    else:
        texts = SAMPLES
    backend = tts.create(recipe, args.device)
    written = builder.audition(texts, recipe, tools.load(args.tools), args.out, backend)
    print(f"{len(written)} files in {args.out}")
    return 0


def _build(args) -> int:
    recipe = recipes.load(args.recipe)
    cfg = tools.load(args.tools)
    rows = manifest.read(args.manifest)
    backend = tts.create(recipe, args.device)
    data = GameData(args.data) if args.data else None
    try:
        report = builder.build(
            rows, recipe, cfg, args.out, backend, data,
            include_review=args.include_review, limit=args.limit, force=args.force,
        )
    finally:
        if data:
            data.close()
    print(f"built {report.built}, already there {report.existing}, not built (skip/review) {report.skipped}, failed {len(report.failed)}")
    if report.failed:
        args.out.mkdir(parents=True, exist_ok=True)
        path = args.out / "build_failures.csv"
        builder.write_report(path, report)
        print(f"failures listed in {path}")
    return 1 if report.failed else 0


if __name__ == "__main__":
    sys.exit(main())
