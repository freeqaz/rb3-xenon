#!/bin/bash
# A23 rb3-app run: run.sh <tag> <script> [frames] [extra env...]. Fresh reflink copy of memcard's
# first-boot store per run. Log: a19/r_<tag>/run.log, last line rc=N.
A=/home/free/tmp/chartplay2; C=/home/free/tmp/claude-1000/rb3-scout/coord
X=${X:-/home/free/tmp/wt-chartplay2}; NV=/home/free/tmp/claude-1000/nv615-userspace
tag=$1; script=$2; frames=${3:-900}; shift 3 2>/dev/null
D=$A/r_$tag; rm -rf $D; mkdir -p $D
cp -r --reflink=auto /home/free/tmp/memcard/content_a $D/content
(ulimit -d unlimited; cd $D && env VK_DRIVER_FILES=$NV/nvidia_icd_615.json VK_ICD_FILENAMES=$NV/nvidia_icd_615.json LD_LIBRARY_PATH=$NV/usr/lib \
   RB3_CONTENT_DIR=$D/content RB3_APP_RUN=1 RB3_APP_FRAMES=$frames RB3_APP_SECONDS=${SECS:-130} \
   MILO_INPUT_LOG=1 MILO_INPUT_SCRIPT=$script "$@" \
   flock -s -w 900 /home/free/tmp/claude-1000/gpu1.lock $C/no610m.sh timeout ${TMO:-150} $WRAP ${BIN:-$X/native/${BD:-build}/rb3-app} $HOME/code/milohax/rb3/orig-assets/xbox-zip > run.log 2>&1; echo "rc=$?" >> run.log)
command grep -a -E "^rb3-app: frame|DC3 Input|No screen to pop|A16|RESULT|rc=" $D/run.log | tail -${TAILN:-60}
