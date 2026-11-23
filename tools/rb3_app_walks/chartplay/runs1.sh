#!/bin/bash
# runs1.sh <K> <bin> <frames>: guitar/drums autoplay vs chartplay, det, walk.txt (gemwalk's a25 copy)
cd /home/free/tmp/chartplay; K=$1; B=$PWD/$2; F=${3:-11000}; W=$PWD/walk.txt
D="RB3_APP_DETERMINISTIC=1"
BIN=$B SECS=400 TMO=430 ./runb.sh ${K}GA $W $F $D RB3_APP_JOYPAD_KIND=guitar RB3_APP_AUTOPLAY=1 > ${K}GA.out 2>&1 &
BIN=$B SECS=400 TMO=430 ./runb.sh ${K}GC $W $F $D RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1 > ${K}GC.out 2>&1 &
BIN=$B SECS=400 TMO=430 ./runb.sh ${K}DA $W $F $D RB3_APP_JOYPAD_KIND=drums RB3_APP_AUTOPLAY=1 > ${K}DA.out 2>&1 &
BIN=$B SECS=400 TMO=430 ./runb.sh ${K}DC $W $F $D RB3_APP_JOYPAD_KIND=drums RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1 > ${K}DC.out 2>&1 &
wait; echo runs1 done $K
