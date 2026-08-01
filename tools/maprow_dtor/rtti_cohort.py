#!/usr/bin/env python3
"""Lane CG-1: build the 'destructor row whose class has no retail RTTI' cohort.

Deliberately reports DENOMINATORS next to every count, and carries its own
positive/negative controls (--selftest) because a class-name extractor that
silently yields '' would make every row look RTTI-absent.
"""
import json, re, subprocess, sys, collections

BAND = 'orig/45410914/band.exe'
CCONV = ('__thiscall ', '__cdecl ', '__stdcall ', '__fastcall ', '__clrcall ', '__vectorcall ')
DTOR_MARKS = ("::`scalar deleting dtor'", "::`vector deleting dtor'", '::~')


def undname(names):
    """Batch-demangle. Returns dict mangled->demangled (or None)."""
    out = {}
    B = 2000
    names = list(names)
    for i in range(0, len(names), B):
        chunk = names[i:i + B]
        p = subprocess.run(['llvm-undname'], input='\n'.join(chunk) + '\n',
                           capture_output=True, text=True)
        lines = p.stdout.split('\n')
        # llvm-undname echoes: <mangled>\n<demangled>\n\n
        cur = None
        for ln in lines:
            if ln in chunk or (cur is None and ln.strip()):
                if ln in chunk:
                    cur = ln
                    continue
            if cur is not None and ln.strip():
                out.setdefault(cur, ln.strip())
                cur = None
    return out


def cls_from_dtor(dem):
    """Class name out of a demangled destructor. None if not a destructor."""
    if dem is None:
        return None
    rest = dem
    for c in CCONV:
        if c in rest:
            rest = rest.split(c, 1)[1]
            break
    for m in DTOR_MARKS[:2]:
        if m in rest:
            return rest.split(m, 1)[0].strip()
    # plain ??1: Class::~Class(...)  -- cut at the LAST '::~'
    idx = rest.rfind('::~')
    if idx > 0:
        return rest[:idx].strip()
    return None


def cls_from_rtti(dem):
    if dem is None:
        return None
    s = dem
    s = re.sub(r"\s*`RTTI Type Descriptor Name'\s*$", '', s)
    for kw in ('class ', 'struct ', 'union ', 'enum '):
        if s.startswith(kw):
            s = s[len(kw):]
            break
    return s.strip() or None


def retail_rtti_names(path=BAND):
    d = open(path, 'rb').read()
    pat = rb'\.\?A[VUW][A-Za-z0-9_@\$\?<>\-\.]{1,400}?@@'
    return sorted(set(m.group(0).decode('ascii') for m in re.finditer(pat, d)))


def selftest():
    ok = True
    # positive controls: extractor must recover the class
    pos = [
        ('public: __thiscall Bar::Foo::~Foo(void)', 'Bar::Foo'),
        ("public: virtual void * __thiscall CharEyes::`scalar deleting dtor'(unsigned int)", 'CharEyes'),
        ('public: __thiscall std::vector<class Foo *, class allocator<class Foo *>>::~vector<class Foo *, class allocator<class Foo *>>(void)',
         'std::vector<class Foo *, class allocator<class Foo *>>'),
    ]
    for dem, want in pos:
        got = cls_from_dtor(dem)
        print(f'  dtor  {"OK " if got == want else "FAIL"}  {got!r} (want {want!r})')
        ok &= got == want
    for dem, want in [("class Bar::Foo `RTTI Type Descriptor Name'", 'Bar::Foo'),
                      ("struct Head@BandCharDesc `RTTI Type Descriptor Name'", 'Head@BandCharDesc')]:
        got = cls_from_rtti(dem)
        print(f'  rtti  {"OK " if got == want else "FAIL"}  {got!r} (want {want!r})')
        ok &= got == want
    # NEGATIVE control: a non-destructor must yield None, not ''
    for dem in ['public: void __thiscall Foo::Bar(void)', 'int __cdecl main(void)']:
        got = cls_from_dtor(dem)
        print(f'  neg   {"OK " if got is None else "FAIL"}  {got!r} (want None)')
        ok &= got is None
    # FAIL-ON-DEMAND: prove the RTTI scan can report absence
    fake = retail_rtti_names.__doc__
    print(f'  scan-can-fail: .?AVDefinitelyNotARealClassXYZ@@ in retail? ', end='')
    names = set(retail_rtti_names())
    print('ABSENT (good)' if '.?AVDefinitelyNotARealClassXYZ@@' not in names else 'PRESENT (BROKEN)')
    ok &= '.?AVDefinitelyNotARealClassXYZ@@' not in names
    print(f'  scan-can-hit:  .?AVCharEyes@@ in retail? ', end='')
    hit = '.?AVCharEyes@@' in names
    print('PRESENT (good)' if hit else 'ABSENT (BROKEN — scan is vacuous)')
    ok &= hit
    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


