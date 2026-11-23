#!/bin/bash
# checks.sh <K> <bin>: lane framerate rework checks (coordinator, 2026-11-23):
#   N  normal launch, no script, no det (60 Hz paced), 300 frames (movies play: ends on the intro)
#   N0 the same with RB3_APP_MOVIES=0: reaches splash_screen
#   J  RB3_APP_JOYPAD_GATE=1 (a harness run: legacy 30 Hz)
#   D  det walk.txt 6000 frames without RB3_APP_HZ (legacy): compare with the base binary's d0
#   S1/S2  det walk.txt 6000 frames at RB3_APP_HZ=60, twice: compare with each other
cd /home/free/tmp/framerate; K=$1; B=$PWD/$2
r() { t=$1; s=$2; f=$3; shift 3; BIN=$B SECS=600 TMO=630 TAILN=1 ./runb.sh $K$t "$s" $f "$@" > $K$t.out 2>&1 & }
r N "" 300
r N0 "" 300 RB3_APP_MOVIES=0
r J "" 300 RB3_APP_JOYPAD_GATE=1
r D $PWD/walk.txt 6000 RB3_APP_DETERMINISTIC=1
r S1 $PWD/walk.txt 6000 RB3_APP_DETERMINISTIC=1 RB3_APP_HZ=60
r S2 $PWD/walk.txt 6000 RB3_APP_DETERMINISTIC=1 RB3_APP_HZ=60
wait; echo checks done $K
