#!/bin/bash
# Rebase a wave worktree branch onto main, auto-resolving the union files
# (scripts/target_symbol_map.json + config/45410914/objects.json as dict-unions,
#  config/45410914/splits.txt as a line-union) so independent lanes compose.
#
# Usage:
#   scripts/harvest/land.sh <worktree-path>      # e.g. /home/free/code/milohax/wt-w13-gapA-bisect-port
#   scripts/harvest/land.sh <branch-name>        # e.g. w13-gapA-bisect-port (its worktree is looked up)
#
# Prints:
#   READY:<branch>            -> rebased clean (or via union-resolve); caller does `git merge --ff-only`
#   DEFER:<branch> <reason>   -> cascade / non-union conflict; rebase aborted, branch untouched
#
# After READY for ALL lanes: run the splits overlap self-check, then the composed
# verify (rm stamp + touch config.yml + fresh_report.sh). See the wave-loop SOP.
set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"

arg="${1:?usage: land.sh <worktree-path|branch-name>}"
if [ -d "$arg" ]; then
  W="$(cd "$arg" && pwd)"
else
  # treat as branch name; find its registered worktree
  W="$(git -C "$REPO" worktree list --porcelain | awk -v b="refs/heads/$arg" '
       $1=="worktree"{wt=$2} $1=="branch" && $2==b {print wt; exit}')"
  if [ -z "$W" ]; then echo "DEFER:$arg no-worktree-for-branch"; exit 2; fi
fi
b="$(git -C "$W" rev-parse --abbrev-ref HEAD)"
cd "$REPO"

git -C "$W" rebase --abort 2>/dev/null
out=$(git -C "$W" rebase main 2>&1); tries=0
while echo "$out" | grep -qi "conflict\|could not apply"; do
  tries=$((tries+1))
  if [ $tries -gt 5 ]; then git -C "$W" rebase --abort 2>/dev/null; echo "DEFER:$b cascade"; exit 2; fi
  for f in scripts/target_symbol_map.json config/45410914/objects.json; do
    git -C "$W" status --short "$f" 2>/dev/null | grep -q "^UU" && {
      python3 "$HERE/resolve_json_union.py" "$W" "$f" >/dev/null 2>&1 && git -C "$W" add "$f" \
        || { git -C "$W" rebase --abort; echo "DEFER:$b $f"; exit 2; }; }
  done
  git -C "$W" status --short config/45410914/splits.txt 2>/dev/null | grep -q "^UU" && {
    python3 "$HERE/resolve_splits_union.py" "$W" >/dev/null 2>&1 && git -C "$W" add config/45410914/splits.txt; }
  git -C "$W" status --short 2>/dev/null | grep -q "^UU\|^AA" && {
    git -C "$W" rebase --abort 2>/dev/null
    echo "DEFER:$b nonjson:$(git -C "$W" status --short | grep '^UU\|^AA' | awk '{print $2}' | tr '\n' ' ')"
    exit 2; }
  out=$(GIT_EDITOR=true git -C "$W" rebase --continue 2>&1)
done
echo "READY:$b"
