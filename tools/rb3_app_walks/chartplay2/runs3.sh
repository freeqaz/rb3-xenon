#!/bin/bash
# runs3.sh <K> <bin> [frames]: pro guitar after the ghost-string fix
cd /home/free/tmp/chartplay2; K=$1; B=$PWD/$2; F=${3:-16000}
D="RB3_APP_DETERMINISTIC=1"; PG="RB3_APP_JOYPAD_KIND=proguitar"
A="RB3_APP_AUTOPLAY=1"; C="RB3_APP_CHARTPLAY=1"; L="RB3_APP_CHARTPLAY_LOG=1"
r() { t=$1; w=$PWD/$2; shift 2; BIN=$B SECS=900 TMO=930 ./runb.sh $K$t $w $F $D "$@" > $K$t.out 2>&1 & }
r GA walk_pg.txt $PG $A
r GC walk_pg.txt $PG $C $L
r GN walk_pg.txt $PG $C $L RB3_APP_CHARTPLAY_OFFSET_MS=-80
r XGC walk_pgx.txt $PG $C $L
r XGH walk_pgx.txt $PG $C $L RB3_APP_CHARTPLAY_HOPO=tap
r XGM walk_pgx.txt $PG $C $L RB3_APP_CHARTPLAY_MISS=10
wait; echo runs3 done $K
