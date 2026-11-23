#!/bin/bash
# runs_h.sh <bin> <tag> [frames] [prefix-env...]: the harmony run set -- v0/v1: solo harmony
# singer (seed 0: 3 parts, seed 1: 2 parts); b0: the harmony singer plus guitar, drums and bass
# pads (bandwalk/deploywalk scripts) with RB3_APP_AUTODEPLOY, seed 0. All det + autoplay +
# DET_OD + DET_HARM. XENV adds env (e.g. malloc perturb).
cd /home/free/tmp/harmwalk; B=$PWD/$1; T=$2; F=${3:-12000}
W=$PWD/walk_harm.txt
EP="RB3_APP_EXTRA_PADS=guitar=$PWD/p1_od.txt,drums=$PWD/p2_od.txt,guitar=$PWD/p3_od.txt"
C="RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 RB3_APP_DET_OD=1 RB3_APP_DET_HARM=1"
BIN=$B SECS=520 TMO=580 ./runb.sh v0$T $W $F $C RB3_APP_SEED=0 $XENV > v0$T.out 2>&1 &
BIN=$B SECS=520 TMO=580 ./runb.sh v1$T $W $F $C RB3_APP_SEED=1 $XENV > v1$T.out 2>&1 &
BIN=$B SECS=520 TMO=580 ./runb.sh b0$T $W $F $C RB3_APP_SEED=0 RB3_APP_AUTODEPLOY=1 $EP $XENV > b0$T.out 2>&1 &
wait
echo runs_h done $T
