#!/bin/bash
# runs.sh <K> <bin> <asanbin>: deploywalk identity legs, all RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1
# RB3_APP_AUTODEPLOY=1 RB3_APP_DET_OD=1, 90000-frame cap (SECS=540 TMO=580):
#   <K>A, <K>B  band walk (4 pads, walk_od.txt + p1/p2/p3_od.txt), two runs: identity
#   <K>P        band walk under GLIBC_TUNABLES=glibc.malloc.perturb=190
#   <K>S        band walk, ASan+UBSan binary
#   <K>X        save band walk (p3_save.txt, RB3_APP_AUTOPLAY_SKIP=3): fail, save, band fail
cd /home/free/tmp/deploywalk; K=$1; B=$PWD/$2; AB=$PWD/$3
W=$PWD/walk_od.txt
EP="RB3_APP_EXTRA_PADS=guitar=$PWD/p1_od.txt,drums=$PWD/p2_od.txt,guitar=$PWD/p3_od.txt"
ES="RB3_APP_EXTRA_PADS=guitar=$PWD/p1_od.txt,drums=$PWD/p2_od.txt,guitar=$PWD/p3_save.txt"
DE="RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1 RB3_APP_AUTODEPLOY=1 RB3_APP_DET_OD=1"
for t in A B; do BIN=$B SECS=540 TMO=580 ./runb.sh ${K}$t $W 90000 $DE $EP > ${K}$t.out 2>&1 & done
BIN=$B SECS=540 TMO=580 ./runb.sh ${K}P $W 90000 $DE $EP GLIBC_TUNABLES=glibc.malloc.perturb=190 > ${K}P.out 2>&1 &
BIN=$AB SECS=540 TMO=580 ./runb.sh ${K}S $W 90000 $DE $EP ASAN_OPTIONS=halt_on_error=0:suppress_equal_pcs=0:detect_leaks=0:alloc_dealloc_mismatch=1:new_delete_type_mismatch=1:symbolize=1 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 > ${K}S.out 2>&1 &
BIN=$B SECS=540 TMO=580 ./runb.sh ${K}X $W 90000 $DE RB3_APP_AUTOPLAY_SKIP=3 $ES > ${K}X.out 2>&1 &
wait
sha256sum $B $AB | cut -c1-12
echo runs done $K
