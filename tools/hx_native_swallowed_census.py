#!/usr/bin/env python3
"""Census of function DEFINITIONS the match build structurally CANNOT SEE.

WHY THIS EXISTS
---------------
`src/macros.h` force-defines MILO_DEBUG tree-wide, but the match build never
defines HX_NATIVE.  A function definition that has been swept inside an
`#ifdef HX_NATIVE` block therefore emits NO BODY at all -- and because the
match build COMPILES but never LINKS, nothing can catch it.  The only symptom
is downstream: the retail row for that function reads fuzzy 0 (our objects
define the name nowhere, so objdiff has nothing to pair), and every call site
takes a relocation-name charge.

Lane W15-B hit exactly this: `DataArray::Release` -- a core refcount method
with 1,882 retail call sites -- had been swept into DataArray.cpp's native-only
include block and had no definition in the match build for months.

WHAT IT DOES
------------
Walks src/{system,band3,network}, and for every function definition reports
whether its enclosing preprocessor state requires HX_NATIVE.  `--classify`
additionally decides, per row, whether RETAIL contains the function:

  RETAIL_HAS_IT  a retail row carries the mangled name AND our compiled
                 objects define it nowhere  => un-gate and port
  DEFINED_ANYWAY the name is compiled from some other TU (an un-gated twin,
                 or a header) => the gate is not actually swallowing it
  NATIVE_ONLY    no retail row bears the name => genuinely host-only code
  UNKNOWN        could not form a mangled-name probe

PARSING DISCIPLINE (both halves are load-bearing)
-------------------------------------------------
1. Directives are matched ONLY at a line anchor (`^\s*#\s*if`), never
   "anywhere on the line".  `ScatterIncludes.cmake` matched `#if` anywhere and
   a PROSE COMMENT containing the text `#ifdef HX_NATIVE` pushed a phantom
   frame, silently reclassifying 212 lines and breaking the native link
   (rb3-xenon CLAUDE.md, commit 6c087cbd).
2. `/* */` and `//` comments are stripped first, with string/char literals
   respected, so a directive inside a comment cannot push a frame either.
3. `#if`/`#endif` balance is asserted at EOF.  A desync is reported, never
   silently absorbed -- that is the one bucket the CMake module ignored.

Usage:
  python3 tools/hx_native_swallowed_census.py --selftest
  python3 tools/hx_native_swallowed_census.py [--root .] [--classify] [--tsv]
"""
import argparse
import glob
import json
import os
import re
import sys

DIRECTIVE = re.compile(r'^[ \t]*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b[ \t]*(.*)$')
HX = 'HX_NATIVE'

# ---------------------------------------------------------------- comments


def strip_comments(text, comment_blind=False):
    """Blank out /* */ and // comments, preserving line count and offsets.

    String and char literals are honoured so that a `//` inside a literal is
    not mistaken for a comment.  `comment_blind=True` is the SABOTAGE path used
    by --selftest to prove the fixture discriminates.
    """
    if comment_blind:
        return text
    out = []
    i, n = 0, len(text)
    state = 'code'   # code | line | block | str | chr
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ''
        if state == 'code':
            if c == '/' and nxt == '/':
                state = 'line'; out.append('  '); i += 2; continue
            if c == '/' and nxt == '*':
                state = 'block'; out.append('  '); i += 2; continue
            if c == '"':
                state = 'str'; out.append(c); i += 1; continue
            if c == "'":
                state = 'chr'; out.append(c); i += 1; continue
            out.append(c); i += 1; continue
        if state == 'line':
            if c == '\n':
                state = 'code'; out.append(c)
            else:
                out.append(' ')
            i += 1; continue
        if state == 'block':
            if c == '*' and nxt == '/':
                state = 'code'; out.append('  '); i += 2; continue
            out.append('\n' if c == '\n' else ' '); i += 1; continue
        if state in ('str', 'chr'):
            out.append(c)
            if c == '\\':
                if i + 1 < n:
                    out.append(text[i + 1]); i += 2
                else:
                    i += 1
                continue
            if (state == 'str' and c == '"') or (state == 'chr' and c == "'"):
                state = 'code'
            i += 1; continue
    return ''.join(out)


