#!/bin/bash
# gate.sh <tag> <bin> [frames]: the band gate walk (rb3-scout a38's walk2 + p*_2, 4 pads, det +
# autoplay) at RB3_APP_HZ=60, seed $SEED (default 34 since 2026-11-24: on main 9d0a07794 it loads
# beforeiforget, as seed 12 did on f25eee490 (lane walk60) and legacy seed 17 does; the det RNG moved
# with 8786e09b5 and 9d0a07794. Measured on 9d0a07794, repeat IDENTICAL over 44000 frames: song 1
# band 156,462, players 42,000/28,962/48,475/37,025, = walk60's record; song 2 is another song,
# band 140,323; song_select at 41,085). Default cap 44000 frames: song 2 starts at ~23300 and runs ~17400 at 60 Hz (legacy
# frame 18738, 60 Hz ~37500) plus margin. Legacy leg: HZ=legacy G=<dir of the a38 originals>.
L=$(cd $(dirname $0) && pwd); G=${G:-$L}; tag=$1; B=$2; F=${3:-44000}
EP="RB3_APP_EXTRA_PADS=guitar=$G/p1_guitar_2.txt,drums=$G/p2_drums_2.txt,guitar=$G/p3_bass_2.txt"
BIN=$B SECS=${SECS:-1500} TMO=${TMO:-1560} $L/runb.sh $tag $G/walk2.txt $F RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 RB3_APP_SEED=${SEED:-34} "$EP"
