#!/bin/bash
# STALE SEED since main 9d0a07794 (det RNG moved by 8786e09b5 + 9d0a07794): seed 41 now loads iloverocknroll, and ohmygod is not among guitar seeds 0-63 (walk60b/seeds60.txt); pick another coda song before reusing these legs.
# legs3.sh <K> <bin>: tilt (15-poll hold) and whammy+tilt legs on the coda song, expert guitar
cd /home/free/tmp/codawalk; K=$1; B=$PWD/$2
D="RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=41 RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1 RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1"
r() { t=$1; shift; BIN=$B SECS=1500 TMO=1530 TAILN=1 ./runb.sh $K$t $PWD/walk_x.txt 7700 $D "$@" > $K$t.out 2>&1 & }
r GT RB3_APP_CHARTPLAY_TILT=1
r GW RB3_APP_CHARTPLAY_WHAMMY=od RB3_APP_CHARTPLAY_TILT=1
r GA RB3_APP_CHARTPLAY_WHAMMY=all
wait; echo legs3 done $K
