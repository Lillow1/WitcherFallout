# Player voice: fo4voice

Replaces the female player character's voice (`PlayerVoiceFemale01`) with an original text-to-speech voice inspired by a Witcher heroine: young, British, low and slightly husky, dry.

**No one's real voice is cloned.** The voice is a weighted blend of [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M)'s stock British voices (Apache-2.0), shaped with pitch and EQ. The tool has no input for reference recordings, by design.

## What you need (Windows)

- Python 3.11 or 3.12. Kokoro's dependencies lag behind newer Pythons.
- [ffmpeg](https://ffmpeg.org) on PATH.
- The Fallout 4 Creation Kit, which provides:
  - `xWMAEncode.exe`, the game's audio format encoder.
  - `FaceFXWrapper.exe`, only if you want new lip sync.
- A GPU helps a lot. There are roughly 13,000 player lines; on CPU expect many hours.

```sh
cd voice
python -m venv .venv
.venv\Scripts\activate
pip install -e .[kokoro]
copy tools.example.toml tools.toml   # then fix the paths inside
```

The first Kokoro run downloads the model and voicepacks from Hugging Face.

## Steps

**1. Tune the voice.** No game files are touched in this step.
```sh
fo4voice audition --recipe voices/witcher_inspired.toml --tools tools.toml
```
This writes 8 sample lines to `audition/`. Listen, then change `voices/witcher_inspired.toml` and run it again:
- The blend weights and `speed` change the voice's character.
- `pitch` makes it lower (<1) or higher (>1).
- `filters` (ffmpeg EQ and compression) controls warmth and grit.

Add `--text "any line"` to try specific lines.

**2. Extract the lines.**
```sh
fo4voice extract --data "C:/Games/Fallout 4/Data" --recipe voices/witcher_inspired.toml -o lines.csv
```
This produces one row per player voice file, and every row gets a status:
- `ok`: speak as is.
- `review`: text was changed. Stage directions like `{Angry}` or `[laughs]` were removed, or a name the game fills in (`<Alias=...>`) was dropped. Open `lines.csv`, fix `text` if needed, and set `status` to `ok` or `skip`.
- `skip`: grunts, pain sounds, and lines with no text. The vanilla file stays.

For the DLCs, add `--plugin Fallout4.esm --plugin DLCCoast.esm` and so on. Each plugin's voice folder and strings are looked up by its name.

**3. Do a test build** on a few lines and listen to them in game:
```sh
fo4voice build --recipe voices/witcher_inspired.toml --tools tools.toml --manifest lines.csv --data "C:/Games/Fallout 4/Data" --out build --limit 20
```
Install `build/` as an MO2 mod. The folder contains `Sound/Voice/Fallout4.esm/PlayerVoiceFemale01/*.fuz`. Loose files override the game's archive.

**4. Full build.** Drop `--limit`. Add `--include-review` once you've gone through the review lines.
- Interrupted runs resume, because finished lines are skipped. Use `--force` to rebuild.
- Failures don't stop the run; they're listed in `build/build_failures.csv`.

## Lip sync

Set `lip_mode` in `tools.toml`:
- `vanilla` (the default) reuses each line's original lip file. It works right away. Mouth timing follows the old performance, so it's close but not exact.
- `generate` makes new lip files with the Creation Kit's FaceFXWrapper. Before a full build, check the argument order in `tools.toml` against your CK install: run it by hand on one line and confirm a `.lip` file appears.
- `none` means no mouth movement.

## How it works

| File | Job |
|---|---|
| `fo4voice/ba2.py` | Reads the game's BA2 archives. |
| `fo4voice/esm.py` | Reads dialogue records (INFO) from `Fallout4.esm`. Handles compressed records and oversized fields. |
| `fo4voice/strings.py` | Reads the localized text tables (`Fallout4_en.ILSTRINGS`). |
| `fo4voice/manifest.py` | Matches voice files to dialogue lines. File `0001A4E8_2.fuz` is response 2 of dialogue record `0001A4E8`. |
| `fo4voice/lines.py` | Cleans text and decides ok / review / skip. |
| `fo4voice/tts.py` | Kokoro with the blended voice, plus a `tone` stand-in backend for testing. |
| `fo4voice/tools.py` | ffmpeg shaping (output 44.1 kHz mono 16-bit), xWMAEncode, FaceFXWrapper. Each runs as a separate process with list arguments, never through a shell. |
| `fo4voice/build.py` | Runs TTS, then shaping, xWMA, lip sync and `.fuz` packing for each line. Writes are atomic. |

Run the tests with `pip install -e .[test]` and then `pytest`. They build tiny fake game files, so Fallout 4 doesn't need to be installed.

## Known limits

- **The Kokoro step hasn't been run yet.** It was written against Kokoro's source, but its model couldn't be downloaded where this was built. The first `audition` run is the real test.
- **The dialogue is still written for Nora.** In this voice she'll sound like a different woman saying Nora's lines. Rewriting the most out-of-character lines is a separate job.
- **Emotion isn't used yet.** Each line's emotion is in the CSV (`emotion`), but every line is read in the same tone. Per-emotion speed and pitch could come next.
- **The male voice (`PlayerVoiceMale01`) isn't covered by the recipe.** Use `--voice-type PlayerVoiceMale01` with a different recipe if you want it.
