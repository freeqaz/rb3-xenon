#!/bin/bash
# survey.sh <K> <bin> <frames> <seeds...>: expert guitar chartplay, bonus fields, one run per seed
# walk60b (60 Hz): the record's survey seeds 1 llama, 2 beastandtheharlot, 3 crazytrain, 5 thelook, 6 midlifecrisis
# are 60 Hz seeds 6, 8, 55, 13, 26 (walk_x guitar table: ../walk60b/seeds60.txt)
cd /home/free/tmp/codawalk; K=$1; B=$PWD/$2; F=$3; shift 3; W=$PWD/walk_x.txt
for s in "$@"; do
  BIN=$B SECS=1500 TMO=1530 ./runb.sh ${K}s$s $W $F RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=$s RB3_APP_JOYPAD_KIND=guitar \
    RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1 RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1 > ${K}s$s.out 2>&1 &
done
wait; echo survey done $K
