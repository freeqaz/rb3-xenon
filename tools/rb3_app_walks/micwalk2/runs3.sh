#!/bin/bash
# micwalk2 run set 3 (bin-2): harmony walk autoplay vs three sung mics (seed 0), plus a short
# seed sweep (1..4, sung mic) to find a song with unpitched notes (the `rb3-app-mic: song` line).
cd /home/free/tmp/micwalk2; export BIN=${BIN:-/home/free/tmp/micwalk2/bin-2}
export SECS=${SECS:-420} TMO=${TMO:-460}
F=${F:-8400}; W=/home/free/tmp/claude-1000/rb3-scout/a25/walk.txt
E=(RB3_APP_DETERMINISTIC=1 RB3_APP_DET_HARM=1 RB3_APP_DET_OD=1)
./runb.sh HMA $PWD/walk_harm.txt $F "${E[@]}" RB3_APP_AUTOPLAY=1 > HMA.out 2>&1 &
./runb.sh HM3 $PWD/walk_harm.txt $F "${E[@]}" RB3_APP_SUNG_MIC=1 RB3_APP_SUNG_MIC_COUNT=3 RB3_APP_SUNG_MIC_TAMB=1 RB3_APP_SUNG_MIC_LOG=1 > HM3.out 2>&1 &
for s in 1 2 3 4; do
  SECS=150 TMO=190 ./runb.sh SW$s $W 1300 RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=$s RB3_APP_SUNG_MIC=1 > SW$s.out 2>&1 &
done
wait
