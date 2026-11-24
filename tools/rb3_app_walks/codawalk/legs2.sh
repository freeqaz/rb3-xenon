#!/bin/bash
# legs2.sh <K> <bin>: tilt/whammy leg on the coda song, guitar without knobs (whammy-rest check), and two band legs
cd /home/free/tmp/codawalk; K=$1; B=$PWD/$2
D="RB3_APP_DETERMINISTIC=1 RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1"; G="RB3_APP_JOYPAD_KIND=guitar"
C="RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1"
EP="RB3_APP_EXTRA_PADS=guitar=$PWD/p1_od.txt,drums=$PWD/p2_od.txt,guitar=$PWD/p3_od.txt"
r() { t=$1; w=$2; f=$3; shift 3; BIN=$B SECS=1500 TMO=1530 TAILN=1 ./runb.sh $K$t $PWD/$w $f $D "$@" > $K$t.out 2>&1 & }
r GW walk_x.txt 7700 RB3_APP_SEED=41 $G $C RB3_APP_CHARTPLAY_WHAMMY=od RB3_APP_CHARTPLAY_TILT=1
r GC walk_x.txt 7700 RB3_APP_SEED=41 $G $C
r GT walk_x.txt 7700 RB3_APP_SEED=41 $G $C RB3_APP_CHARTPLAY_TILT=1
r UA walk_od.txt 9500 RB3_APP_AUTOPLAY=1 RB3_APP_AUTODEPLOY=1 $EP
r US walk_od.txt 9500 RB3_APP_AUTOPLAY=1 RB3_APP_AUTODEPLOY=1 RB3_APP_AUTOPLAY_SKIP=3 $EP
r UN walk_od.txt 9500 RB3_APP_AUTOPLAY=1 $EP
wait; echo legs2 done $K
