#!/bin/bash
cd /home/free/tmp/tourwalk2
BIN=$PWD/bin2 SECS=1500 TMO=1540 TAILN=1 ./runb.sh full $PWD/w_full.txt 64000 RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1 RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1 RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_WHAMMY=od RB3_APP_CHARTPLAY_TILT=1 > full.out 2>&1 &
BIN=$PWD/bin2 SECS=600 TMO=640 TAILN=1 ./runb.sh losequit $PWD/w_lose_quit.txt 9000 RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1 RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1 RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_MISS=100 > losequit.out 2>&1 &
BIN=$PWD/bin1asan SECS=1500 TMO=1560 TAILN=1 ./runb.sh asan $PWD/w_full.txt 18000 ASAN_OPTIONS=halt_on_error=0:suppress_equal_pcs=0:detect_leaks=0:alloc_dealloc_mismatch=1:new_delete_type_mismatch=1:symbolize=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1 RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1 RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_WHAMMY=od RB3_APP_CHARTPLAY_TILT=1 > asan.out 2>&1 &
cp ~/tmp/tourwalk2/../wt-tourwalk2/tools/rb3_app_walks/tourwalk/w_reload.txt w_reload.txt
wait; echo legs3 done
