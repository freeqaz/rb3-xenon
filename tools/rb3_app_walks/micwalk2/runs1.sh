#!/bin/bash
# micwalk2 run set 1: Hard and Expert vocals, autoplay vs sung mic (seed 0, det walk).
cd /home/free/tmp/micwalk2; export BIN=${BIN:-/home/free/tmp/micwalk2/bin-1}
export SECS=${SECS:-200} TMO=${TMO:-240}
F=${F:-8400}
./runb.sh HA $PWD/walk_d2.txt $F RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 > HA.out 2>&1 &
./runb.sh HS $PWD/walk_d2.txt $F RB3_APP_DETERMINISTIC=1 RB3_APP_SUNG_MIC=1 RB3_APP_SUNG_MIC_LOG=1 > HS.out 2>&1 &
./runb.sh XA $PWD/walk_d3.txt $F RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 > XA.out 2>&1 &
./runb.sh XS $PWD/walk_d3.txt $F RB3_APP_DETERMINISTIC=1 RB3_APP_SUNG_MIC=1 RB3_APP_SUNG_MIC_LOG=1 > XS.out 2>&1 &
wait
