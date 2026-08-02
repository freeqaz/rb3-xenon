#!/usr/bin/env python3
"""Apply lane CI-1 map repairs TEXTUALLY (1-space indent is load-bearing; never
json.dump this file). Same gating discipline as CH-4's apply_ch4.py:

  * every edit states the EXPECTED current value and refuses on mismatch
  * the line must be UNIQUELY locatable in the raw text
  * the result is re-parsed with an object_pairs_hook duplicate-key detector
    (appliers that insert at the top produce a phantom edit otherwise: json.load
    keeps the LAST key, so the diff looks clean and the delta is 0)
  * name-injectivity is reported BEFORE and AFTER; the lane's budget is 0 new
    duplicates

Edit kinds (an edit is `{addr, expect, new, why}`):

  INSERT   expect == null, new == "<name>"   -> add a brand-new row
  REPLACE  expect == "<old>", new == "<new>" -> repoint an existing row
  DELETE   expect == "<old>", new == null    -> REMOVE THE KEY  (lane CM-3)

★ DELETE used to be UNEXPRESSIBLE. `new: null` fell into the REPLACE branch and
wrote the literal token `null` as the row's VALUE, and
`scripts/obj_target_symbol_renamer.py` had no type check on it -- so the None
reached `.encode("ascii")`. Measured behaviour (lane CM-3, real target obj):
crash + rc=1 when the address's `fn_<addr>` symbol exists in some obj (loud, but
`--batch --apply` is NOT atomic -- objs already written stay written), and a
silent rc=0 no-op when it does not. Both sides are fixed: the renamer now skips
null rows, and this applier removes the key outright.

⚠ A map edit of ANY kind is INERT without a forced re-split: the renamer only
ever sees `fn_<addr>` on a FRESH split. Once an obj carries the mangled name,
deleting or repointing the row changes nothing (lane CF-1 lost a whole A/B leg
to this: "[APPLIED] ... 0 files patched"). So this script forces the re-split
itself -- see force_resplit().

Usage:  ci1_apply.py <edits.json>      # [{addr, expect|null, new|null, why}, ...]
        ci1_apply.py <edits.json> --dry
        ci1_apply.py <edits.json> --no-resplit
"""
import json, re, sys, os, collections

_MISSING = object()

MAP = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   '..', '..', 'scripts', 'target_symbol_map.json')


def dupcheck(pairs):
    seen = collections.Counter(k for k, _ in pairs)
    dup = [k for k, n in seen.items() if n > 1]
    if dup:
        raise SystemExit(f'DUPLICATE KEYS in map: {dup[:10]}')
    return dict(pairs)


def addr_collisions(m):
    """Two DIFFERENT key spellings of the SAME address (e.g. 0x8262C280 vs
    0x8262c280). object_pairs_hook cannot see these -- they are distinct JSON
    keys -- but the renamer resolves both to one address, so one silently wins.
    """
    by = collections.defaultdict(list)
    for k in m:
        if k.startswith('0x'):
            by[int(k, 16)].append(k)
    return {hex(a): ks for a, ks in by.items() if len(ks) > 1}


def injectivity(m):
    n2a = collections.defaultdict(list)
    for k, v in m.items():
        if isinstance(v, str):
            n2a[v].append(k)
    return {n: a for n, a in n2a.items() if len(a) > 1}


def force_resplit(root, dry):
    """A map edit is INERT without a fresh SPLIT (see module docstring).

    Two of the three things ab_measure.py does for a map patch are pure BUILD
    ARTIFACTS and safe to do anywhere: rm the renamer stamp, touch config.yml.
    The third -- `git checkout -- config/45410914/symbols.txt` -- is a VCS
    mutation of a tracked file, and this script may be run in the SHARED main
    tree where clobbering another agent's in-flight edits is forbidden. So we
    DETECT symbols.txt drift and tell the operator, rather than restoring it.
    """
    stamp = os.path.join(root, 'build', '45410914',
                         'target_symbol_renames.stamp')
    cfg = os.path.join(root, 'config', '45410914', 'config.yml')
    if dry:
        print(f'  [resplit] DRY -- would rm {stamp} and touch {cfg}')
        return
    try:
        os.unlink(stamp)
        print(f'  [resplit] removed renamer stamp {stamp}')
    except FileNotFoundError:
        print(f'  [resplit] renamer stamp already absent ({stamp})')
    if os.path.exists(cfg):
        os.utime(cfg, None)
        print(f'  [resplit] touched {cfg} -- next ninja will re-SPLIT')
    else:
        print(f'  [resplit] WARN: {cfg} not found; re-split NOT forced')
    sym = os.path.join('config', '45410914', 'symbols.txt')
    rc = os.system(f'cd {root!r} && git diff --quiet -- {sym!r}')
    if rc != 0:
        print(f'  [resplit] ⚠ {sym} is DRIFTED. A split-forcing build needs it '
              f'restored or the split fails ("ends within symbol"). This '
              f'script will NOT touch tracked files. Run yourself, in the '
              f'right tree:\n              git -C {root} checkout -- {sym}')


