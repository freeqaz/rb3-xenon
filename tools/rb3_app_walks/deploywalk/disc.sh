#!/bin/bash
# disc.sh <bin> <suffix> [frames]: short det+autoplay discovery walks with Select presses:
# guitar g, drums d, vocals v (default pad), band b (4 pads); SAVE=1 adds s, the band walk with
# pad 3 (bass, medium) not autoplayed. XENV adds env to every run. 12000 frames by default.
cd /home/free/tmp/deploywalk; B=$PWD/$1; S=$2; F=${3:-12000}
W=$PWD/walk_od.txt
EP="RB3_APP_EXTRA_PADS=guitar=$PWD/p1_od.txt,drums=$PWD/p2_od.txt,guitar=$PWD/p3_od.txt"
DE="RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 ${XENV}"
BIN=$B SECS=300 TMO=340 ./runb.sh g$S $W $F RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 RB3_APP_JOYPAD_KIND=guitar $XENV > g$S.out 2>&1 &
BIN=$B SECS=300 TMO=340 ./runb.sh d$S $W $F RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 RB3_APP_JOYPAD_KIND=drums $XENV > d$S.out 2>&1 &
BIN=$B SECS=300 TMO=340 ./runb.sh v$S $W $F RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 $XENV > v$S.out 2>&1 &
BIN=$B SECS=300 TMO=340 ./runb.sh b$S $W $F RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 $EP $XENV > b$S.out 2>&1 &
if [ -n "$SAVE" ]; then
BIN=$B SECS=300 TMO=340 ./runb.sh s$S $W $F RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 RB3_APP_AUTOPLAY_SKIP=3 "RB3_APP_EXTRA_PADS=guitar=$PWD/p1_od.txt,drums=$PWD/p2_od.txt,guitar=$PWD/p3_save.txt" $XENV > s$S.out 2>&1 &
fi
wait
echo disc done $S
