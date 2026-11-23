#!/bin/bash
# runs.sh <K> <bin> [frames]: keys / pro keys / pro guitar legs, det, autoplay vs chartplay
cd /home/free/tmp/chartplay2; K=$1; B=$PWD/$2; F=${3:-9000}
D="RB3_APP_DETERMINISTIC=1"; KK="RB3_APP_JOYPAD_KIND=keys"; PG="RB3_APP_JOYPAD_KIND=proguitar"
A="RB3_APP_AUTOPLAY=1"; C1="RB3_APP_CHARTPLAY=1"; C2="RB3_APP_CHARTPLAY_LOG=1"
r() { t=$1; w=$PWD/$2; shift 2; BIN=$B SECS=900 TMO=930 ./runb.sh $K$t $w $F $D "$@" > $K$t.out 2>&1 & }
r KA walk.txt $KK $A
r KC walk.txt $KK $C1 $C2
r P1A walk_k1.txt $KK $A
r P1C walk_k1.txt $KK $C1 $C2
r P2A walk_k2.txt $KK $A
r P2C walk_k2.txt $KK $C1 $C2
r GA walk_pg.txt $PG $A
r GC walk_pg.txt $PG $C1 $C2
wait; echo runs done $K
