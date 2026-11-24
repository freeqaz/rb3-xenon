#!/bin/bash
cd /home/free/tmp/walk60b; S=41; D=(RB3_APP_SEED=$S RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1); G=RB3_APP_JOYPAD_KIND=guitar; C=(RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1)
./r.sh codawalk c1GC walk_x.txt 9000 "${D[@]}" $G "${C[@]}" &
./r.sh codawalk c1GM walk_x.txt 9000 "${D[@]}" $G "${C[@]}" RB3_APP_CHARTPLAY_CODA=mash &
./r.sh codawalk c1GB walk_x.txt 9000 "${D[@]}" $G "${C[@]}" RB3_APP_CHARTPLAY_CODA=blow &
./r.sh codawalk c1GA walk_x.txt 9000 "${D[@]}" $G RB3_APP_AUTOPLAY=1 &
wait; echo coda done