def main():
    edits = json.load(open(sys.argv[1]))
    dry = '--dry' in sys.argv
    no_resplit = '--no-resplit' in sys.argv
    src = open(MAP).read()
    before = json.loads(src, object_pairs_hook=dupcheck)
    dup_before = injectivity(before)
    print(f'map keys before: {len(before)}   duplicate NAMES before: '
          f'{len(dup_before)}')

    keys = [k for k in before if k.startswith('0x')]

    def anchor_for(addr):
        """Nearest existing key BELOW addr, for diff locality only."""
        v = int(addr, 16)
        lo = [k for k in keys if int(k, 16) < v]
        return max(lo, key=lambda k: int(k, 16)) if lo else keys[0]

    for e in edits:
        addr, expect, new = e['addr'], e.get('expect'), e['new']
        cur = before.get(addr)
        if cur != expect:
            raise SystemExit(f'GATE FAIL {addr}: expected {expect!r}, found {cur!r}')
        if expect is None and new is None:
            raise SystemExit(
                f'{addr}: expect=null AND new=null is a no-op (delete of an '
                f'absent key). Refusing rather than silently counting it.')
        if expect is None:
            # INSERT a brand-new key after a nearby anchor line. Inserting at
            # the TOP is the documented phantom-edit footgun (json.load keeps
            # the LAST key, so the diff looks clean and the delta is 0).
            anc = anchor_for(addr)
            pat = re.compile(r'^ "%s": ("(?:[^"\\]|\\.)*"),\n' % re.escape(anc), re.M)
            m = pat.search(src)
            if not m:
                raise SystemExit(f'anchor line for {anc} not found')
            src = (src[:m.end()] + ' "%s": %s,\n' % (addr, json.dumps(new))
                   + src[m.end():])
            print(f'  INSERT {addr} = {new}   [{e.get("why","")}]')
            continue
        if new is None:
            # DELETE. `cur != expect` above already proved the row exists with
            # the stated value, so expect is a real string here.
            for tail in (',\n', '\n'):
                old_line = ' "%s": %s%s' % (addr, json.dumps(expect), tail)
                if src.count(old_line) == 1:
                    break
            else:
                raise SystemExit(
                    f'{addr}: DELETE line not uniquely locatable '
                    f'(comma-form {src.count(chr(32)+chr(34)+addr+chr(34)+": "+json.dumps(expect)+",")} '
                    f'hits, bare-form {src.count(chr(32)+chr(34)+addr+chr(34)+": "+json.dumps(expect)+chr(10))} hits)')
            i = src.index(old_line)
            head, rest = src[:i], src[i + len(old_line):]
            if tail == '\n':
                # We just removed the FINAL entry, which carries no trailing
                # comma. Whatever is now last still has ITS comma -> invalid
                # JSON. Strip it. (Without this the applier silently produces
                # an unparseable map; the dupcheck reparse below would raise,
                # but only AFTER the caller has been told nothing useful.)
                if not head.endswith(',\n'):
                    raise SystemExit(
                        f'{addr}: refusing -- removing the final row leaves no '
                        f'preceding row to un-comma (single-entry map?)')
                head = head[:-2] + '\n'
            src = head + rest
            print(f'  DELETE {addr} (was {expect})   [{e.get("why","")}]')
            continue
        # The LAST entry in the JSON object has no trailing comma, so the
        # comma-form pattern silently finds 0 hits (lane CJ-4 hit this on
        # 0x82b7b0b0, the final row).  Try both forms; still demand EXACTLY one
        # hit, so the uniqueness gate is unchanged.
        for tail in (',\n', '\n'):
            old_line = ' "%s": %s%s' % (addr, json.dumps(expect), tail)
            if src.count(old_line) == 1:
                break
        else:
            raise SystemExit(
                f'{addr}: line not uniquely locatable (comma-form '
                f'{src.count(chr(32) + chr(34) + addr + chr(34) + ": " + json.dumps(expect) + ",")} '
                f'hits, bare-form '
                f'{src.count(chr(32) + chr(34) + addr + chr(34) + ": " + json.dumps(expect) + chr(10))} hits)')
        src = src.replace(old_line, ' "%s": %s%s' % (addr, json.dumps(new), tail))
        print(f'  EDIT {addr}: {expect}  ->  {new}   [{e.get("why","")}]')

    after = json.loads(src, object_pairs_hook=dupcheck)
    col = addr_collisions(after)
    if col:
        raise SystemExit(f'REFUSING: case-variant duplicate ADDRESS keys: {col}')
    dup_after = injectivity(after)
    # ★ Must iterate the UNION: a DELETEd key is absent from `after`, so the old
    # `for k in after` comprehension could not see it and the
    # len(changed) != len(edits) gate below would fire on every correct delete.
    # The _MISSING sentinel keeps "key removed" distinct from "value is null".
    changed = {k for k in set(before) | set(after)
               if before.get(k, _MISSING) != after.get(k, _MISSING)}
    removed = sorted(set(before) - set(after))
    if removed:
        print(f'rows REMOVED: {len(removed)}  {removed[:10]}')
    print(f'map keys after:  {len(after)} (delta {len(after)-len(before)})   '
          f'changed keys: {len(changed)}')
    print(f'duplicate NAMES after: {len(dup_after)}  '
          f'(new: {sorted(set(dup_after) - set(dup_before))})')
    if len(changed) != len(edits):
        raise SystemExit(f'expected {len(edits)} changed, got {len(changed)}')
    if set(dup_after) - set(dup_before):
        raise SystemExit('REFUSING: this patch introduces new duplicate names')
    if dry:
        print('DRY RUN -- not written')
        return
    open(MAP, 'w').write(src)
    print('written')
    root = os.path.normpath(os.path.join(os.path.dirname(MAP), '..'))
    if no_resplit:
        print('  [resplit] SKIPPED (--no-resplit). ⚠ The edit is INERT until '
              'something forces a fresh SPLIT.')
    else:
        force_resplit(root, dry=False)


if __name__ == '__main__':
    main()
