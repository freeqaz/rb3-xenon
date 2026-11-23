#!/bin/bash
# micwalk2 run set 6 (bin-3, seed 2 Easy: 138 unpitched notes, a25 walk): the song's unpitched notes sung vs silent
# vs autoplay; RB3_APP_DET_HARM=1 for the per-phrase score/cap.
cd /home/free/tmp/micwalk2; export BIN=${BIN:-/home/free/tmp/micwalk2/bin-3}
export SECS=${SECS:-420} TMO=${TMO:-460}
F=${F:-8400}; W=/home/free/tmp/claude-1000/rb3-scout/a25/walk.txt
E=(RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=2 RB3_APP_DET_HARM=1)
./runb.sh TA $W $F "${E[@]}" RB3_APP_AUTOPLAY=1 > TA.out 2>&1 &
./runb.sh T1 $W $F "${E[@]}" RB3_APP_SUNG_MIC=1 RB3_APP_SUNG_MIC_LOG=1 > T1.out 2>&1 &
./runb.sh T0 $W $F "${E[@]}" RB3_APP_SUNG_MIC=1 RB3_APP_SUNG_MIC_LOG=1 RB3_APP_SUNG_MIC_UNPITCHED=0 > T0.out 2>&1 &
wait
