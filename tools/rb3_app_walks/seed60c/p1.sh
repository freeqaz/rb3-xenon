#!/bin/bash
# p1.sh <class> <seed>: one probe run on main 9d0a07794's rb3-app (seed re-derivation after the
# endgame-overlay and CharPollableSorter RNG shifts); first songs/*.milo appended to probe.txt
c=$1; s=$2; read -r _ S W F E <<< "$(command grep -a "^$c " /home/free/tmp/seed60c/classes.txt)"
WT=/home/free/tmp/wt-absent2/tools/rb3_app_walks/$S; export A=/home/free/tmp/seed60c/probe
read -r -a env <<< "${E//@W@/$WT}"
if [ "$S" = gate60 ]; then FR=$((F*2)); else FR=$F; fi   # gate60/runb.sh takes 60 Hz frames directly
cd $WT && BIN=/home/free/tmp/seed60c/bin SECS=300 TMO=330 TAILN=1 ./runb.sh ${c}_$s $WT/$W $FR RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=$s "${env[@]}" > /dev/null 2>&1
song=$(command grep -a -o -E "'songs/[a-z0-9_]+/" $A/r_${c}_$s/run.log | head -1 | cut -d/ -f2)
echo "$c $s ${song:-none} $(tail -1 $A/r_${c}_$s/run.log)" >> /home/free/tmp/seed60c/probe.txt
rm -rf $A/r_${c}_$s/content
