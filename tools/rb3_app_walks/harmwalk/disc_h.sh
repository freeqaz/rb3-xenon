#!/bin/bash
# disc_h.sh <bin> <suffix> [frames] [seeds]: harmony discovery walks (det + autoplay + DET_OD +
# DET_HARM) on walk_harm.txt (pad 0 picks overshell_vocal_harmony) for each seed. XENV adds env.
cd /home/free/tmp/harmwalk; B=$PWD/$1; S=$2; F=${3:-3000}; SEEDS=${4:-"0 1 2 3 4 5"}
W=$PWD/walk_harm.txt
for sd in $SEEDS; do
BIN=$B SECS=300 TMO=340 ./runb.sh h$S$sd $W $F RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=$sd RB3_APP_AUTOPLAY=1 RB3_APP_DET_OD=1 RB3_APP_DET_HARM=1 $XENV > h$S$sd.out 2>&1 &
done
wait
echo disc_h done $S
