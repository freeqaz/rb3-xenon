#!/usr/bin/env bash
#
# Measure incremental decomp progress between a baseline commit and HEAD.
#
# Uses a git worktree to build the baseline report, then compares it
# against the main repo's current report using compare_progress.py.
#
# Usage:
#   scripts/measure_progress.sh                    # Compare HEAD vs HEAD~1
#   scripts/measure_progress.sh dd02a3e            # Compare HEAD vs specific commit
#   scripts/measure_progress.sh --worktree /path   # Use existing worktree dir
#   scripts/measure_progress.sh --detailed HEAD~5  # Show per-unit breakdown
#   scripts/measure_progress.sh --functions c8d98a # Show function-level changes
#   scripts/measure_progress.sh --regressions      # Only show regressions
#   scripts/measure_progress.sh --current-dir /path/to/worktree HEAD  # Use worktree as "current"
#
set -euo pipefail

MAIN_REPO="$(cd "$(dirname "$0")/.." && pwd)"
REPORT_REL="build/45410914/report.json"
BASELINE_REF="HEAD~1"
COMPARE_FLAGS=()
WORKTREE_DIR="/tmp/claude/measure-progress"
CREATED_WORKTREE=0
CURRENT_DIR=""

# --- Parse arguments ---
while [[ $# -gt 0 ]]; do
    case "$1" in
        --worktree)
            WORKTREE_DIR="$2"
            shift 2
            ;;
        --detailed)
            COMPARE_FLAGS+=("--detailed")
            shift
            ;;
        --functions|-f)
            COMPARE_FLAGS+=("--functions")
            shift
            ;;
        --regressions|-r)
            COMPARE_FLAGS+=("--regressions")
            shift
            ;;
        --current-dir)
            CURRENT_DIR="$2"
            shift 2
            ;;
        --limit)
            COMPARE_FLAGS+=("--limit" "$2")
            shift 2
            ;;
        --help|-h)
            sed -n '2,14p' "$0"
            exit 0
            ;;
        *)
            BASELINE_REF="$1"
            shift
            ;;
    esac
done

WORKTREE="${WORKTREE_DIR}"
CACHE_DIR="${MAIN_REPO}/build/45410914/baselines"

# --- Resolve current directory (main repo or worktree) ---
if [[ -n "${CURRENT_DIR}" ]]; then
    CURRENT_DIR="$(cd "${CURRENT_DIR}" && pwd)"
    if [[ ! -f "${CURRENT_DIR}/${REPORT_REL}" ]]; then
        echo "Current report not found in worktree, building..."
        ninja -C "${CURRENT_DIR}" "${REPORT_REL}" -j"$(nproc)" 2>&1 | tail -1
    fi
    CURRENT_REPORT="${CURRENT_DIR}/${REPORT_REL}"
    CURRENT_LABEL="worktree:$(basename "${CURRENT_DIR}")"
else
    CURRENT_DIR="${MAIN_REPO}"
    CURRENT_REPORT="${MAIN_REPO}/${REPORT_REL}"
    CURRENT_LABEL="working tree"
fi

# --- Verify prerequisites ---
if [[ ! -f "${CURRENT_REPORT}" ]]; then
    echo "Error: Current report not found: ${CURRENT_REPORT}"
    echo "Run 'ninja' first."
    exit 1
fi

if [[ ! -d "${MAIN_REPO}/orig/45410914" ]]; then
    echo "Error: orig/ binaries not found in main repo."
    exit 1
fi

# Resolve the baseline ref to an actual commit hash
BASELINE_COMMIT=$(git -C "${MAIN_REPO}" rev-parse "${BASELINE_REF}")
BASELINE_SHORT=$(git -C "${MAIN_REPO}" rev-parse --short "${BASELINE_COMMIT}")
CURRENT_SHORT="${CURRENT_LABEL}"

echo "Measuring progress: ${BASELINE_SHORT} (baseline) -> ${CURRENT_SHORT} (current)"

# --- Check baseline cache ---
CACHED_REPORT="${CACHE_DIR}/${BASELINE_COMMIT}.json"
if [[ -f "${CACHED_REPORT}" ]]; then
    echo "Using cached baseline report for ${BASELINE_SHORT}"
    BASELINE_REPORT="${CACHED_REPORT}"
