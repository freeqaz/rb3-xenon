#!/bin/bash
# gate.sh <tag> <bin> [frames]: the band gate walk (rb3-scout a38's walk2 + p*_2, 4 pads, det +
# autoplay) at RB3_APP_HZ=60, seed $SEED (default 13: the 60 Hz seed that loads oyemiamor first,
# as legacy seed 0 does; lane walk60). Default cap 40000 frames = song 2's song_select (legacy
# frame 18738, 60 Hz ~37500) plus margin. Legacy leg: HZ=legacy G=<dir of the a38 originals>.
L=$(cd $(dirname $0) && pwd); G=${G:-$L}; tag=$1; B=$2; F=${3:-40000}
EP="RB3_APP_EXTRA_PADS=guitar=$G/p1_guitar_2.txt,drums=$G/p2_drums_2.txt,guitar=$G/p3_bass_2.txt"
BIN=$B SECS=${SECS:-1500} TMO=${TMO:-1560} $L/runb.sh $tag $G/walk2.txt $F RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 RB3_APP_SEED=${SEED:-13} "$EP"
