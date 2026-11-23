#!/bin/bash
# gate60 rb3-app run (lane walk60, 2026-11-23): runb.sh <tag> <script> [frames] [extra env...].
# Frames and the wall caps (SECS, TMO) are 60 Hz frames / seconds; RB3_APP_HZ defaults to 60
# (HZ=legacy runs the old rate, for a legacy leg of an unconverted script). Fresh reflink copy of
# memcard's first-boot store per run. Log: $A/r_<tag>/run.log, last line rc=N.
A=${A:-/home/free/tmp/walk60}; C=/home/free/tmp/claude-1000/rb3-scout/coord
X=${X:-/home/free/tmp/wt-walk60}; NV=/home/free/tmp/claude-1000/nv615-userspace
tag=$1; script=$2; frames=${3:-1800}; shift 3 2>/dev/null
D=$A/r_$tag; rm -rf $D; mkdir -p $D
cp -r --reflink=auto /home/free/tmp/memcard/content_a $D/content
(ulimit -d unlimited; cd $D && env VK_DRIVER_FILES=$NV/nvidia_icd_615.json VK_ICD_FILENAMES=$NV/nvidia_icd_615.json LD_LIBRARY_PATH=$NV/usr/lib \
   RB3_CONTENT_DIR=$D/content RB3_APP_RUN=1 RB3_APP_FRAMES=$frames RB3_APP_SECONDS=${SECS:-260} RB3_APP_HZ=${HZ:-60} \
   MILO_INPUT_LOG=1 MILO_INPUT_SCRIPT=$script "$@" \
   flock -s -w 900 /home/free/tmp/claude-1000/gpu1.lock $C/no610m.sh timeout ${TMO:-300} $WRAP ${BIN:-$X/native/${BD:-build}/rb3-app} $HOME/code/milohax/rb3/orig-assets/xbox-zip > run.log 2>&1; echo "rc=$?" >> run.log)
command grep -a -E "^rb3-app: frame|DC3 Input|No screen to pop|RESULT|rc=" $D/run.log | tail -${TAILN:-60}