else
    echo "No cached baseline for ${BASELINE_SHORT}, building..."
    echo "Using worktree: ${WORKTREE}"

    # --- Create worktree if it doesn't exist ---
    if [[ ! -d "${WORKTREE}" ]]; then
        echo "Creating worktree at ${WORKTREE}..."
        git -C "${MAIN_REPO}" worktree add --detach "${WORKTREE}" HEAD --quiet
        CREATED_WORKTREE=1
    fi

    # --- Save worktree state for restoration ---
    ORIGINAL_COMMIT=$(git -C "${WORKTREE}" rev-parse HEAD)

    cleanup() {
        echo ""
        if [[ "${CREATED_WORKTREE}" -eq 1 ]]; then
            echo "Removing temporary worktree..."
            git -C "${MAIN_REPO}" worktree remove --force "${WORKTREE}" 2>/dev/null || true
        else
            echo "Restoring worktree to ${ORIGINAL_COMMIT:0:7}..."
            git -C "${WORKTREE}" reset --hard --quiet "${ORIGINAL_COMMIT}" 2>/dev/null || true
        fi
    }
    trap cleanup EXIT

    # --- Reset worktree to baseline commit ---
    echo "Resetting worktree to baseline ${BASELINE_SHORT}..."
    git -C "${WORKTREE}" reset --hard --quiet "${BASELINE_COMMIT}"

    # Clean untracked source files but preserve build artifacts and symlinks
    git -C "${WORKTREE}" clean -fd \
        --exclude=build/ \
        --exclude=bin/ \
        --exclude=orig \
        --exclude=scripts \
        --exclude=compile_commands.json \
        --exclude=decomp.db \
        --exclude=objdiff.json \
        --exclude=build.ninja \
        --quiet 2>/dev/null || true

    # --- Ensure orig/ symlink (replace if not already a symlink to main repo) ---
    if [[ ! -L "${WORKTREE}/orig" || "$(readlink "${WORKTREE}/orig")" != "${MAIN_REPO}/orig" ]]; then
        rm -rf "${WORKTREE}/orig"
        ln -sf "${MAIN_REPO}/orig" "${WORKTREE}/orig"
        echo "Restored orig/ symlink"
    fi

    # --- Ensure scripts symlink ---
    if [[ ! -L "${WORKTREE}/scripts" || "$(readlink "${WORKTREE}/scripts")" != "${MAIN_REPO}/scripts" ]]; then
        rm -rf "${WORKTREE}/scripts"
        ln -sf "${MAIN_REPO}/scripts" "${WORKTREE}/scripts"
        echo "Restored scripts/ symlink"
    fi

    # --- Ensure build tools and compilers are available (avoid downloads) ---
    mkdir -p "${WORKTREE}/build/tools" "${WORKTREE}/build/45410914/pch"
    # Pre-create empty PCH file — WIBO_FS_CACHE=1 breaks creating new files in
    # case-insensitive path components (45410914). cl.exe can overwrite existing files fine.
    touch "${WORKTREE}/build/45410914/pch/system.pch"
    for tool in "${MAIN_REPO}/build/tools"/*; do
        dest="${WORKTREE}/build/tools/$(basename "$tool")"
        [[ -e "$dest" ]] || ln -sf "$tool" "$dest"
    done
    if [[ -d "${MAIN_REPO}/build/compilers" && ! -d "${WORKTREE}/build/compilers" ]]; then
        ln -sf "${MAIN_REPO}/build/compilers" "${WORKTREE}/build/compilers"
    fi
    # Symlink binutils if present
    if [[ -d "${MAIN_REPO}/build/binutils" && ! -d "${WORKTREE}/build/binutils" ]]; then
        ln -sf "${MAIN_REPO}/build/binutils" "${WORKTREE}/build/binutils"
    fi

    # --- Extract configure args from main repo (resolve relative paths to absolute) ---
    CONFIGURE_ARGS=()
    if [[ -f "${MAIN_REPO}/build.ninja" ]]; then
        # Read configure_args, joining continuation lines
        raw_args=$(sed -n '/^configure_args/{ :a; /\$$/{ N; s/\$\n\s*/ /; ba }; s/^configure_args = //; p }' \
            "${MAIN_REPO}/build.ninja")
        # Resolve relative paths to absolute (relative to MAIN_REPO)
        for arg in $raw_args; do
            if [[ "$arg" == --* ]]; then
                CONFIGURE_ARGS+=("$arg")
            elif [[ "$arg" == ../* || "$arg" == ./* ]]; then
                CONFIGURE_ARGS+=("$(cd "${MAIN_REPO}" && realpath "$arg")")
            else
                CONFIGURE_ARGS+=("$arg")
            fi
        done
    fi

    # --- Resolve tool paths from main repo's build.ninja to absolute ---
    # configure.py defaults to relative paths (../jeff/..., ../wibo/..., etc.)
    # which break in worktrees outside the source tree
    resolve_tool() {
        local rel_path="$1"
        local abs_path
        abs_path="$(cd "${MAIN_REPO}" && realpath -e "${rel_path}" 2>/dev/null)" || return 1
        echo "${abs_path}"
    }

    # Extract tool paths used in the main build and pass them explicitly
    for tool_flag_pair in \
        "--dtk:../jeff/target/release/dtk" \
        "--objdiff:../objdiff/target/release/objdiff-cli" \
        "--wrapper:../wibo/build/release/wibo"; do
        flag="${tool_flag_pair%%:*}"
        rel="${tool_flag_pair#*:}"
        abs="$(resolve_tool "${rel}")" && CONFIGURE_ARGS+=("${flag}" "${abs}")
    done

    echo "Using configure args: ${CONFIGURE_ARGS[*]}"

    # Extract dtk path from configure args so we can run the split step directly.
    # This avoids a misleading ninja "manifest still dirty" loop when the split
    # fails and build/45410914/config.json is never produced.
    DTK_BIN=""
    for ((i = 0; i < ${#CONFIGURE_ARGS[@]}; i++)); do
        if [[ "${CONFIGURE_ARGS[$i]}" == "--dtk" && $((i + 1)) -lt ${#CONFIGURE_ARGS[@]} ]]; then
            DTK_BIN="${CONFIGURE_ARGS[$((i + 1))]}"
            break
        fi
    done

    # --- Generate split config explicitly (clear error path if dtk fails) ---
    if [[ -n "${DTK_BIN}" && -x "${DTK_BIN}" ]]; then
        echo "Generating baseline split config (dtk xex split)..."
        SPLIT_LOG="$(mktemp -t measure_progress_split.XXXXXX.log)"
        if ! (cd "${WORKTREE}" && "${DTK_BIN}" xex split config/45410914/config.yml build/45410914) \
            >"${SPLIT_LOG}" 2>&1; then
            echo "Error: Failed to generate baseline split config with dtk:"
            echo "  ${DTK_BIN} xex split config/45410914/config.yml build/45410914"
            echo ""
            tail -100 "${SPLIT_LOG}" || true
            echo ""
            echo "Hint: the selected baseline may require a different dtk version or a cached baseline report."
            exit 1
        fi
        rm -f "${SPLIT_LOG}" 2>/dev/null || true
    fi

    # --- Reconfigure for baseline's file set ---
    echo "Reconfiguring baseline..."
    (cd "${WORKTREE}" && python3 configure.py "${CONFIGURE_ARGS[@]}") >/dev/null

    # Ninja can loop on "manifest 'build.ninja' still dirty" when the reused
    # worktree/build artifacts have coarse or future mtimes (common with cached
    # build dirs in /tmp worktrees). Normalize generator deps, then bump the
    # generated manifest outputs to a strictly newer timestamp.
    normalize_manifest_timestamps() {
        local deps=(
            "${WORKTREE}/build/45410914/config.json"
            "${WORKTREE}/configure.py"
            "${WORKTREE}/tools/project.py"
            "${WORKTREE}/tools/ninja_syntax.py"
            "${WORKTREE}/config/45410914/config.json"
            "${WORKTREE}/config/45410914/objects.json"
            "${WORKTREE}/config/45410914/link_order.txt"
        )
        local touched_any=0
        for dep in "${deps[@]}"; do
            if [[ -e "${dep}" ]]; then
                touch "${dep}" 2>/dev/null || true
                touched_any=1
            fi
        done
        if [[ "${touched_any}" -eq 1 ]]; then
            # Ensure build.ninja/objdiff.json are newer than all configure deps.
            sleep 1
        fi
        touch "${WORKTREE}/build.ninja" "${WORKTREE}/objdiff.json" 2>/dev/null || true
    }
    normalize_manifest_timestamps

    # --- Build baseline report ---
    echo "Building baseline report (this may take a moment)..."
    BUILD_LOG="$(mktemp -t measure_progress_ninja.XXXXXX.log)"
    if ninja -C "${WORKTREE}" "${REPORT_REL}" -j"$(nproc)" >"${BUILD_LOG}" 2>&1; then
        tail -1 "${BUILD_LOG}" || true
    else
        tail -100 "${BUILD_LOG}" || true
        if grep -q "manifest 'build.ninja' still dirty" "${BUILD_LOG}" && \
           grep -q "output build/45410914/config.json doesn't exist" "${BUILD_LOG}"; then
            echo ""
            echo "Hint: ninja's manifest-dirty loop is usually a secondary symptom."
            echo "      The baseline split step failed, so build/45410914/config.json was never created."
        fi
        exit 1
    fi
    rm -f "${BUILD_LOG}" 2>/dev/null || true

    if [[ ! -f "${WORKTREE}/${REPORT_REL}" ]]; then
        echo "Error: Baseline report was not generated."
        exit 1
    fi

    # --- Cache the baseline report ---
    mkdir -p "${CACHE_DIR}"
    cp "${WORKTREE}/${REPORT_REL}" "${CACHED_REPORT}"
    echo "Cached baseline report -> ${CACHED_REPORT}"

    BASELINE_REPORT="${WORKTREE}/${REPORT_REL}"
fi

# --- Compare ---
echo ""
python3 "${MAIN_REPO}/scripts/analysis/compare_progress.py" \
    "${COMPARE_FLAGS[@]}" \
    "${BASELINE_REPORT}" \
    "${CURRENT_REPORT}"