# ------------------------------------------------------------ preprocessor

def _hx_polarity(expr):
    """+1 if the condition REQUIRES HX_NATIVE, -1 if it requires its absence,
    0 if HX_NATIVE does not appear.

    Only conjunctive positions are treated as requiring: `defined(A) &&
    defined(HX_NATIVE)` is +1, but `defined(A) || defined(HX_NATIVE)` is 0
    because the block can still compile with HX_NATIVE undefined.
    """
    if HX not in expr:
        return 0
    if '||' in expr:
        return 0
    neg = re.search(r'!\s*(defined\s*\(\s*%s\s*\)|defined\s+%s\b)' % (HX, HX), expr)
    if neg:
        return -1
    return +1


def preproc_hx_state(text, anchored=True):
    """Return (states, errors): states[i] is True when line i+1 compiles only
    with HX_NATIVE defined.  `anchored=False` is the SABOTAGE path."""
    pat = DIRECTIVE if anchored else re.compile(
        r'.*#[ \t]*(if|ifdef|ifndef|elif|else|endif)\b[ \t]*(.*)$')
    lines = text.split('\n')
    states, errors, stack = [], [], []
    for ln, line in enumerate(lines, 1):
        m = pat.match(line)
        if m:
            kw, rest = m.group(1), m.group(2).strip()
            if kw in ('if', 'ifdef', 'ifndef'):
                if kw == 'ifdef':
                    pol = +1 if rest.split()[0:1] == [HX] else 0
                elif kw == 'ifndef':
                    pol = -1 if rest.split()[0:1] == [HX] else 0
                else:
                    pol = _hx_polarity(rest)
                stack.append({'pol': pol, 'line': ln, 'seen_else': False})
            elif kw == 'elif':
                if stack:
                    stack[-1]['pol'] = _hx_polarity(rest)
                else:
                    errors.append(f'line {ln}: #elif with no open #if')
            elif kw == 'else':
                if stack:
                    f = stack[-1]
                    f['pol'] = -f['pol']
                    f['seen_else'] = True
                else:
                    errors.append(f'line {ln}: #else with no open #if')
            elif kw == 'endif':
                if stack:
                    stack.pop()
                else:
                    errors.append(f'line {ln}: #endif with no open #if')
            states.append(any(f['pol'] > 0 for f in stack))
            continue
        states.append(any(f['pol'] > 0 for f in stack))
    if stack:
        errors.append('unbalanced #if at EOF: %d frame(s) open, first at line %d'
                      % (len(stack), stack[0]['line']))
    return states, errors


# --------------------------------------------------------------- functions

_NOT_FN = re.compile(
    r'\b(class|struct|union|enum|namespace|typedef|template|using|return|'
    r'if|for|while|switch|catch|do|else|throw)\b')
_NAME = re.compile(
    r'(?:(~?\w+|operator\s*(?:[^\w\s(]+|\w+))\s*$)')


def _decl_name(decl):
    """Extract Class::Method (or a free function name) from a declarator."""
    d = decl.strip()
    # trailing ctor-initialiser list: `Foo::Foo(int a) : mA(a)` -> cut at ':'
    # but only a ':' that is not part of '::'
    depth = 0
    cut = len(d)
    i = 0
    while i < len(d):
        ch = d[i]
        if ch == '(':
            depth += 1
        elif ch == ')':
            depth -= 1
        elif ch == ':' and depth == 0:
            if i + 1 < len(d) and d[i + 1] == ':':
                i += 2
                continue
            if i > 0 and d[i - 1] == ':':
                i += 1
                continue
            cut = i
            break
        i += 1
    d = d[:cut]
    # find the LAST top-level '(' -- the parameter list
    depth = 0
    open_at = -1
    for i, ch in enumerate(d):
        if ch == '(':
            if depth == 0:
                open_at = i
            depth += 1
        elif ch == ')':
            depth -= 1
    if open_at < 0:
        return None
    head = d[:open_at].strip()
    m = re.search(r'((?:\w+\s*::\s*)*(?:~?\w+|operator\s*(?:\[\]|\(\)|[^\w\s(]+|\w+)))\s*$',
                  head)
    if not m:
        return None
    return re.sub(r'\s+', '', m.group(1))


