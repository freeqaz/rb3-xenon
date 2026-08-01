#!/usr/bin/env python3
"""Apply lane CH-4's map repairs TEXTUALLY (1-space indent is load-bearing;
never json.dump this file), with every edit gated on the expected current
value and the result re-parsed under a duplicate-key detector.

Two groups, both from instruments calibrated in-lane:

A. QuatKeys displacement chain (Part 1). Three ??_G rows each sit one link
   down a chain; confirmed by TWO independent instruments (retail vtable
   slot-0 attribution AND the ??_G body's first non-helper bl):
       0x8242b5e0  slot0(QuatKeys)   dtor_bl ??1QuatKeys    -> ??_GQuatKeys   [RESTORE: CG-1 deleted this]
       0x82711820  slot0(MetaMusic)  dtor_bl ??1MetaMusic   -> ??_GMetaMusic
       0x8276f458  slot0(BeatMaster) dtor_bl ??1BeatMaster  -> ??_GBeatMaster [name was unmapped]

B. StaticClassName platform-variant repairs (Part 3), from the string-free
   vtable-caller discriminator calibrated 258/258 = 100% on the unambiguous
   population against an address-shuffled null of 2/258. Three RECIPROCAL
   SWAPS plus one rename.
"""
import json, re, sys, os, collections

MAP = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   '..', '..', 'scripts', 'target_symbol_map.json')

# (addr, expected_current_or_None, new_value)
EDITS = [
    # --- A: QuatKeys chain ------------------------------------------------
    ('0x8242b5e0', None, '??_GQuatKeys@@UAAPAXI@Z'),
    ('0x82711820', '??_GQuatKeys@@UAAPAXI@Z', '??_GMetaMusic@@UAAPAXI@Z'),
    ('0x8276f458', '??_GMetaMusic@@UAAPAXI@Z', '??_GBeatMaster@@UAAPAXI@Z'),
    # --- B: StaticClassName platform variants -----------------------------
    ('0x82273860', '?StaticClassName@DxTex@@SA?AVSymbol@@XZ',
                   '?StaticClassName@RndTex@@SA?AVSymbol@@XZ'),
    ('0x827347d0', '?StaticClassName@RndTex@@SA?AVSymbol@@XZ',
                   '?StaticClassName@DxTex@@SA?AVSymbol@@XZ'),
    ('0x824089d0', '?StaticClassName@NgEnviron@@SA?AVSymbol@@XZ',
                   '?StaticClassName@RndEnviron@@SA?AVSymbol@@XZ'),
    ('0x82739080', '?StaticClassName@RndEnviron@@SA?AVSymbol@@XZ',
                   '?StaticClassName@NgEnviron@@SA?AVSymbol@@XZ'),
    ('0x8240dcb8', '?StaticClassName@DxLight@@SA?AVSymbol@@XZ',
                   '?StaticClassName@RndLight@@SA?AVSymbol@@XZ'),
    ('0x8273fd30', '?StaticClassName@RndLight@@SA?AVSymbol@@XZ',
                   '?StaticClassName@DxLight@@SA?AVSymbol@@XZ'),
    ('0x8227a7a8', '?StaticClassName@HamSong@@SA?AVSymbol@@XZ',
                   '?StaticClassName@BandSong@@SA?AVSymbol@@XZ'),
]
# a brand-new key is inserted after this anchor line (same TU, keeps locality)
INSERT_AFTER = {'0x8242b5e0': '0x8242b540'}


def dupcheck(pairs):
    seen = collections.Counter(k for k, _ in pairs)
    dup = [k for k, n in seen.items() if n > 1]
    if dup:
        raise SystemExit(f'DUPLICATE KEYS in map: {dup[:10]}')
    return dict(pairs)


def main():
    src = open(MAP).read()
    before = json.loads(src, object_pairs_hook=dupcheck)
    print(f'map keys before: {len(before)}')

    for addr, expect, new in EDITS:
        cur = before.get(addr)
        if expect is None:
            if cur is not None:
                raise SystemExit(f'GATE FAIL {addr}: expected absent, found {cur!r}')
            anchor = INSERT_AFTER[addr]
            pat = re.compile(r'^ "%s": ("(?:[^"\\]|\\.)*"),\n' % re.escape(anchor),
                             re.M)
            m = pat.search(src)
            if not m:
                raise SystemExit(f'anchor line for {anchor} not found')
            ins = ' "%s": %s,\n' % (addr, json.dumps(new))
            src = src[:m.end()] + ins + src[m.end():]
            print(f'  INSERT {addr} = {new}')
        else:
            if cur != expect:
                raise SystemExit(f'GATE FAIL {addr}: expected {expect!r}, found {cur!r}')
            old_line = ' "%s": %s,\n' % (addr, json.dumps(expect))
            if src.count(old_line) != 1:
                raise SystemExit(f'{addr}: line not uniquely locatable '
                                 f'({src.count(old_line)} hits)')
            src = src.replace(old_line, ' "%s": %s,\n' % (addr, json.dumps(new)))
            print(f'  EDIT   {addr}: {expect}  ->  {new}')

    after = json.loads(src, object_pairs_hook=dupcheck)
    print(f'map keys after:  {len(after)}  (delta {len(after)-len(before)})')
    changed = {k for k in after if before.get(k) != after.get(k)}
    print(f'changed keys: {len(changed)}')
    assert len(changed) == len(EDITS), (len(changed), len(EDITS))
    open(MAP, 'w').write(src)
    print('written')


if __name__ == '__main__':
    main()
