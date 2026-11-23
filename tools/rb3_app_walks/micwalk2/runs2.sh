#!/bin/bash
# micwalk2 run set 2: Easy vocals (a25 walk, seed 0), bin-2: autoplay, plain sung mic,
# sung mic + tambourine taps, sung mic + freestyle tone. RB3_APP_DET_OD=1 on all.
cd /home/free/tmp/micwalk2; export BIN=${BIN:-/home/free/tmp/micwalk2/bin-2}
export SECS=${SECS:-420} TMO=${TMO:-460}
F=${F:-8400}; W=/home/free/tmp/claude-1000/rb3-scout/a25/walk.txt
E=(RB3_APP_DETERMINISTIC=1 RB3_APP_DET_OD=1)
./runb.sh E0 $W $F "${E[@]}" RB3_APP_AUTOPLAY=1 > E0.out 2>&1 &
./runb.sh ES $W $F "${E[@]}" RB3_APP_SUNG_MIC=1 RB3_APP_SUNG_MIC_LOG=1 > ES.out 2>&1 &
./runb.sh ET $W $F "${E[@]}" RB3_APP_SUNG_MIC=1 RB3_APP_SUNG_MIC_LOG=1 RB3_APP_SUNG_MIC_TAMB=1 > ET.out 2>&1 &
./runb.sh EF $W $F "${E[@]}" RB3_APP_SUNG_MIC=1 RB3_APP_SUNG_MIC_LOG=1 RB3_APP_SUNG_MIC_FREESTYLE=1 > EF.out 2>&1 &
wait
