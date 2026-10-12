"""End to end: fake game Data folder -> extract -> build, with the tone
backend, real ffmpeg and stand-in encoder/lip tools."""

import shutil
import sys
import wave

import pytest

from fo4voice import cli, fuz, manifest

from . import fakegame

pytestmark = pytest.mark.skipif(shutil.which("ffmpeg") is None, reason="needs ffmpeg")

VOICE = "sound/voice/fallout4.esm/playervoicefemale01/"

# Stand-ins for xWMAEncode and FaceFXWrapper: copy the wav / write the text.
FAKE_XWMA = "import shutil, sys; shutil.copyfile(sys.argv[-2], sys.argv[-1])"
FAKE_LIP = "import sys; open(sys.argv[1], 'wb').write(b'LIP:' + sys.argv[2].encode())"


@pytest.fixture
def game(tmp_path):
    data = tmp_path / "Data"
    data.mkdir()
    fakegame.plugin(
        data / "Fallout4.esm",
        [
            fakegame.info(0x00012345, [(1, 1), (2, 2)], localized=True),
            fakegame.info(0x00012346, [(1, 3)], localized=True),
            fakegame.info(0x00012347, [(1, 4)], localized=True),
        ],
        localized=True,
    )
    fakegame.ba2(data / "Fallout4 - Interface.ba2", {
        "Strings/Fallout4_en.ILSTRINGS": fakegame.ilstrings({
            1: "Vault-Tec lied to all of us.",
            2: "{Angry} Get out of my way.",
            3: "(Grunts)",
            4: "Thanks, <Alias=Settler>.",
        }),
    })
    vanilla = lambda tag: fuz.pack(b"VANILLA-" + tag, b"OLDAUDIO")
    fakegame.ba2(data / "Fallout4 - Voices.ba2", {
        VOICE + "00012345_1.fuz": vanilla(b"1"),
        VOICE + "00012345_2.fuz": vanilla(b"2"),
        VOICE + "00012346_1.fuz": vanilla(b"3"),
        VOICE + "00012347_1.fuz": vanilla(b"4"),
        VOICE + "00099999_1.fuz": vanilla(b"orphan"),  # no INFO record
    })
    return data


def write_recipe(path, backend="tone"):
    path.write_text(
        f'[voice]\nname = "test"\nbackend = "{backend}"\nspeed = 1.0\n'
        '[[voice.blend]]\nvoice = "bf_emma"\nweight = 1\n'
        '[post]\npitch = 0.95\nfilters = "highpass=f=70"\n'
        '[pronounce]\n"Vault-Tec" = "Vault Tek"\n'
    )
    return path


def write_tools(path, lip_mode):
    py = sys.executable.replace("\\", "/")
    path.write_text(
        f'xwma = ["{py}", "-c", "{FAKE_XWMA}", "{{wav}}", "{{out}}"]\n'
        f'lip_mode = "{lip_mode}"\n'
        f'lipgen = ["{py}", "-c", "{FAKE_LIP}", "{{lip}}", "{{text}}"]\n'
    )
    return path


