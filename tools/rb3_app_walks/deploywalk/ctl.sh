#!/bin/bash
# ctl.sh <tag> <bin> [frames]: control -- bandwalk's own band walk (walk1.txt + p1_guitar/p2_drums/p3_bass,
# a38 copies), RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 and none of deploywalk's env: the det line
# must be byte-identical between the keyswalk base binary and this branch's.
cd /home/free/tmp/deploywalk; B=$PWD/$2; F=${3:-12000}
BIN=$B SECS=300 TMO=340 ./runb.sh $1 $PWD/walk1.txt $F RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 "RB3_APP_EXTRA_PADS=guitar=$PWD/p1_guitar.txt,drums=$PWD/p2_drums.txt,guitar=$PWD/p3_bass.txt" > $1.out 2>&1