def find_definitions(text):
    """Return [(decl_line, name, declarator, body_line)] for definitions at file scope
    (including inside namespaces / extern "C" blocks).

    Works on comment-stripped text.  Tracks brace depth; at depth 0 a `{`
    preceded by a declarator carrying a top-level parameter list is a function
    definition.  Returns a second value: True when braces balanced at EOF.
    """
    defs = []
    depth = 0
    buf = []
    buf_start_line = 1
    line = 1
    paren = 0
    i, n = 0, len(text)
    # skip preprocessor lines entirely when accumulating declarators
    at_line_start = True
    in_directive = False
    while i < n:
        c = text[i]
        if c == '\n':
            line += 1
            at_line_start = True
            in_directive = False
            i += 1
            continue
        if at_line_start and c in ' \t':
            i += 1
            continue
        if at_line_start and c == '#':
            in_directive = True
        at_line_start = False
        if in_directive:
            i += 1
            continue
        if c in '"\'':
            q = c
            i += 1
            while i < n:
                if text[i] == '\\':
                    i += 2
                    continue
                if text[i] == q:
                    i += 1
                    break
                if text[i] == '\n':
                    line += 1
                i += 1
            continue
        if depth == 0:
            if c == '(':
                paren += 1
            elif c == ')':
                paren = max(0, paren - 1)
            if c == ';' and paren == 0:
                buf = []
                buf_start_line = line
                i += 1
                continue
            if not buf and not c.isspace():
                buf_start_line = line
            if c == '{':
                decl = ''.join(buf)
                nm = None
                if decl.strip() and not _NOT_FN.search(decl):
                    nm = _decl_name(decl)
                if nm:
                    defs.append((buf_start_line, nm,
                                 re.sub(r'\s+', ' ', decl.strip()), line))
                depth = 1
                buf = []
                buf_start_line = line
                i += 1
                continue
            if c == '}':
                # stray close at depth 0 -- desync
                buf = []
                buf_start_line = line
                i += 1
                continue
            buf.append(c)
            i += 1
            continue
        # inside a body
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                buf = []
                buf_start_line = line
        i += 1
    return defs, depth == 0


# ------------------------------------------------------------------ census

def census_file(path, text=None, comment_blind=False, anchored=True):
    if text is None:
        with open(path, 'r', errors='replace') as fh:
            text = fh.read()
    stripped = strip_comments(text, comment_blind=comment_blind)
    states, pp_err = preproc_hx_state(stripped, anchored=anchored)
    defs, braces_ok = find_definitions(stripped)
    rows = []
    for ln, name, decl, body_ln in defs:
        idx = body_ln - 1
        swallowed = states[idx] if 0 <= idx < len(states) else False
        if swallowed:
            rows.append({'file': path, 'line': ln, 'body_line': body_ln,
                         'name': name, 'decl': decl})
    return rows, pp_err, braces_ok


def mangle_probe(name):
    """MSVC mangled-name PREFIX probe for `Class::Method` / `free`.

    `void DataArray::Release()` -> `?Release@DataArray@@`, which is a literal
    substring of `?Release@DataArray@@QAAXXZ`.  Nested classes and namespaces
    are handled by reversing the qualifier chain.  Returns None for operators
    and destructors (their mangling is not a simple prefix).
    """
    parts = name.split('::')
    leaf = parts[-1]
    if leaf.startswith('~') or leaf.startswith('operator'):
        return None
    if len(parts) >= 2 and leaf == parts[-2]:
        return None            # constructor -> ??0Class@
    quals = parts[:-1]
    if not re.match(r'^\w+$', leaf):
        return None
    return '?' + leaf + '@' + ''.join(q + '@' for q in reversed(quals)) + '@'


# ---------------------------------------------------------------- selftest

