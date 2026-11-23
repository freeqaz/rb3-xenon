#!/bin/bash
# disc.sh <tag> <bin> <walk> [frames] [extra env...]: a deterministic menu walk with the focus log on.
# Log r_<tag>/run.log; r_<tag>/screens.txt = the screen/focus/input trail.
cd /home/free/tmp/trainwalk
tag=$1; bin=$2; walk=$3; frames=${4:-6000}; shift 4 2>/dev/null
BIN=$PWD/$bin SECS=${SECS:-300} TMO=${TMO:-340} TAILN=1 ./runb.sh $tag $PWD/walks/$walk $frames RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1 "$@" > /dev/null
command grep -a -E "^rb3-app: frame|DC3 Input|^FAIL|UNREACHED|RESULT|rc=|runtime error|ERROR: AddressSanitizer|Assertion|abort" r_$tag/run.log | command grep -a -v "^rb3-app-det" > r_$tag/screens.txt
echo "$tag: $(tail -1 r_$tag/run.log) FAIL=$(command grep -a -c '^FAIL' r_$tag/run.log) UNREACHED=$(command grep -a -c UNREACHED r_$tag/run.log) last=$(command grep -a '^rb3-app: frame' r_$tag/run.log | tail -1)"
