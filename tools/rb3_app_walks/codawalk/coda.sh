#!/bin/bash
# coda.sh <K> <bin> <frames> [seed]: big-rock-ending legs on a coda song (seed 18 = ohmygod), expert walk
cd /home/free/tmp/codawalk; K=$1; B=$PWD/$2; F=$3; S=${4:-18}; W=$PWD/walk_x.txt
D="RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=$S RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1"
G="RB3_APP_JOYPAD_KIND=guitar"; R="RB3_APP_JOYPAD_KIND=drums"; C="RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1"
r() { t=$1; shift; BIN=$B SECS=1500 TMO=1530 TAILN=1 ./runb.sh $K$t $W $F $D "$@" > $K$t.out 2>&1 & }
r GC $G $C
r GM $G $C RB3_APP_CHARTPLAY_CODA=mash
r GB $G $C RB3_APP_CHARTPLAY_CODA=blow
r GA $G RB3_APP_AUTOPLAY=1
r GW $G $C RB3_APP_CHARTPLAY_WHAMMY=od RB3_APP_CHARTPLAY_TILT=1
r DM $R $C RB3_APP_CHARTPLAY_CODA=mash
wait; echo coda legs done $K
