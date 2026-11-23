#!/bin/bash
# tilt.sh <K> <bin> [legacy-seed] [60hz-seed] [frames-legacy]: lane framerate tilt legs, expert guitar
# by chartplay. Each tilt is held a fixed song time (RB3_APP_CHARTPLAY_TILT_MS), so the input is
# the same at either frame rate; the TU5 any_sign_filter needs num_frames 12 consecutive polls >= 0.93.
# L* = RB3_APP_HZ=legacy (1/30 s frames, the harness default), F* = RB3_APP_HZ=60; *N0 = no tilt.
# The seed -> song map moves with the frame rate: legacy 18 / 60 Hz 18 = ohmygod / smokeonthewater
# (run t1), legacy 2 / 60 Hz 10 = beastandtheharlot both (run t2).
cd /home/free/tmp/framerate; K=$1; B=$PWD/$2; SL=${3:-18}; SF=${4:-18}; FL=${5:-4800}; FF=$((FL * 2))
D="RB3_APP_DETERMINISTIC=1 RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1 RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1"
r() { t=$1; f=$2; shift 2; BIN=$B SECS=1500 TMO=1530 TAILN=1 ./runb.sh $K$t $PWD/walk_x.txt $f $D "$@" > $K$t.out 2>&1 & }
T=RB3_APP_CHARTPLAY_TILT=1
r L300 $FL RB3_APP_SEED=$SL RB3_APP_HZ=legacy $T RB3_APP_CHARTPLAY_TILT_MS=300
r L500 $FL RB3_APP_SEED=$SL RB3_APP_HZ=legacy $T RB3_APP_CHARTPLAY_TILT_MS=500
r F150 $FF RB3_APP_SEED=$SF RB3_APP_HZ=60 $T RB3_APP_CHARTPLAY_TILT_MS=150
r F300 $FF RB3_APP_SEED=$SF RB3_APP_HZ=60 $T RB3_APP_CHARTPLAY_TILT_MS=300
if [ -n "$NOTILT" ]; then
  r LN0 $FL RB3_APP_SEED=$SL RB3_APP_HZ=legacy
  r FN0 $FF RB3_APP_SEED=$SF RB3_APP_HZ=60
fi
wait; echo tilt legs done $K
