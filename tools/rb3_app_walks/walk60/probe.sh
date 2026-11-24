#!/bin/bash
# probe.sh <bin> <seeds...>: song each seed loads in the 60 Hz gate walk (2400-frame cap)
cd /home/free/tmp/walk60; B=$PWD/$1; shift
for s in "$@"; do SEED=$s SECS=400 TMO=430 TAILN=1 /home/free/tmp/wt-walk60/tools/rb3_app_walks/gate60/gate.sh gq$s $B 2400 > gq$s.out 2>&1 & done
wait
for s in "$@"; do echo "seed $s: $(command grep -a -o -E "'songs/[a-z0-9_]+/[a-z0-9_]+\.milo'" r_gq$s/run.log | head -1)"; done
