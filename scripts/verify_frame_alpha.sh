#!/bin/bash
# verify_frame_alpha.sh <dir-of-pngs> [glob]: rb3-app / rb3-render readback frames must be opaque.
# Run it on MILO_SCREENSHOT_DIR output (e.g. the China Grove walk shots) in a landing gate.
# Every pixel of every PNG must be opaque (alpha 255). The port's surface is
# CompositeAlphaMode::Opaque, so a frame's alpha is never displayed; a readback
# PNG that carries alpha < 1 is a frame whose alpha channel was left holding a
# render-internal value (the l20 regression: the world-end blit wrote 0).
# Threshold: min alpha must be exactly 1 (255/255). No tolerance: the only
# writers are the composite (1.0) and later draws under SetAlphaBlendMax (max),
# so an opaque frame has no legitimate pixel below 255.
# Scope: that holds for BLENDED draws (kAlphaBlendMax: PipelineManager sets the
# alpha op of every blended mode to Max(1,1), so they cannot lower alpha). An
# OPAQUE (kBlendSrc) draw writes its own output alpha, and since A73 a
# use_environ material's output alpha carries the environ's ambient alpha
# (retail c1.w, NgMat::SetupAmbient fn_824A9090), so an opaque draw under a
# faded environ can legitimately leave alpha < 1 -- as retail's EDRAM would.
# No gated frame does this today (China Grove walk, movie pause walk, placed
# band: all min 1.0). If one starts to, look at what drew there before calling
# it a regression.
# Prints: ALPHA_GATE verdict=PASS|FAIL min=<min over all pixels> mean=<mean over all pixels> n=<pngs>
# Exit 0 PASS, 1 FAIL, 2 no PNGs (vacuous: not a pass), 3 unreadable PNG.
d=$1; glob=${2:-*.png}
[ -d "$d" ] || { echo "ALPHA_GATE verdict=FAIL reason=no-such-dir dir=$d"; exit 2; }
shopt -s nullglob
files=("$d"/$glob)
n=${#files[@]}
[ "$n" -gt 0 ] || { echo "ALPHA_GATE verdict=FAIL reason=no-pngs min=- mean=- n=0 dir=$d"; exit 2; }
out=$(magick identify -format "%[fx:minima.a] %[fx:mean.a] %w %h %f\n" "${files[@]}" 2>&1) || { echo "ALPHA_GATE verdict=FAIL reason=unreadable n=$n dir=$d: $out" | head -3; exit 3; }
echo "$out" | awk -v n="$n" -v d="$d" '
  NF >= 5 { m = $1 + 0; if (k == 0 || m < mn) mn = m; px = $3 * $4; s += $2 * px; tot += px; k++;
            if (m < 1) bad = bad " " $5 "(min=" $1 ",mean=" $2 ")" }
  END { if (k != n) { printf "ALPHA_GATE verdict=FAIL reason=parsed-%d-of-%d n=%d dir=%s\n", k, n, n, d; exit 3 }
        v = (mn >= 1) ? "PASS" : "FAIL";
        printf "ALPHA_GATE verdict=%s min=%.6f mean=%.6f n=%d dir=%s\n", v, mn, s / tot, n, d;
        if (v == "FAIL") { printf "ALPHA_GATE non-opaque:%s\n", bad; exit 1 } }'