FIXTURE = r'''
#include "foo.h"

// A prose comment that mentions #ifdef HX_NATIVE and must NOT push a frame.
/* A block comment spanning lines
   #ifdef HX_NATIVE
   still not a frame
   #endif
*/

void Plain::Visible() { mX = 1; }

#ifdef HX_NATIVE
void Swallowed::One() { mA = 2; }
#endif

#if defined(MILO_DEBUG) && defined(HX_NATIVE)
int Swallowed::Two(int a) { return a; }
#endif

#ifndef HX_NATIVE
void Retail::Only() { }
#else
void Swallowed::Three() { }
#endif

#if defined(SOMETHING) || defined(HX_NATIVE)
void NotRequired::Four() { }
#endif

#ifdef HX_NATIVE
#ifdef NESTED
void Swallowed::Five() { }
#endif
#endif

const char* kStr = "#ifdef HX_NATIVE inside a string literal";

/* Documentation block in the exact ScatterIncludes shape -- a directive at a
   LINE ANCHOR inside a comment with no matching endif inside the comment.
#ifdef HX_NATIVE
   ...prose explaining the guard...
*/
int gAfterGuard = 0;

void Plain::After() { }
'''

FIXTURE_EXPECT = ['Swallowed::One', 'Swallowed::Two', 'Swallowed::Three',
                  'Swallowed::Five']


def selftest():
    ok = True
    rows, pp_err, braces_ok = census_file('<fixture>', text=FIXTURE)
    got = [r['name'] for r in rows]
    print('  real analyzer  ->', got)
    if got != FIXTURE_EXPECT:
        print('  FAIL: expected', FIXTURE_EXPECT)
        ok = False
    if pp_err:
        print('  FAIL: unexpected preprocessor errors:', pp_err)
        ok = False
    if not braces_ok:
        print('  FAIL: brace desync on a balanced fixture')
        ok = False

    # --- discrimination: each sabotage MUST break the fixture -------------
    sab = {}
    r2, _, _ = census_file('<fixture>', text=FIXTURE, comment_blind=True)
    sab['comment-blind'] = [r['name'] for r in r2]
    r3, _, _ = census_file('<fixture>', text=FIXTURE, anchored=False)
    sab['directive-unanchored'] = [r['name'] for r in r3]
    for label, got2 in sab.items():
        same = (got2 == FIXTURE_EXPECT)
        print(f'  sabotage {label:22s} -> {got2}  {"VACUOUS" if same else "differs (good)"}')
        if same:
            print(f'  FAIL: fixture cannot discriminate {label}; the test proves nothing')
            ok = False

    # --- balance check must actually fire --------------------------------
    _, errs, _ = census_file('<fixture>', text='#ifdef HX_NATIVE\nvoid A::B() { }\n')
    if not any('unbalanced' in e for e in errs):
        print('  FAIL: unbalanced #if at EOF was not reported')
        ok = False
    else:
        print('  balance check fires on a missing #endif (good)')

    # --- mangled probe ----------------------------------------------------
    probes = {'DataArray::Release': '?Release@DataArray@@',
              'Hmx::Object::AddSink': '?AddSink@Object@Hmx@@',
              'PeakDetector::Synapse::DSP::Detect': None}
    for k, v in list(probes.items())[:2]:
        got3 = mangle_probe(k)
        if got3 != v:
            print(f'  FAIL: mangle_probe({k}) = {got3}, expected {v}')
            ok = False
    print('  mangle probe ok')
    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


