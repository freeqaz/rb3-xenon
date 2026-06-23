#!/usr/bin/env python3
"""Best-effort MetroWerks/CodeWarrior symbol demangler for evidence packs.

Produces a human-readable "class::method(args)" rendering from a CW-mangled
symbol. Not exhaustive (CW mangling is gnarly with templates), but good enough
to give a judge the class/method/arg shape. Falls back to the raw symbol.
"""
import re

PRIM = {
    'v': 'void', 'i': 'int', 'l': 'long', 's': 'short', 'c': 'char',
    'b': 'bool', 'f': 'float', 'd': 'double', 'x': 'long long', 'w': 'wchar_t',
    'r': 'long double',
}


def _read_len_name(s, i):
    m = re.match(r'(\d+)', s[i:])
    if not m:
        return None, i
    n = int(m.group(1))
    j = i + len(m.group(1))
    return s[j:j+n], j+n


def _parse_class(sym):
    """Extract the class name from the trailing __<scope> of a method symbol.

    Handles simple `__<len><Name>F...` and qualified `__Q2<len><A><len><B>F...`.
    Returns (class_str, rest_after_F) or (None, None).
    """
    # find the '__' that introduces the scope+signature (last one before an F sig)
    # Simplest robust approach: locate '__' then parse scope, then 'F' begins args.
    idxs = [m.start() for m in re.finditer(r'__', sym)]
    for start in reversed(idxs):
        after = sym[start+2:]
        cls, rest = _parse_scope(after)
        if cls is not None:
            # const method: optional 'C' qualifier sits between scope and 'F'
            r = rest
            const = False
            if r.startswith('C'):
                const = True
                r = r[1:]
            if r.startswith('F'):
                return (cls + (' const' if const else '')), r[1:]
        # free function: __F<args>
        if after.startswith('F'):
            return '', after[1:]
    return None, None


def _parse_scope(s):
    """Parse a scope: either Q<N><parts> or a single <len><name>."""
    if s.startswith('Q'):
        m = re.match(r'Q(\d)', s)
        if not m:
            return None, s
        nparts = int(m.group(1))
        i = 2
        parts = []
        for _ in range(nparts):
            name, i2 = _read_len_name(s, i)
            if name is None:
                return None, s
            parts.append(name)
            i = i2
        return '::'.join(parts), s[i:]
    else:
        name, i2 = _read_len_name(s, 0)
        if name is None:
            return None, s
        return name, s[i2:]


def demangle(sym):
    """Return a readable string; conservative, never raises."""
    try:
        # constructor/destructor special forms
        ctor = sym.startswith('__ct__')
        dtor = sym.startswith('__dt__')
        base = sym
        method = None
        if ctor or dtor:
            # __ct__<scope>F...  -> method = Class / ~Class
            rest = sym[len('__ct__'):] if ctor else sym[len('__dt__'):]
            cls, after = _parse_scope(rest)
            if cls and after.startswith('F'):
                short = cls.split('::')[-1]
                method = ('~' + short) if dtor else short
                return f'{cls}::{method}(...)'
            return sym
        # find method name = up to first '__'
        midx = sym.find('__')
        if midx <= 0:
            return sym
        method = sym[:midx]
        cls, _ = _parse_class(sym)
        if cls is None:
            return sym
        if cls == '':
            return f'{method}(...)   [free function]'
        return f'{cls}::{method}(...)'
    except Exception:
        return sym


if __name__ == '__main__':
    import sys, json
    for s in sys.argv[1:]:
        print(s, '=>', demangle(s))
