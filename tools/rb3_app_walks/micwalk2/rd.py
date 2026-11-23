import struct, sys
d = open('/home/free/code/milohax/rb3-xenon/orig/45410914/band.exe', 'rb').read()
pe = struct.unpack_from('<I', d, 0x3c)[0]
ns = struct.unpack_from('<H', d, pe + 6)[0]; osz = struct.unpack_from('<H', d, pe + 20)[0]
secs = []
for i in range(ns):
    o = pe + 24 + osz + 40 * i
    va, vs, raw, rp = struct.unpack_from('<IIII', d, o + 8)
    secs.append((va, vs, rp))
def at(v, n):
    rva = v - 0x82000000
    return d[rva:rva + n]
    for va, vs, rp in secs:
        if va <= rva < va + vs: return d[rp + rva - va: rp + rva - va + n]
for a in sys.argv[1:]:
    kind, v = a.split(':'); v = int(v, 16)
    b = at(v, 64)
    if kind == 's': print(hex(v), b.split(b'\0')[0].decode(errors='replace'))
    elif kind == 'f': print(hex(v), struct.unpack('>f', b[:4])[0])
    elif kind == 'd': print(hex(v), struct.unpack('>d', b[:8])[0])
