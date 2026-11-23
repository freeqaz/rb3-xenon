#!/bin/bash
# lcprobe.sh <kind> <seeds...>: song each seed loads in the LEGACY chartplay walk_x (1100-frame cap)
cd /home/free/tmp/walk60; k=$1; shift; R=/home/free/tmp/wt-walk60/tools/rb3_app_walks/gate60/runb.sh
for s in "$@"; do HZ=legacy BIN=$PWD/bin-w1 SECS=300 TMO=330 TAILN=1 $R lc$k$s /home/free/tmp/walk60/legacy/chartplay/walk_x.txt 1100 RB3_APP_DETERMINISTIC=1 RB3_APP_JOYPAD_KIND=$k RB3_APP_SEED=$s > lc$k$s.out 2>&1 & done
wait
for s in "$@"; do echo "$k seed $s: $(command grep -a -o -E "'songs/[a-z0-9_]+/[a-z0-9_]+\.milo'" r_lc$k$s/run.log | head -1)"; done
