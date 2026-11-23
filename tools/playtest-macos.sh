#!/usr/bin/env bash
# Exercise TU5 Quickplay with chart-driven fret and strum inputs, including misses.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
app="$root/native/build-macos/rb3-app"
disc="${1:-$root/../rb3/orig-assets/xbox-zip}"
tu="${RB3_TITLE_UPDATE_DIR:-$root/orig/45410914/tu5-package}"
out="$(mktemp -d "${TMPDIR:-/tmp}/rb3-xenon-playtest.XXXXXXXX")"
mkdir -p "$out/content"
printf 'Playtest files: %s\n' "$out"
if [[ ! -x "$app" ]]; then "$root/tools/build-macos.sh"; fi
# Canonicalize before rb3-app changes its working directory to the disc.
disc="$(cd "$disc" && pwd)"
tu="$(cd "$tu" && pwd)"
for part in gen/patch_xbox.hdr gen/patch_xbox_0.ark; do
    [[ -s "$tu/$part" ]] || { printf 'Missing TU5 file %s/%s\n' "$tu" "$part" >&2; exit 2; }
done
cd "$root"
RB3_CONTENT_DIR="$out/content" RB3_TITLE_UPDATE_DIR="$tu" \
MILO_HEADLESS=1 RB3_APP_WINDOWED=0 RB3_APP_RUN=1 RB3_APP_MOVIES=0 \
RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=0 RB3_APP_JOYPAD_KIND=guitar \
RB3_APP_AUTOPLAY=0 RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_MISS=10 \
RB3_APP_FRAMES=11900 RB3_APP_SECONDS=600 \
MILO_INPUT_SCRIPT="$root/tools/rb3_app_walks/chartplay/walk.txt" \
MILO_SCREENSHOT_DIR="$out" MILO_SCREENSHOT_FRAMES=3500 \
"$app" "$disc" >"$out/play.log" 2>&1
awk '
/^rb3-app-chartplay: song done:/ && !done {
    for (i=1; i<NF; ++i) {
        if ($i == "gems") gems=$(i+1)
        if ($i == "acted") acted=$(i+1)
        if ($i == "skipped") skipped=$(i+1)
    }
    done=1
}
/^rb3-app-det: frame/ && / screen game_screen / && !done && / p0 / {
    sub(/^.* p0 /, "")
    for (i=1; i<NF; ++i) {
        if ($i == "auto" && $(i+1) != 0) autoplay=1
        if ($i == "score" && $(i+1)>score) score=$(i+1)
        if ($i == "hit" && $(i+1)>hits) hits=$(i+1)
        if ($i == "miss" && $(i+1)>misses) misses=$(i+1)
    }
}
/^rb3-app: frame .*screen=coop_endgame_screen transition=none/ {results=1}
/^rb3-app: frame .*screen=song_select_screen transition=none/ && results {returned=1}
END {
    printf "Guitar: %d hits, %d misses, %d points. Results %d, returned to library %d.\n", hits, misses, score, results, returned
    exit !(done && !autoplay && score>0 && hits>0 && misses>0 && hits==acted && misses==skipped && hits+misses==gems && results && returned)
}' "$out/play.log"
