#!/bin/bash
# runs1.sh <K> <bin> [frames]: part discovery (1-3 downs) on the pro guitar pad, autoplay
cd /home/free/tmp/chartplay3; K=$1; B=$PWD/$2; F=${3:-9000}
D="RB3_APP_DETERMINISTIC=1"; PG="RB3_APP_JOYPAD_KIND=proguitar"
A="RB3_APP_AUTOPLAY=1"; C="RB3_APP_CHARTPLAY=1"; L="RB3_APP_CHARTPLAY_LOG=1"
r() { t=$1; w=$PWD/$2; shift 2; BIN=$B SECS=900 TMO=930 ./runb.sh $K$t $w $F $D "$@" > $K$t.out 2>&1 & }
r B1A walk_pb1.txt $PG $A
r B2A walk_pb2.txt $PG $A
r B3A walk_pb3.txt $PG $A
r GC walk_pg.txt $PG $C $L
wait; echo runs1 done $K
