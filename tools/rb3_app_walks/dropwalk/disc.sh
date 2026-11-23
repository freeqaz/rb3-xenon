#!/bin/bash
# (lane dropwalk: copy of modwalk/disc.sh with per-pad script overrides P1/P2/P3)
# disc.sh <tag> <bin> <walk> [frames] [extra env...]: one deterministic autoplayed 4-pad band walk
# (pad 0 vocals runs <walk>; pads 1-3 guitar/drums/bass run p1-p3.txt; pad 3 is not autoplayed, so
# it misses and can fail the band) with the focus log and the modifier-effect log on.
# Log ~/tmp/dropwalk/r_<tag>/run.log; r_<tag>/screens.txt = the screen/focus/mods trail.
H=/home/free/tmp/wt-dropwalk/tools/rb3_app_walks/dropwalk
cd /home/free/tmp/dropwalk
tag=$1; bin=$2; walk=$3; frames=${4:-20000}; shift 4 2>/dev/null
BIN=$PWD/$bin SECS=${SECS:-540} TMO=${TMO:-580} TAILN=1 $H/runb.sh $tag $H/$walk $frames \
  RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 RB3_APP_AUTOPLAY_SKIP=${SKIP:-3} RB3_APP_FOCUS_LOG=1 RB3_APP_MOD_LOG=1 \
  "RB3_APP_EXTRA_PADS=${EP:-guitar=$H/${P1:-p1.txt},drums=$H/${P2:-p2.txt},guitar=$H/${P3:-p3.txt}}" "$@" > /dev/null
command grep -a -E "^rb3-app: frame|^FAIL|UNREACHED|RESULT|rc=|runtime error|ERROR: AddressSanitizer|Assertion|abort" r_$tag/run.log | command grep -a -v "^rb3-app-det" > r_$tag/screens.txt
echo "$tag: $(tail -1 r_$tag/run.log) FAIL=$(command grep -a -c '^FAIL' r_$tag/run.log) UNREACHED=$(command grep -a -c UNREACHED r_$tag/run.log) last=$(command grep -a 'screen=' r_$tag/run.log | tail -1)"
