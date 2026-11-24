#!/bin/bash
cd /home/free/tmp/walk60b; R=./r.sh
F=RB3_APP_FOCUS_LOG=1
T=($F RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1 RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1)
SECS=2500 TMO=2600 $R tourwalk2 full w_full.txt 64000 "${T[@]}" RB3_APP_CHARTPLAY_WHAMMY=od RB3_APP_CHARTPLAY_TILT=1 &
$R tourwalk2 losequit w_lose_quit.txt 9000 "${T[@]}" RB3_APP_CHARTPLAY_MISS=100 &
$R tourwalk2 loseres w_lose_resume.txt 26000 "${T[@]}" RB3_APP_CHARTPLAY_MISS=100 RB3_APP_CHARTPLAY_MISS_TO=600 &
SECS=2000 TMO=2100 $R trainwalk2 DC w_dc.txt 30000 $F RB3_APP_JOYPAD_KIND=drums RB3_APP_AUTOPLAY=1 RB3_APP_AUTODEPLOY=1 &
$R trainwalk2 CP1 w_dc.txt 7000 $F RB3_APP_JOYPAD_KIND=drums RB3_APP_CHARTPLAY=1 &
$R trainwalk2 SP w_speed.txt 9000 $F RB3_APP_JOYPAD_KIND=drums RB3_APP_AUTOPLAY=1 &
$R trainwalk2 WR w_wrong.txt 2600 $F &
$R trainwalk2 RGPC w_rg_pc.txt 12000 $F RB3_APP_JOYPAD_KIND=proguitar RB3_APP_AUTOPLAY=1 &
$R trainwalk2 RKMB w_rk_mbc.txt 12000 $F RB3_APP_JOYPAD_KIND=keys RB3_APP_AUTOPLAY=1 &
M=(RB3_APP_AUTOPLAY=1 RB3_APP_AUTOPLAY_SKIP=3 $F RB3_APP_MOD_LOG=1 "RB3_APP_EXTRA_PADS=guitar=@W@/p1.txt,drums=@W@/p2.txt,guitar=@W@/p3.txt")
for w in base mods indep; do $R modwalk $w m_$w.txt 20000 "${M[@]}" & done
$R menuwalk calib w_calib.txt 5000 $F &
$R menuwalk practice w_practice.txt 5000 $F RB3_APP_AUTOPLAY=1 &
$R menuwalk setlist w_setlist.txt 5000 $F &
$R menuwalk hub1 d_hub1.txt 5000 $F &
$R menuwalk hub3 d_hub3.txt 5000 $F &
wait; echo LEGS_DONE
