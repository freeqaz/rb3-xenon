#!/bin/bash
# cprobe.sh <kind> <script> <seeds...>: song each seed loads at 60 Hz with a converted walk (2400-frame cap)
cd /home/free/tmp/walk60; k=$1; w=$2; shift 2; R=/home/free/tmp/wt-walk60/tools/rb3_app_walks/gate60/runb.sh
for s in "$@"; do BIN=$PWD/bin-w1 SECS=400 TMO=430 TAILN=1 $R cq$k$s $w 2400 RB3_APP_DETERMINISTIC=1 RB3_APP_JOYPAD_KIND=$k RB3_APP_SEED=$s > cq$k$s.out 2>&1 & done
wait
for s in "$@"; do echo "$k seed $s: $(command grep -a -o -E "'songs/[a-z0-9_]+/[a-z0-9_]+\.milo'" r_cq$k$s/run.log | head -1)"; done
