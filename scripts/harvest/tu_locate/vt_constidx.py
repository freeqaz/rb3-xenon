"""Index every 32-bit constant materialised in .text via lis+addi / lis+ori.
Used to find the code sites that store a vtable pointer (ctors/dtors).
Pure-python (no numpy in this env)."""
import array, sys, os, pickle
from _paths import SCRATCH, REPO, BANDEXE, WII_SRC  # noqa: E402
from vt_pe import data, SEC, TEXT_LO

CACHE = SCRATCH+'/constidx2.pkl'
_n, _sva, _vs, _pr, _rs = SEC['.text']
N = min(_vs, _rs) // 4


def build():
    W = array.array('I', data[_pr:_pr + 4 * N])
    W.byteswap()  # file is BE, host LE
    out = {}
    pend_imm = [None] * 32
    pend_i = [-99] * 32
    for i in range(N):
        w = W[i]
        o = w >> 26
        if o == 15:
            if ((w >> 16) & 31) == 0:
                d = (w >> 21) & 31
                pend_imm[d] = w & 0xFFFF
                pend_i[d] = i
            continue
        if o == 14:
            a = (w >> 16) & 31
            d = a
            if pend_imm[d] is not None and i - pend_i[d] <= 8:
                lo = w & 0xFFFF
                if lo >= 0x8000:
                    lo -= 0x10000
                v = ((pend_imm[d] << 16) + lo) & 0xFFFFFFFF
                out.setdefault(v, []).append(TEXT_LO + 4 * pend_i[d])
            continue
        if o == 24:
            s = (w >> 21) & 31
            if pend_imm[s] is not None and i - pend_i[s] <= 8:
                v = ((pend_imm[s] << 16) | (w & 0xFFFF)) & 0xFFFFFFFF
                out.setdefault(v, []).append(TEXT_LO + 4 * pend_i[s])
            continue
    return out


if os.path.exists(CACHE):
    CONST = pickle.load(open(CACHE, 'rb'))
else:
    CONST = build()
    pickle.dump(CONST, open(CACHE, 'wb'))


def sites(va):
    return sorted(set(CONST.get(va, ())))


if __name__ == '__main__':
    print('constants:', len(CONST))
    for a in sys.argv[1:]:
        v = int(a, 16)
        print(a, [hex(x) for x in sites(v)])
