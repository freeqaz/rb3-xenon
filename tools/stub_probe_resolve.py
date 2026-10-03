#!/usr/bin/env python3
"""Resolve a RB3_STUB_PROBE dump (native/src/cc5_stub_probe.c) to symbol names.

Usage: stub_probe_resolve.py <probe-build executable> <RB3_STUB_PROBE_OUT file>
Prints "<count> <demangled symbol>" per distinct entered function. The PIE load
bias is recovered from the dump's #REF line against nm's rb3_stub_record.
(Lane W16-PD; CC-5 left the resolver as an unnamed one-off.)
"""
import sys, subprocess, bisect
exe, probe = sys.argv[1], sys.argv[2]
syms=[]
for ln in subprocess.run(['nm','-C','--defined-only',exe],capture_output=True,text=True).stdout.splitlines():
    p=ln.split(' ',2)
    if len(p)==3 and p[1] in 'TtWw':
        syms.append((int(p[0],16),p[2]))
syms.sort()
addrs=[s[0] for s in syms]
ref_nm=[a for a,n in syms if n=='rb3_stub_record'][0]
lines=open(probe).read().splitlines()
ref=int(lines[0].split()[2],16); bias=ref-ref_nm
for ln in lines[2:]:
    _,a,c=ln.split(); a=int(a,16)-bias
    i=bisect.bisect_right(addrs,a)-1
    print(c, syms[i][1] if i>=0 and syms[i][0]==a else f'?{a:x}')
