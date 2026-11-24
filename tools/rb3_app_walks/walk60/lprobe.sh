#!/bin/bash
# lprobe.sh <seeds...>: song each seed loads in the LEGACY 30 Hz gate walk (a38 originals, 1100-frame cap)
cd /home/free/tmp/walk60
for s in "$@"; do G=/home/free/tmp/walk60/gate30 HZ=legacy SEED=$s SECS=300 TMO=330 TAILN=1 /home/free/tmp/wt-walk60/tools/rb3_app_walks/gate60/gate.sh lq$s $PWD/bin-w0 1100 > lq$s.out 2>&1 & done
wait
for s in "$@"; do echo "seed $s: $(command grep -a -o -E "'songs/[a-z0-9_]+/[a-z0-9_]+\.milo'" r_lq$s/run.log | head -1)"; done
