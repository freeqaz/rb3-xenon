#!/bin/bash
# runs2.sh <K> <bin> [frames]: pro bass, pro guitar mute/slide/bend, pro keys deploy/hands (seed 0)
cd /home/free/tmp/chartplay3; K=$1; B=$PWD/$2; F=${3:-10000}; SET=${SET:-1}
D="RB3_APP_DETERMINISTIC=1"; PG="RB3_APP_JOYPAD_KIND=proguitar"; KY="RB3_APP_JOYPAD_KIND=keys"
A="RB3_APP_AUTOPLAY=1"; C="RB3_APP_CHARTPLAY=1"; L="RB3_APP_CHARTPLAY_LOG=1"
r() { t=$1; w=$PWD/$2; shift 2; BIN=$B SECS=900 TMO=930 ./runb.sh $K$t $w $F $D "$@" > $K$t.out 2>&1 & }
if [ $SET = 1 ]; then
r BC walk_pb1.txt $PG $C $L
r XBA walk_pb1x.txt $PG $A
r XBC walk_pb1x.txt $PG $C $L
r XGC walk_pgx.txt $PG $C $L
r XGA walk_pgx.txt $PG $A
r XGMo walk_pgx.txt $PG $C $L RB3_APP_CHARTPLAY_MUTE=open
r XGMa walk_pgx.txt $PG $C $L RB3_APP_CHARTPLAY_MUTE=openall
r XGSo walk_pgx.txt $PG $C $L RB3_APP_CHARTPLAY_SLIDE=on
elif [ $SET = 3 ]; then
S3="RB3_APP_SEED=3"
r 3XGA walk_pgx.txt $PG $A $S3
r 3XGC walk_pgx.txt $PG $C $L $S3
r 3XGMo walk_pgx.txt $PG $C $L $S3 RB3_APP_CHARTPLAY_MUTE=open
r 3XGSo walk_pgx.txt $PG $C $L $S3 RB3_APP_CHARTPLAY_SLIDE=on
r XGMk walk_pgx.txt $PG $C $L RB3_APP_CHARTPLAY_MUTE=keep
r XPDs walk_k1x.txt $KY $C $L RB3_APP_CHARTPLAY_DEPLOY=stomp
else
r XGSa walk_pgx.txt $PG $C $L RB3_APP_CHARTPLAY_SLIDE=all
r XGB walk_pgx.txt $PG $C $L RB3_APP_CHARTPLAY_BEND=1
r XBSo walk_pb1x.txt $PG $C $L RB3_APP_CHARTPLAY_SLIDE=on
r XBSa walk_pb1x.txt $PG $C $L RB3_APP_CHARTPLAY_SLIDE=all
r XBMo walk_pb1x.txt $PG $C $L RB3_APP_CHARTPLAY_MUTE=open
r XPC walk_k1x.txt $KY $C $L
r XPDp walk_k1x.txt $KY $C $L RB3_APP_CHARTPLAY_DEPLOY=pedal
r XPDb walk_k1x.txt $KY $C $L RB3_APP_CHARTPLAY_DEPLOY=button
r XPH walk_k1x.txt $KY $C $L RB3_APP_CHARTPLAY_HANDS=1
fi
wait; echo runs2 done $K set $SET
# set 3 (appended): seed 3 (coldasice, PART REAL_GUITAR_22): the 22-fret track on the 22-fret pad
