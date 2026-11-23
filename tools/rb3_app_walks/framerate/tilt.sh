#!/bin/bash
# tilt.sh <K> <bin>: lane framerate tilt legs on ohmygod (seed 18), expert guitar by chartplay.
# Each tilt is held a fixed song time (RB3_APP_CHARTPLAY_TILT_MS), so the input is the same at
# either frame rate; the TU5 any_sign_filter needs num_frames 12 consecutive polls >= 0.93.
# Legs: L* = RB3_APP_HZ=legacy (1/30 s frames, the pre-framerate loop), F* = default 60 Hz.
cd /home/free/tmp/framerate; K=$1; B=$PWD/$2
D="RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=18 RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1 RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1 RB3_APP_CHARTPLAY_TILT=1"
r() { t=$1; f=$2; shift 2; BIN=$B SECS=1500 TMO=1530 TAILN=1 ./runb.sh $K$t $PWD/walk_x.txt $f $D "$@" > $K$t.out 2>&1 & }
r L300 4800 RB3_APP_HZ=legacy RB3_APP_CHARTPLAY_TILT_MS=300
r L500 4800 RB3_APP_HZ=legacy RB3_APP_CHARTPLAY_TILT_MS=500
r F150 9600 RB3_APP_CHARTPLAY_TILT_MS=150
r F300 9600 RB3_APP_CHARTPLAY_TILT_MS=300
wait; echo tilt legs done $K
