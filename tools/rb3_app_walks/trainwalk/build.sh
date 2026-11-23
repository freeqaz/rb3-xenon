#!/bin/bash
# build.sh <bd> [cmake extra...]: configure (first time) + build rb3-app in ~/tmp/wt-trainwalk/native/<bd>
W=/home/free/tmp/wt-trainwalk; L=/home/free/tmp/trainwalk; bd=$1; shift
cd $W/native
if [ ! -f $bd/build.ninja ]; then
  nice -n 15 cmake -S . -B $bd -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn "$@" > $L/configure-$bd.log 2>&1 || { echo "configure FAILED"; tail -20 $L/configure-$bd.log; exit 1; }
fi
nice -n 15 timeout 3000 cmake --build $bd -j 12 --target rb3-app -- -k 0 > $L/build-$bd.log 2>&1; rc=$?
echo "build rc=$rc" >> $L/build-$bd.log
command grep -a -E "error:|undefined symbol" $L/build-$bd.log | head -30
echo "build rc=$rc"
