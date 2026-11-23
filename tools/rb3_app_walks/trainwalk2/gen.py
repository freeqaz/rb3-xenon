#!/usr/bin/env python3
# gen.py <out> <prefix> <trainer_idx> <lesson_downs> <sections> [finish]
# trainers.lst index 0 drums, 1 pro drums (hint screen), 2 pro guitar, 3 pro keys.
import sys
out, prefix, ti, ld, ns = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
finish = len(sys.argv) > 6 and sys.argv[6] == "finish"
L = open(prefix).read().rstrip("\n").split("\n")
L += [f"# trainwalk2 gen: trainer {ti}, lesson downs {ld}, sections {ns}, finish {finish}",
      "+60 down", "+90 down", "+150 a", "+300 down", "+360 a", "wait_screen training_screen"]
t = 120
for _ in range(ti):
    L.append(f"+{t} down"); t += 60
L.append(f"+{t} a"); t += 220
if ti == 1:
    L.append(f"+{t} a"); t += 300   # dismiss hint_prodrums_screen
for _ in range(ld):
    L.append(f"+{t} down"); t += 60
L.append(f"+{t+40} a")
for i in range(ns):
    L += ["wait_screen gem_preview_screen", "+90 a"]
    if i < ns - 1 or not finish:
        L += ["wait_screen lesson_finished_screen", "+30 a"]
if finish:
    L += ["wait_screen trainer_finished_screen", "+60 a", "wait_screen training_screen", "+300 b", "+500 b", "+700 b"]
open(out, "w").write("\n".join(L) + "\n")
