#!/usr/bin/env python3
"""Textual map-fragment applier for scripts/target_symbol_map.json.

Inserts each {addr: mangled_name} entry from a stage's map_fragment.json as a
single ' "addr": "name",' line immediately after the opening brace of the map.

HARD PROJECT INVARIANT: never json.dump-rewrite target_symbol_map.json. It is a
~17k-line, 1-space-indent file whose formatting is a load-bearing convention
(the target-symbol renamer and downstream diffs depend on it); a full
json.dump would reflow every line. This applier only splices new lines in and
leaves every existing line byte-identical.

Collision asserts (fail fast, never silently overwrite):
  * addr collision : the fragment key already exists in the map (case-insensitive).
  * name collision : the fragment value is already a mapped value.

Usage:
  tu5_map_apply_fragment.py <fragment.json> <target_symbol_map.json>
"""
import argparse
import json


def apply_fragment(frag_path, map_path):
    frag = json.load(open(frag_path))
    lines = open(map_path).read().split('\n')
    assert lines[0] == '{', lines[0]
    # sanity: no key/value collisions against the current map
    cur = json.load(open(map_path))
    keys = set(k.lower() for k in cur.keys())
    vals = set(v for v in cur.values() if isinstance(v, str))
    ins = []
    for k, v in sorted(frag.items()):
        assert k.lower() not in keys, f'addr collision {k}'
        assert v not in vals, f'name collision {v}'
        ins.append(f' "{k}": {json.dumps(v)},')
    out = [lines[0]] + ins + lines[1:]
    open(map_path, 'w').write('\n'.join(out))
    return len(ins)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('fragment', help='map_fragment.json emitted by a stage driver')
    ap.add_argument('map', help='path to scripts/target_symbol_map.json to update')
    args = ap.parse_args()
    n = apply_fragment(args.fragment, args.map)
    print(f'inserted {n} entries into {args.map}')


if __name__ == '__main__':
    main()
