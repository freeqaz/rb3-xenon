#!/usr/bin/env python3
"""Given a candidate retail address, decompile it in Ghidra and, if it looks
like an ObjPtrList<T,ObjectDir>::Replace body (calls the __RTDynamicCast
thunk with a 5th-arg-style call), extract the target RTTI type descriptor
address and resolve the class name T by reading the descriptor bytes."""
import re
import sys
sys.path.insert(0, 'tools/ghidra')
from mcp_client import create_client

RTDYN_PAT = re.compile(r"Function_82804DA8\([^,]+,\s*0,\s*0x[0-9a-fA-F]+,\s*0x([0-9a-fA-F]+),\s*0\)")

def probe(addr, client=None):
    c = client or create_client()
    result = c.decompile_function(addr if isinstance(addr, str) else hex(addr))
    code = result.get('decompiled') or result.get('code') or str(result)
    m = RTDYN_PAT.search(code)
    if not m:
        return {'addr': addr, 'ok': False, 'reason': 'no RTDynamicCast call found', 'code': code}
    target_hex = m.group(1)
    # address may include leading 'ffffffff' 64-bit sign extension prefix
    target_hex = target_hex[-8:]
    target_addr = '0x' + target_hex
    rb = c.read_bytes(target_addr, 48)
    data = bytes.fromhex(rb['data'])
    name = data[8:].split(b'\x00')[0].decode('ascii', errors='replace')
    return {'addr': addr, 'ok': True, 'rtti_addr': target_addr, 'name': name}

if __name__ == '__main__':
    c = create_client()
    for a in sys.argv[1:]:
        r = probe(a, c)
        print(r)
