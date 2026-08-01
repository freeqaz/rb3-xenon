"""Faithful port of objdiff-core/src/obj/read.rs::get_normalized_symbol_name.

This is THE RULER. Comparing raw names (what lane CH-2 and my first pass did)
flags symbols objdiff happily pairs -> a deterministic false-positive class.
Returns None when objdiff would not normalize (i.e. raw name is used).
"""
DUMMY = '0000'
DUMMY_MSVC = '00000000'


def normalized(name):
    if '@class$' in name:
        prefix, suffix = name.split('@class$', 1)
        idx = next((i for i, c in enumerate(suffix) if not c.isdigit()), None)
        if idx is not None and idx > 0:
            return f'{prefix}@class${DUMMY}{suffix[idx:]}'
    if '$' in name:
        prefix, suffix = name.split('$', 1)
        if suffix and suffix.isdigit() and prefix not in ('__unwind', '__catch'):
            return f'{prefix}${DUMMY}'
    if '.' in name:
        prefix, suffix = name.split('.', 1)
        if suffix and suffix.isdigit():
            return f'{prefix}.{DUMMY}'
    if name.startswith('?'):
        idxs = []
        start = 0
        while True:
            i = name.find('?A0x', start)
            if i < 0:
                break
            idxs.append(i)
            start = i + 1
        if not idxs:
            return None
        s = name
        for i in idxs:
            try:
                int(s[i + 4:i + 12], 16)
            except ValueError:
                continue
            if len(s[i + 4:i + 12]) != 8:
                continue
            if s[i + 12:i + 14] != '@@':
                continue
            s = s[:i + 4] + DUMMY_MSVC + s[i + 14 - 2:]
        return s
    return None


def key(name):
    """The name objdiff effectively pairs on."""
    n = normalized(name)
    return n if n is not None else name


if __name__ == '__main__':
    a = '?NewNetMessage@MainHubAdvanceMsg@?A0xfb94c5e0@@SAPAVNetMessage@@XZ'
    b = '?NewNetMessage@MainHubAdvanceMsg@?A0x24d0882e@@SAPAVNetMessage@@XZ'
    print(key(a)); print(key(b)); print('PAIR' if key(a) == key(b) else 'NO-PAIR')
    c = '??$__destroy_range@PAUDebugGraph@?A0x8ed455d4@@U12@@stlpmtx_std@@YAXPAUDebugGraph@?A0x8ed455d4@@00@Z'
    d = '??$__destroy_range@PAUDebugGraph@?A0x9f7029cb@@U12@@stlpmtx_std@@YAXPAUDebugGraph@?A0x9f7029cb@@00@Z'
    print('multi-hash PAIR' if key(c) == key(d) else 'multi-hash NO-PAIR')
    # FAIL ON DEMAND: genuinely different symbols must NOT collapse
    e = '?ByteCode@OpenWaitingGateMsg@?A0xe564e51a@@UBAEXZ'
    f = '?ByteCode@KickPlayerMsg@?A0xe564e51a@@UBAEXZ'
    print('fail-on-demand:', 'OK (distinct stay distinct)' if key(e) != key(f) else 'BROKEN')
    g = '__unwind$123'
    print('unwind not normalized:', 'OK' if key(g) == g else 'BROKEN')