# -------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--root', default='.')
    ap.add_argument('--selftest', action='store_true')
    ap.add_argument('--classify', action='store_true')
    ap.add_argument('--tsv', action='store_true')
    ap.add_argument('--dirs', default='src/system,src/band3,src/network')
    a = ap.parse_args()
    if a.selftest:
        return selftest()

    files = []
    for d in a.dirs.split(','):
        for ext in ('*.cpp', '*.c', '*.h', '*.hpp'):
            files += glob.glob(os.path.join(a.root, d, '**', ext), recursive=True)
    files = sorted(set(files))

    rows, pp_errors, brace_bad = [], [], []
    for f in files:
        r, errs, braces_ok = census_file(f)
        rows += r
        for e in errs:
            pp_errors.append(f'{f}: {e}')
        if not braces_ok:
            brace_bad.append(f)

    sys.stderr.write(f'scanned {len(files)} files; {len(rows)} HX_NATIVE-swallowed '
                     f'definitions; {len(pp_errors)} preprocessor errors; '
                     f'{len(brace_bad)} brace desyncs\n')
    for e in pp_errors[:20]:
        sys.stderr.write('  PP  ' + e + '\n')
    for f in brace_bad[:20]:
        sys.stderr.write('  BR  ' + f + '\n')

    if a.classify:
        tmap = json.load(open(os.path.join(a.root, 'scripts/target_symbol_map.json')))
        # ⚠ target_symbol_map.json carries PROSE NOTE values as well as mangled
        # names.  A bare substring probe matches inside a note and manufactures
        # a false RETAIL_HAS_IT (measured: `?MemFree@@` hit a 2 KB audit note).
        # Admit only values shaped like a symbol.
        SYMRE = re.compile(r'^[?_A-Za-z@$][A-Za-z0-9_?@$.<>,~\-]*$')

        def _syms(v):
            if isinstance(v, str):
                return [v] if SYMRE.match(v) else []
            if isinstance(v, (list, tuple)):
                out = []
                for x in v:
                    out += _syms(x)
                return out
            return []
        retail = set()
        for v in tmap.values():
            retail.update(_syms(v))
        rep = json.load(open(os.path.join(a.root, 'build/45410914/report.json')))
        rsizes = {}
        for u in rep['units']:
            for fn in u.get('functions', []):
                rsizes.setdefault(fn['name'], (u['name'], int(fn.get('size', 0)),
                                               float(fn.get('fuzzy_match_percent', 0.0))))
                if SYMRE.match(fn['name']):
                    retail.add(fn['name'])
        ours = set()
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        objroot = os.path.join(a.root, 'build/45410914/src')
        import struct

        def defined_symbols(path):
            d = open(path, 'rb').read()
            if len(d) < 20:
                return set()
            psym, nsym = struct.unpack_from('<II', d, 8)
            if psym == 0 or nsym == 0 or psym + nsym * 18 > len(d):
                return set()
            st = psym + nsym * 18
            out, i = set(), 0
            while i < nsym:
                off = psym + i * 18
                raw = d[off:off + 8]
                _v, secnum, _t, storage, naux = struct.unpack_from('<IhHBB', d, off + 8)
                if raw[:4] == b'\x00\x00\x00\x00':
                    so, = struct.unpack_from('<I', raw, 4)
                    end = d.find(b'\x00', st + so)
                    nm = d[st + so:end].decode('utf-8', 'replace')
                else:
                    nm = raw.rstrip(b'\x00').decode('utf-8', 'replace')
                if secnum > 0 and storage in (2, 3):
                    out.add(nm)
                i += 1 + naux
            return out
        for p in glob.glob(os.path.join(objroot, '**', '*.obj'), recursive=True):
            ours |= defined_symbols(p)

        for r in rows:
            probe = mangle_probe(r['name'])
            r['probe'] = probe or ''
            if not probe:
                r['klass'] = 'UNKNOWN'
                continue
            hits = [s for s in retail if probe in s]
            if not hits:
                r['klass'] = 'NATIVE_ONLY'
                continue
            undef = [h for h in hits if h not in ours]
            if undef:
                r['klass'] = 'RETAIL_HAS_IT'
                r['retail'] = sorted(undef, key=lambda h: -rsizes.get(h, ('', 0, 0))[1])
            else:
                r['klass'] = 'DEFINED_ANYWAY'
                r['retail'] = sorted(hits)[:3]

    if a.tsv:
        for r in rows:
            print('\t'.join([r.get('klass', ''), r['file'], str(r['line']),
                             r['name'], r.get('probe', ''),
                             ','.join(r.get('retail', [])[:2])]))
    else:
        print(json.dumps(rows, indent=1))
    return 0


if __name__ == '__main__':
    sys.exit(main())
