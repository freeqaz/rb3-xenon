#!/bin/bash
# runs.sh <bin> [suffix]: lane dropwalk's walk set, all deterministic (disc.sh), launched in parallel.
# <bin> is a copy of rb3-app under ~/tmp/dropwalk. Each walk: r_<tag><suffix>/run.log + screens.txt.
H=/home/free/tmp/wt-dropwalk/tools/rb3_app_walks/dropwalk; B=$1; S=$2
cd /home/free/tmp/dropwalk
$H/disc.sh base$S  $B w_base.txt 20000 > base$S.out 2>&1 &
$H/disc.sh base2$S $B w_base.txt 20000 > base2$S.out 2>&1 &
$H/disc.sh pause$S $B w_pause.txt 20000 > pause$S.out 2>&1 &
$H/disc.sh nofail$S $B w_nofail.txt 20000 > nofail$S.out 2>&1 &
$H/disc.sh restart$S $B w_restart.txt 20000 > restart$S.out 2>&1 &
$H/disc.sh quit$S  $B w_quit.txt 20000 > quit$S.out 2>&1 &
P3=p3_leave.txt  $H/disc.sh leave$S  $B w_base.txt 20000 > leave$S.out 2>&1 &
P1=p1_diff.txt   $H/disc.sh diff$S   $B w_base.txt 20000 > diff$S.out 2>&1 &
P3=p3_dropin.txt $H/disc.sh dropin$S $B w_base.txt 20000 > dropin$S.out 2>&1 &
EP="drums=$H/p2.txt,guitar=$H/p3.txt" SKIP=0 $H/disc.sh sitout$S $B w_sitout.txt 20000 \
  RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_MISS=100 RB3_APP_CHARTPLAY_MISS_FROM=40 \
  RB3_APP_CHARTPLAY_MISS_TO=200 RB3_APP_AUTODEPLOY=1 > sitout$S.out 2>&1 &
wait
cat *$S.out
