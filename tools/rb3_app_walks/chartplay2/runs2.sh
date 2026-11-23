#!/bin/bash
# runs2.sh <K> <bin> [frames]: full song 1 + perturbation legs (keys K, pro keys P, pro guitar G)
cd /home/free/tmp/chartplay2; K=$1; B=$PWD/$2; F=${3:-16000}
D="RB3_APP_DETERMINISTIC=1"; KK="RB3_APP_JOYPAD_KIND=keys"; PG="RB3_APP_JOYPAD_KIND=proguitar"
A="RB3_APP_AUTOPLAY=1"; C="RB3_APP_CHARTPLAY=1"; L="RB3_APP_CHARTPLAY_LOG=1"
r() { t=$1; w=$PWD/$2; shift 2; BIN=$B SECS=900 TMO=930 ./runb.sh $K$t $w $F $D "$@" > $K$t.out 2>&1 & }
r KA walk.txt $KK $A
r KC walk.txt $KK $C $L
r KM walk.txt $KK $C $L RB3_APP_CHARTPLAY_MISS=10
r KP walk.txt $KK $C $L RB3_APP_CHARTPLAY_OFFSET_MS=80
r PA walk_k1.txt $KK $A
r PC walk_k1.txt $KK $C $L
r PM walk_k1.txt $KK $C $L RB3_APP_CHARTPLAY_MISS=10
r PP walk_k1.txt $KK $C $L RB3_APP_CHARTPLAY_OFFSET_MS=80
r PS walk_k1.txt $KK $C $L RB3_APP_CHARTPLAY_SUSTAIN=0.5
r GM walk_pg.txt $PG $C $L RB3_APP_CHARTPLAY_MISS=10
r GP walk_pg.txt $PG $C $L RB3_APP_CHARTPLAY_OFFSET_MS=80
r GN walk_pg.txt $PG $C $L RB3_APP_CHARTPLAY_OFFSET_MS=-80
r GH walk_pg.txt $PG $C $L RB3_APP_CHARTPLAY_HOPO=tap
wait
r XKA walk_x.txt $KK $A
r XKC walk_x.txt $KK $C $L
r XPA walk_k1x.txt $KK $A
r XPC walk_k1x.txt $KK $C $L
r XGA walk_pgx.txt $PG $A
r XGC walk_pgx.txt $PG $C $L
wait; echo runs2 done $K