def main():
    if '--selftest' in sys.argv:
        sys.exit(selftest())

    rep = json.load(open('build/45410914/report.json'))
    rows = []
    nfun = 0
    for u in rep['units']:
        for f in u.get('functions') or []:
            nfun += 1
            n = f['name']
            m = re.match(r'\?\?(1|_G|_E)', n)
            if m:
                rows.append(dict(unit=u['name'], sym=n, kind=m.group(1),
                                 fuzzy=f.get('fuzzy_match_percent'),
                                 tgt_size=int(f['size']), addr=f.get('address')))
    print(f'functions in report: {nfun}   destructor-mangled rows: {len(rows)}')

    rtti = retail_rtti_names()
    print(f'retail type-descriptor name strings: {len(rtti)}')
    rd = undname(rtti)
    rtti_cls = set(filter(None, (cls_from_rtti(rd.get(x)) for x in rtti)))
    print(f'  -> distinct retail RTTI class names: {len(rtti_cls)} '
          f'(undname resolved {len(rd)}/{len(rtti)})')

    dd = undname(sorted(set(r['sym'] for r in rows)))
    unresolved = 0
    for r in rows:
        dem = dd.get(r['sym'])
        r['dem'] = dem
        r['cls'] = cls_from_dtor(dem)
        if r['cls'] is None:
            unresolved += 1
        r['rtti'] = (r['cls'] in rtti_cls) if r['cls'] else None
    print(f'  -> destructor rows with unextractable class: {unresolved}/{len(rows)}')

    # ---- THE CONTROL: RTTI-presence rate over the WHOLE destructor population
    def rate(sel, label):
        sel = [r for r in sel if r['cls']]
        if not sel:
            print(f'  {label}: EMPTY')
            return
        have = sum(1 for r in sel if r['rtti'])
        print(f'  {label}: RTTI present {have}/{len(sel)} = {100*have/len(sel):.1f}%')

    print('\n=== untreated-population control (RTTI presence by stratum) ===')
    rate(rows, 'ALL destructor rows          ')
    for k in ('1', '_G', '_E'):
        rate([r for r in rows if r['kind'] == k], f'  kind ??{k:<3}                  ')
    f100 = [r for r in rows if r['fuzzy'] == 100.0]
    rate(f100, 'fuzzy==100                   ')
    rate([r for r in rows if r['fuzzy'] is not None and r['fuzzy'] < 100.0],
         'fuzzy<100                    ')
    for k in ('1', '_G', '_E'):
        rate([r for r in f100 if r['kind'] == k], f'  fuzzy100 kind ??{k:<3}         ')

    absent = [r for r in f100 if r['cls'] and not r['rtti']]
    print(f'\nfuzzy-100 destructor rows with class ABSENT from retail RTTI: '
          f'{len(absent)} / {len([r for r in f100 if r["cls"]])}')
    json.dump(rows, open('/home/free/tmp/laneCG1/dtor_cohort.json', 'w'), indent=1)
    print('wrote /home/free/tmp/laneCG1/dtor_cohort.json')


if __name__ == '__main__':
    main()
