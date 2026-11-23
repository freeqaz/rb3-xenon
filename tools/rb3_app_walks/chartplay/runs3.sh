#!/bin/bash
# runs3.sh <K> <bin> <frames>: expert legs (walk_x.txt: three DOWNs before the difficulty A)
cd /home/free/tmp/chartplay; K=$1; B=$PWD/$2; F=${3:-7600}; W=$PWD/walk_x.txt
D="RB3_APP_DETERMINISTIC=1"; G="RB3_APP_JOYPAD_KIND=guitar"; R="RB3_APP_JOYPAD_KIND=drums"
r() { t=$1; shift; BIN=$B SECS=1200 TMO=1230 ./runb.sh $K$t $W $F $D "$@" > $K$t.out 2>&1 & }
r XGA $G RB3_APP_AUTOPLAY=1
r XGC $G RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1
r XGH $G RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1 RB3_APP_CHARTPLAY_HOPO=tap
r XDA $R RB3_APP_AUTOPLAY=1
r XDC $R RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1
wait; echo runs3 done $K
