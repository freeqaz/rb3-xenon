"""pytest coverage for scripts/verify_target_image.py (collected by the `scripts` root
of scripts/test_tools.py). Never reads orig/: every case builds its own image."""
import hashlib
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import verify_target_image as vti  # noqa: E402


def _tree(tmp_path, body=b"clean"):
    (tmp_path / "orig").mkdir()
    (tmp_path / "orig" / "default.xex").write_bytes(body)
    man = tmp_path / "build.sha1"
    man.write_text(f"{hashlib.sha1(b'clean').hexdigest()}  orig/default.xex\n")
    return man


def test_selftest_passes():
    assert vti.selftest() == 0


def test_match_writes_stamp(tmp_path):
    man = _tree(tmp_path)
    stamp = tmp_path / "out" / "image.stamp"
    rc = vti.main(["--manifest", str(man), "--root", str(tmp_path),
                   "--stamp-out", str(stamp), "--quiet"])
    assert rc == 0 and stamp.read_text().endswith("orig/default.xex\n")


def test_patched_image_fails_and_writes_nothing(tmp_path):
    man = _tree(tmp_path, body=b"patched")
    stamp = tmp_path / "image.stamp"
    rc = vti.main(["--manifest", str(man), "--root", str(tmp_path),
                   "--stamp-out", str(stamp), "--quiet"])
    assert rc == 1 and not stamp.exists()


def test_empty_manifest_is_rc2(tmp_path):
    man = tmp_path / "build.sha1"
    man.write_text("# only a comment\n")
    assert vti.main(["--manifest", str(man), "--root", str(tmp_path)]) == 2


def test_repo_manifest_names_one_xex():
    entries = vti.parse_manifest(Path(__file__).resolve().parent.parent / vti.DEFAULT_MANIFEST)
    assert [p for _, p in entries] == ["orig/45410914/default.xex"]
    assert entries[0][0] in vti.KNOWN