def test_extract_then_build(game, tmp_path, capsys):
    recipe = write_recipe(tmp_path / "r.toml")
    lines_csv = tmp_path / "lines.csv"
    assert cli.main(["extract", "--data", str(game), "--recipe", str(recipe), "-o", str(lines_csv)]) == 0

    rows = {r.file.rsplit("/", 1)[1]: r for r in manifest.read(lines_csv)}
    assert rows["00012345_1.fuz"].status == "ok"
    assert rows["00012345_1.fuz"].text == "Vault Tek lied to all of us."
    assert rows["00012345_2.fuz"].status == "review"
    assert rows["00012345_2.fuz"].text == "Get out of my way."
    assert rows["00012346_1.fuz"].status == "skip"
    assert rows["00012347_1.fuz"].text == "Thanks."
    assert rows["00099999_1.fuz"].status == "skip"
    assert "no dialogue record" in rows["00099999_1.fuz"].reasons

    out = tmp_path / "out"
    tools_vanilla = write_tools(tmp_path / "tools.toml", "vanilla")
    args = ["build", "--recipe", str(recipe), "--tools", str(tools_vanilla), "--manifest", str(lines_csv),
            "--data", str(game), "--out", str(out)]
    assert cli.main(args) == 0

    built = sorted(p.name for p in (out / VOICE).glob("*.fuz"))
    assert built == ["00012345_1.fuz"]  # review lines need --include-review

    lip, audio = fuz.unpack((out / VOICE / "00012345_1.fuz").read_bytes())
    assert lip == b"VANILLA-1"
    assert audio[:4] == b"RIFF"  # our fake encoder passes the shaped wav through
    wav_path = tmp_path / "check.wav"
    wav_path.write_bytes(audio)
    with wave.open(str(wav_path)) as w:
        assert (w.getframerate(), w.getnchannels(), w.getsampwidth()) == (44100, 1, 2)

    # Resumes: nothing rebuilt; review lines added on request.
    capsys.readouterr()
    assert cli.main(args + ["--include-review"]) == 0
    assert "built 2, already there 1" in capsys.readouterr().out

    # Generated lip sync gets the line's text.
    tools_gen = write_tools(tmp_path / "tools2.toml", "generate")
    out2 = tmp_path / "out2"
    assert cli.main(["build", "--recipe", str(recipe), "--tools", str(tools_gen), "--manifest", str(lines_csv),
                     "--out", str(out2), "--limit", "1"]) == 0
    lip, _ = fuz.unpack((out2 / VOICE / "00012345_1.fuz").read_bytes())
    assert lip == b"LIP:Vault Tek lied to all of us."


def test_failures_are_reported_not_fatal(game, tmp_path, capsys):
    recipe = write_recipe(tmp_path / "r.toml")
    lines_csv = tmp_path / "lines.csv"
    cli.main(["extract", "--data", str(game), "--recipe", str(recipe), "-o", str(lines_csv)])
    tools_bad = tmp_path / "bad.toml"
    tools_bad.write_text('xwma = ["definitely-not-a-real-encoder", "{wav}", "{out}"]\nlip_mode = "none"\n')
    out = tmp_path / "out"
    rc = cli.main(["build", "--recipe", str(recipe), "--tools", str(tools_bad), "--manifest", str(lines_csv),
                   "--out", str(out), "--include-review"])
    assert rc == 1
    assert "failed 3" in capsys.readouterr().out
    assert "not found" in (out / "build_failures.csv").read_text()
    assert not list(out.rglob("*.fuz"))  # nothing half-written


def test_audition_samples(tmp_path):
    recipe = write_recipe(tmp_path / "r.toml")
    out = tmp_path / "aud"
    assert cli.main(["audition", "--recipe", str(recipe), "-o", str(out), "--text", "Hello.", "--text", "Bye."]) == 0
    assert sorted(p.name for p in out.glob("*.wav")) == ["line_01.wav", "line_02.wav"]


def test_recipe_rejects_bad_values(tmp_path):
    from fo4voice import recipe

    bad = tmp_path / "bad.toml"
    bad.write_text('[voice]\nbackend = "kokoro"\n')
    with pytest.raises(ValueError, match="blend"):
        recipe.load(bad)
    bad.write_text('[voice]\nbackend = "tone"\n[post]\npitch = 0.2\n')
    with pytest.raises(ValueError, match="pitch"):
        recipe.load(bad)


def test_shipped_recipe_loads():
    from pathlib import Path

    from fo4voice import recipe

    r = recipe.load(Path(__file__).parent.parent / "voices" / "witcher_inspired.toml")
    assert r.backend == "kokoro" and len(r.blend) == 3 and r.lang == "b"
    assert r.pronounce["Vault-Tec"] == "Vault Tek"
