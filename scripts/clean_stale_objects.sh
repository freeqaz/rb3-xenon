#!/usr/bin/env bash
#
# Invalidate stale .obj files by touching their source .cpp files.
#
# Ninja doesn't track PCH dependencies, so after header/PCH changes,
# .obj files built against the old PCH are stale. This script finds
# them by comparing timestamps and touches the corresponding .cpp
# so ninja will rebuild them on the next invocation.
#
# Usage:
#   scripts/clean_stale_objects.sh          # Touch .cpp for objs older than PCH
#   scripts/clean_stale_objects.sh --all    # Touch ALL .cpp files (full rebuild)
#   scripts/clean_stale_objects.sh --dry-run  # Show what would be touched
#   scripts/clean_stale_objects.sh --all --dry-run
#
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${REPO}/build/45410914"
PCH="${BUILD_DIR}/pch/system.pch"
SRC_DIR="${REPO}/src"
MODE="stale"
DRY_RUN=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --all|-a)
            MODE="all"
            shift
            ;;
        --dry-run|-n)
            DRY_RUN=1
            shift
            ;;
        --help|-h)
            sed -n '2,14p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 1
            ;;
    esac
done

if [[ "${MODE}" == "all" ]]; then
    # Touch every .cpp under src/
    mapfile -t cpps < <(find "${SRC_DIR}" -name "*.cpp" -type f)
    count=${#cpps[@]}

    if [[ ${count} -eq 0 ]]; then
        echo "No .cpp files found."
        exit 0
    fi

    if [[ ${DRY_RUN} -eq 1 ]]; then
        echo "[dry-run] Would touch ${count} .cpp files (full rebuild)"
    else
        touch "${cpps[@]}"
        echo "Touched ${count} .cpp files (full rebuild on next ninja)"
    fi
    exit 0
fi

# --- Stale mode: find .obj files older than PCH ---
if [[ ! -f "${PCH}" ]]; then
    echo "PCH not found: ${PCH}"
    echo "Build the PCH first: ninja build/45410914/pch/system.pch"
    exit 1
fi

pch_mtime=$(stat -c %Y "${PCH}")
stale=0
touched=0

while IFS= read -r -d '' obj; do
    obj_mtime=$(stat -c %Y "${obj}")
    if [[ ${obj_mtime} -lt ${pch_mtime} ]]; then
        # Map .obj path back to .cpp source
        # build/45410914/src/system/foo/Bar.obj -> src/system/foo/Bar.cpp
        rel="${obj#"${BUILD_DIR}/"}"    # src/system/foo/Bar.obj
        cpp="${SRC_DIR}/${rel#src/}"     # full path, still .obj
        cpp="${cpp%.obj}.cpp"

        if [[ -f "${cpp}" ]]; then
            stale=$((stale + 1))
            if [[ ${DRY_RUN} -eq 1 ]]; then
                printf "  stale: %s\n" "${rel}"
            else
                touch "${cpp}"
                touched=$((touched + 1))
            fi
        fi
    fi
done < <(find "${BUILD_DIR}/src" -name "*.obj" -type f -print0 2>/dev/null)

if [[ ${DRY_RUN} -eq 1 ]]; then
    echo "[dry-run] ${stale} stale .obj files (older than PCH)"
else
    if [[ ${touched} -eq 0 ]]; then
        echo "No stale objects found. All .obj files are newer than PCH."
    else
        echo "Touched ${touched} .cpp files. Run 'ninja' to rebuild."
    fi
fi
