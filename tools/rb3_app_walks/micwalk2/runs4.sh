#!/bin/bash
# micwalk2 run set 4: seed sweep 1..6 to 2,200 frames (sung mic), for the `rb3-app-mic: song` line.
cd /home/free/tmp/micwalk2; export BIN=${BIN:-/home/free/tmp/micwalk2/bin-2}
W=/home/free/tmp/claude-1000/rb3-scout/a25/walk.txt
for s in 1 2 3 4 5 6; do
  SECS=200 TMO=240 ./runb.sh SW$s $W 2200 RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=$s RB3_APP_SUNG_MIC=1 > SW$s.out 2>&1 &
done
wait
