#!/bin/bash
# r.sh <suite> <tag> <walk> <legacy cap> [env...]: one 60 Hz leg of a suite's converted walk on bin-m0;
# @W@ in env = the suite's converted script dir. Log ~/tmp/walk60b/<suite>/r_<tag>/run.log.
S=$1; t=$2; w=$3; F=$4; shift 4
WT=/home/free/tmp/wt-walk60b/tools/rb3_app_walks/$S; export A=/home/free/tmp/walk60b/$S; mkdir -p $A
e=(); for x in "$@"; do e+=("${x//@W@/$WT}"); done
cd $WT && BIN=${BIN:-/home/free/tmp/walk60b/bin-m0} SECS=${SECS:-1500} TMO=${TMO:-1530} TAILN=1 ./runb.sh $t $WT/$w $F RB3_APP_DETERMINISTIC=1 "${e[@]}" > $A/$t.out 2>&1
