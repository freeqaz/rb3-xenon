#!/bin/bash
# seeds.sh <bin> <seeds...>: which song each RB3_APP_SEED loads (1150-frame cap, walk.txt)
cd /home/free/tmp/codawalk; B=$PWD/$1; shift
for s in "$@"; do
  BIN=$B SECS=300 TMO=330 TAILN=1 ./runb.sh q$s $PWD/walk.txt 1150 RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=$s RB3_APP_JOYPAD_KIND=guitar > q$s.out 2>&1 &
done
wait
for s in "$@"; do echo "seed $s: $(command grep -a -o -E "'songs/[a-z0-9_]+/[a-z0-9_]+\.milo'" r_q$s/run.log | head -1)"; done
