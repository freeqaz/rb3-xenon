#!/bin/bash
# p1.sh <class> <seed>: one probe run (classes in classes.txt: class suite walk cap env...)
c=$1; s=$2; read -r _ S W F E <<< "$(command grep -a "^$c " /home/free/tmp/walk60b/classes.txt)"
WT=/home/free/tmp/wt-walk60b/tools/rb3_app_walks/$S; export A=/home/free/tmp/walk60b/probe
read -r -a env <<< "${E//@W@/$WT}"
cd $WT && BIN=/home/free/tmp/walk60b/bin-m0 SECS=300 TMO=330 TAILN=1 ./runb.sh ${c}_$s $WT/$W $F RB3_APP_DETERMINISTIC=1 RB3_APP_SEED=$s "${env[@]}" > /dev/null 2>&1
song=$(command grep -a -o -E "'songs/[a-z0-9_]+/" $A/r_${c}_$s/run.log | head -1 | cut -d/ -f2)
echo "$c $s ${song:-none} $(tail -1 $A/r_${c}_$s/run.log)" >> /home/free/tmp/walk60b/probe.txt
rm -rf $A/r_${c}_$s/content
