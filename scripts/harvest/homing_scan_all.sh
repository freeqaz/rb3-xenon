#!/bin/bash
# Whole-tree reloc-masked byte-identity homing scan, parallel.
#
# homing_scan.py re-parses band.exe per invocation, so it is cheapest to shard
# the obj list and run the shards concurrently.  ~2 min for 914 objs on 16 way.
#
# Usage:  homing_scan_all.sh <worktree> <outdir> [parallelism] [objs-per-shard]
# Result: <outdir>/merged.json  in homing_scan result format (tu -> [records]),
#         which is what caller_side_invert.py / multi_content_disambiguate.py /
#         map_rotation_repair.py all consume.
set -e
WT=${1:?worktree}
OUT=${2:?outdir}
P=${3:-16}
CHUNK=${4:-20}
mkdir -p "$OUT/scan"
cd "$WT"
find build/45410914/src -name '*.obj' | sort > "$OUT/objs.txt"
N=$(wc -l < "$OUT/objs.txt")
NB=$(( (N + CHUNK - 1) / CHUNK ))

cat > "$OUT/shard.sh" <<EOF
#!/bin/bash
WT=$WT; OUT=$OUT; CHUNK=$CHUNK
cd \$WT
mapfile -t OBJS < \$OUT/objs.txt
i=\$(( \$1 * CHUNK ))
args=()
for (( j=i; j<i+CHUNK && j<\${#OBJS[@]}; j++ )); do
  p="\${OBJS[j]}"; key="\${p#build/45410914/src/}"; key="\${key%.obj}"
  args+=("\$key=\$WT/\$p")
done
[ \${#args[@]} -eq 0 ] && exit 0
HOMING_NO_DEFAULTS=1 HOMING_WT=\$WT HOMING_ROOT=\$WT \\
  HOMING_TMAP=\$WT/scripts/target_symbol_map.json \\
  HOMING_OUT=\$OUT/scan/b\$(printf %03d \$1).json \\
  python3 scripts/harvest/homing_scan.py "\${args[@]}" > /dev/null
EOF
chmod +x "$OUT/shard.sh"
seq 0 $((NB - 1)) | xargs -P "$P" -I{} "$OUT/shard.sh" {}

python3 - "$OUT" <<'PY'
import glob, json, sys
from collections import Counter
out = sys.argv[1]
merged, c = {}, Counter()
for f in sorted(glob.glob(out + '/scan/b*.json')):
    for tu, recs in json.load(open(f)).items():
        if not isinstance(recs, list):
            continue
        merged[tu] = recs
        for r in recs:
            c[r['cls']] += 1
json.dump(merged, open(out + '/merged.json', 'w'))
print('%d TUs ->' % len(merged), out + '/merged.json')
print(dict(c.most_common()))
PY
