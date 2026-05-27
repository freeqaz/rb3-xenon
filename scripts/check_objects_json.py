#!/usr/bin/env python3
"""
Check for mismatches between src/ files and config/45410914/objects.json

Reports:
- Source files that exist but aren't in objects.json
- objects.json entries marked MISSING that have source files
- Optionally: entries that might need status updates

Usage:
    ./scripts/check_objects_json.py          # Check only
    ./scripts/check_objects_json.py --fix    # Fix MISSING -> NonMatching
"""

import argparse
import json
import sys
from pathlib import Path

# Third-party library directories to ignore (not part of the actual build)
IGNORE_PREFIXES = [
    "system/jpeg/",
    "system/stlport/",
    "system/synth/tomcrypt/",
    "system/synth_xbox/soundtouch/source/SoundStretch/",
    "system/synth_xbox/soundtouch/source/SoundTouchDLL/",
    "system/synth_xbox/soundtouch/source/SoundTouch/3dnow",
    "system/synth_xbox/soundtouch/source/SoundTouch/BPM",
    "system/synth_xbox/soundtouch/source/SoundTouch/Peak",
    "system/synth_xbox/soundtouch/source/SoundTouch/cpu_detect",
    "system/synth_xbox/soundtouch/source/SoundTouch/mmx",
    "system/synth_xbox/soundtouch/source/SoundTouch/sse",
    "system/oggvorbis/analysis.c",
    "system/oggvorbis/barkmel.c",
    "system/oggvorbis/lookup.c",
    "system/oggvorbis/lpc.c",
    "system/oggvorbis/misc.c",
    "system/oggvorbis/psytune.c",
    "system/oggvorbis/tone.c",
    "system/oggvorbis/vorbisenc.c",
    "system/oggvorbis/vorbisfile.c",
    "system/zlib/compress.c",
    "system/zlib/example.c",
    "system/zlib/gzio.c",
    "system/zlib/infback.c",
    "system/zlib/minigzip.c",
    "system/zlib/uncompr.c",
]

# curl files that aren't used
IGNORE_CURL = [
    "amigaos", "asyn-ares", "axtls", "curl_gethostname", "curl_gssapi",
    "curl_ntlm", "curl_ntlm_core", "curl_ntlm_msgs", "curl_ntlm_wb",
    "curl_rtmp", "curl_sspi", "cyassl", "getenv", "gtls", "hostip6",
    "hostsyn", "http_negotiate", "http_negotiate_sspi", "idn_win32",
    "if2ip", "krb4", "krb5", "ldap", "md4", "memdebug", "non-ascii",
    "nss", "nwlib", "nwos", "openldap", "polarssl", "qssl", "security",
    "socks_gssapi", "socks_sspi", "ssh", "ssluse", "strdup", "strtoofft",
    "telnet", "version",
]

def should_ignore(path: str) -> bool:
    """Check if a path should be ignored (third-party libs not in build)."""
    for prefix in IGNORE_PREFIXES:
        if path.startswith(prefix) or path == prefix.rstrip('/'):
            return True
    # curl ignores
    if path.startswith("system/net/curl/lib/"):
        basename = path.split("/")[-1].replace(".c", "")
        if basename in IGNORE_CURL:
            return True
    # json-c ignores
    if path.startswith("system/net/json-c/"):
        return True
    return False

def main():
    parser = argparse.ArgumentParser(description="Check objects.json sync with src/")
    parser.add_argument("--fix", action="store_true",
                        help="Fix MISSING entries that have source files")
    parser.add_argument("--all", action="store_true",
                        help="Show all files, including ignored third-party libs")
    args = parser.parse_args()

    repo_root = Path(__file__).parent.parent
    objects_json_path = repo_root / "config" / "45410914" / "objects.json"
    src_dir = repo_root / "src"

    if not objects_json_path.exists():
        print(f"Error: {objects_json_path} not found")
        sys.exit(1)

    with open(objects_json_path) as f:
        config = json.load(f)

    # Collect all entries from objects.json with their unit info
    json_entries = {}  # path -> (status, unit_name)
    for unit_name, unit_data in config.items():
        objects = unit_data.get("objects", {})
        for obj_path, status_or_obj in objects.items():
            if isinstance(status_or_obj, dict):
                status = status_or_obj.get("status", "Unknown")
            else:
                status = status_or_obj
            json_entries[obj_path] = (status, unit_name)

    # Find all source files in src/
    src_files = set()
    for ext in ["*.cpp", "*.c"]:
        for f in src_dir.rglob(ext):
            rel_path = str(f.relative_to(src_dir))
            src_files.add(rel_path)

    json_paths = set(json_entries.keys())
    issues_found = False

    # 1. Source files not in objects.json
    missing_from_json = src_files - json_paths
    if not args.all:
        missing_from_json = {p for p in missing_from_json if not should_ignore(p)}

    if missing_from_json:
        issues_found = True
        print("=== Source files NOT in objects.json ===")
        print("These files exist in src/ but have no entry:")
        for path in sorted(missing_from_json):
            print(f"  {path}")
        print()

    # 2. MISSING entries that have source files
    missing_but_exists = []
    for path, (status, unit) in json_entries.items():
        if status == "MISSING" and path in src_files:
            missing_but_exists.append((path, unit))

    if missing_but_exists:
        issues_found = True
        print("=== MISSING entries that have source files ===")
        print("These are marked MISSING but the source file exists:")
        for path, unit in sorted(missing_but_exists):
            print(f"  {path} (unit: {unit})")

        if args.fix:
            print("\nFixing...")
            fixed = 0
            for path, unit in missing_but_exists:
                obj_entry = config[unit]["objects"][path]
                if isinstance(obj_entry, dict):
                    config[unit]["objects"][path]["status"] = "NonMatching"
                else:
                    config[unit]["objects"][path] = "NonMatching"
                fixed += 1
                print(f"  Fixed: {path}")

            with open(objects_json_path, "w") as f:
                json.dump(config, f, indent=4)
                f.write("\n")
            print(f"\nUpdated {fixed} entries in objects.json")
        else:
            print("\nRun with --fix to update these to NonMatching")
        print()

    # 3. NonMatching entries without source files
    nonmatching_no_source = []
    for path, (status, unit) in json_entries.items():
        if status == "NonMatching" and path not in src_files:
            nonmatching_no_source.append(path)

    if nonmatching_no_source:
        issues_found = True
        print("=== NonMatching entries WITHOUT source files ===")
        print("These are marked NonMatching but no source file exists:")
        for path in sorted(nonmatching_no_source):
            print(f"  {path}")
        print()

    # Summary
    total_json = len(json_entries)
    missing_count = sum(1 for s, _ in json_entries.values() if s == "MISSING")
    nonmatching_count = sum(1 for s, _ in json_entries.values() if s == "NonMatching")

    print(f"=== Summary ===")
    print(f"Total entries in objects.json: {total_json}")
    print(f"  MISSING: {missing_count}")
    print(f"  NonMatching: {nonmatching_count}")
    print(f"Source files in src/: {len(src_files)}")

    if not issues_found:
        print("\nAll checks passed!")
        return 0

    return 1

if __name__ == "__main__":
    sys.exit(main())
