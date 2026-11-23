#!/bin/bash
# gate.sh <tag> <bin> [frames]: the band gate walk (rb3-scout a38's walk2 + p*_2, 4 pads, det +
# autoplay) at RB3_APP_HZ=60, seed $SEED (default 12: the 60 Hz seed that loads beforeiforget,
# as legacy seed 17 does; legacy's seed 0 song, oyemiamor, is not among 60 Hz seeds 0-27; lane
# walk60). Default cap 44000 frames: song 2 starts at ~23300 and runs ~17400 at 60 Hz (legacy
# frame 18738, 60 Hz ~37500) plus margin. Legacy leg: HZ=legacy G=<dir of the a38 originals>.
L=$(cd $(dirname $0) && pwd); G=${G:-$L}; tag=$1; B=$2; F=${3:-44000}
EP="RB3_APP_EXTRA_PADS=guitar=$G/p1_guitar_2.txt,drums=$G/p2_drums_2.txt,guitar=$G/p3_bass_2.txt"
BIN=$B SECS=${SECS:-1500} TMO=${TMO:-1560} $L/runb.sh $tag $G/walk2.txt $F RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 RB3_APP_SEED=${SEED:-12} "$EP"
