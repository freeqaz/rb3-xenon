#!/usr/bin/env python3
"""rb3_app_log_cmp.py <logA> <logB> [maxframe]: ordered diff of the WHOLE log of two rb3-app runs made with
RB3_APP_DETERMINISTIC=1, over the lines printed before the last common `rb3-app-det: frame` line (or maxframe).
Finer than tools/rb3_app_det_cmp.py (which compares only the per-frame state hash): any main-thread log line
that differs shows up. Hex pointers and wall-clock suffixes are normalised; two classes that differ between two
runs of one binary with default malloc are dropped: `=== BONE DIAG` blocks (render-side pose dumps) and
StandardStream's `timer error is large` (audio-thread sample clock); RAW=1 keeps them. SHOW=<n> prints the first n hunks.
Exit 0 with no hunks, 1 otherwise. Record: docs/decomp/PERTURB2_DETERMINISTIC_HEAP_FILL_2026-11-23.md."""
import sys, re, difflib
HEX=re.compile(r'0x[0-9a-fA-F]+|\b[0-9a-f]{12,16}\b')
WALL=re.compile(r'\(\s*[0-9.]+ ?(s|ms)\)|[0-9]+\.[0-9]+ ?ms|took [0-9.]+')
RAW=__import__('os').environ.get('RAW')=='1'  # RAW=1: keep the two noise classes
def load(p):
    out=[]; last=0; inbone=False
    for l in open(p,'rb'):
        l=l.decode('latin-1').rstrip('\n')
        if l.startswith('rb3-app-det: frame '): last=int(l.split()[2])
        if l.startswith('=== BONE DIAG'): inbone=True
        elif inbone and not (l.startswith('  ') or l==''): inbone=False
        if not RAW and (inbone or l=='' or l.startswith('timer error is large')): continue
        out.append((last, WALL.sub('<T>', HEX.sub('<P>', l))))
    return out
a=load(sys.argv[1]); b=load(sys.argv[2])
lim=min(a[-1][0], b[-1][0]) if len(sys.argv)<4 else int(sys.argv[3])
la=[l for f,l in a if f<lim]; lb=[l for f,l in b if f<lim]
print('lines A %d B %d up to frame %d' % (len(la), len(lb), lim))
n=0
for g in difflib.SequenceMatcher(None, la, lb, autojunk=False).get_opcodes():
    if g[0]=='equal': continue
    n+=1
    if n<=int(__import__('os').environ.get('SHOW','12')):
        print('--', g[0], 'A[%d:%d] B[%d:%d]'%g[1:])
        for l in la[g[1]:min(g[2],g[1]+4)]: print('  A|', l[:200])
        for l in lb[g[3]:min(g[4],g[3]+4)]: print('  B|', l[:200])
print('diff hunks:', n)
sys.exit(1 if n else 0)
