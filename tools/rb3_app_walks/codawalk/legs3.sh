#!/bin/bash
# legs3.sh <K> <bin>: tilt (15-poll hold) and whammy+tilt legs on the coda song, expert guitar
cd /home/free/tmp/codawalk; K=$1; B=$PWD/$2
D="RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=18 RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1 RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1"
r() { t=$1; shift; BIN=$B SECS=1500 TMO=1530 TAILN=1 ./runb.sh $K$t $PWD/walk_x.txt 7700 $D "$@" > $K$t.out 2>&1 & }
r GT RB3_APP_CHARTPLAY_TILT=1
r GW RB3_APP_CHARTPLAY_WHAMMY=od RB3_APP_CHARTPLAY_TILT=1
r GA RB3_APP_CHARTPLAY_WHAMMY=all
wait; echo legs3 done $K
