#!/bin/bash
# sprobe.sh <suite> <walk> <legacy-frame cap> "<seeds>" [env...]: first songs/*.milo each seed loads
# at 60 Hz in that suite's converted walk (runb.sh doubles the cap). Runs in ~/tmp/walk60b/<suite>.
S=$1; W=$2; F=$3; SEEDS=$4; shift 4
WT=/home/free/tmp/wt-walk60b/tools/rb3_app_walks/$S; export A=/home/free/tmp/walk60b/$S; mkdir -p $A
for s in $SEEDS; do (cd $WT && BIN=/home/free/tmp/walk60b/bin-m0 SECS=${SECS:-400} TMO=${TMO:-430} TAILN=1 ./runb.sh p$s $WT/$W $F RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=$s "$@" > $A/p$s.out 2>&1) & done
wait
for s in $SEEDS; do echo "$S seed $s: $(command grep -a -o -E "'songs/[a-z0-9_]+/[a-z0-9_]+\.milo'" $A/r_p$s/run.log | head -1) $(tail -1 $A/r_p$s/run.log)"; done
