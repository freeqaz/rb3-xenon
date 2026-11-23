#!/bin/bash
# seeds.sh <bin> <seed...>: which song song_select picks for each RB3_APP_SEED (pro guitar pad, 1500 frames)
cd /home/free/tmp/chartplay3; B=$PWD/$1; shift
for s in "$@"; do
  BIN=$B SECS=300 TMO=330 ./runb.sh s$s $PWD/walk_pg.txt 1500 RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=$s RB3_APP_JOYPAD_KIND=proguitar RB3_APP_AUTOPLAY=1 > s$s.out 2>&1 &
done; wait
for s in "$@"; do echo "seed $s: $(command grep -a -o "PreloadPanel::Load[^']*'[a-z0-9]*'" r_s$s/run.log | head -1 | sed "s/.*'\(.*\)'/\1/") $(tail -1 r_s$s/run.log)"; done
