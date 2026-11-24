#!/bin/bash
# pair.sh <tag> <dir> <walk> <legacy frames> [env...]: legacy leg (main 82ff40429's script, HZ=legacy,
# cap F) and 60 Hz leg (walk60's converted script, cap 2F) on bin-w1; @D@ in env = that leg's script dir.
cd /home/free/tmp/walk60; tag=$1; d=$2; w=$3; F=$4; shift 4; R=/home/free/tmp/wt-walk60/tools/rb3_app_walks/gate60/runb.sh
LD=/home/free/tmp/walk60/legacy/$d; ND=/home/free/tmp/wt-walk60/tools/rb3_app_walks/$d
le=(); ne=(); for e in "$@"; do le+=("${e//@D@/$LD}"); ne+=("${e//@D@/$ND}"); done
HZ=legacy BIN=$PWD/bin-w1 SECS=1500 TMO=1560 TAILN=1 $R ${tag}L $LD/$w $F RB3_APP_DETERMINISTIC=1 "${le[@]}" > ${tag}L.out 2>&1 &
BIN=$PWD/bin-w1 SECS=2400 TMO=2460 TAILN=1 $R ${tag}6 $ND/$w $((2*F)) RB3_APP_DETERMINISTIC=1 "${ne[@]}" > ${tag}6.out 2>&1 &
wait; echo "pair $tag done"
