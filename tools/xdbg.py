#!/usr/bin/env python3
"""xdbg — RB3DX (Xbox 360) live-crash + static-disasm helper.

One tool for the SI-hardware debug loop: capture a live crash over XBDM, map the
fault PC/registers against the *real console image*, disassemble with game/DLL
symbol names, walk the stack, read memory and strings — instead of hand-writing
capstone one-offs and raw XBDM commands each time.

The console image is a flat dump (load addr 0x82000000): VA->file = VA-0x82000000.
It is cached locally; if missing, xdbg pulls default.xex off the console and
extracts the base with xextool (wine), once.

Subcommands
-----------
  xdbg crash                 capture the current first-chance fault: faulting
                             thread regs, fault instruction + context, stack walk
  xdbg dis   <VA> [n]        disassemble n instrs at VA (default 32), symbolized
  xdbg fn    <VA>            disassemble the whole function containing VA
  xdbg xref  <VA>           find every `bl VA` call site in the image
  xdbg mem   <VA> [n]        XBDM getmem dump (default 0x80), as words + ascii
  xdbg deref <VA> off...     follow a pointer chain live: *(VA), then +off each hop
  xdbg str   <VA>            read a NUL-terminated string from the image
  xdbg sym   <name|VA>       resolve a symbol name<->VA
  xdbg regs  [thread]        dump a thread's registers

Env: XBOX=192.168.8.180 (console IP), XDBG_BASE=<path to default.base cache>.
Needs: capstone (xex-patcher/.venv), wine+xextool for the one-time base pull.
"""
import os
import re
import struct
import subprocess
import sys

XBOX = os.environ.get("XBOX", "192.168.8.180")
LOAD = 0x82000000                       # console default.xex load address
DLL_BASE = 0x84000000                   # RB3Enhanced.dll load address
HERE = os.path.dirname(os.path.abspath(__file__))
XENON = os.path.dirname(HERE)
MILO = os.path.dirname(XENON)
BASE_CACHE = os.environ.get("XDBG_BASE", os.path.join(HERE, "oss-xbox-build", "_dbg", "default.base"))
PORTS_H = os.path.join(MILO, "RB3Enhanced", "include", "ports_xbox360.h")
DLL_MAP = os.path.join(HERE, "oss-xbox-build", "deploy-si-rb3dx", "RB3Enhanced.map")
XEXTOOL = os.path.join(MILO, "xex-patcher", "tools", "xextool.exe")
XBDM = os.path.join(HERE, "oss-xbox-build", "xbdm_cmd.py")

try:
    import capstone
    _MD = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_BIG_ENDIAN | capstone.CS_MODE_32)
except ImportError:
    _MD = None


# ---- symbol table -----------------------------------------------------------

def load_symbols():
    """VA -> name, from ports_xbox360.h (game) and the DLL map (RB3Enhanced)."""
    syms = {}
    if os.path.exists(PORTS_H):
        for ln in open(PORTS_H, errors="ignore"):
            m = re.match(r"\s*#define\s+PORT_\w+\s+(0x[0-9a-fA-F]+)\s*//\s*(.*)", ln)
            if m:
                syms[int(m.group(1), 16)] = m.group(2).strip()
    if os.path.exists(DLL_MAP):
        for ln in open(DLL_MAP, errors="ignore"):
            m = re.match(r"\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s+f\s", ln)
            if m:
                syms[int(m.group(2), 16)] = m.group(1)
    return syms

SYMS = load_symbols()

def sym_for(va):
    if va in SYMS:
        return SYMS[va]
    # nearest preceding symbol within 0x1000 (for offset annotation)
    best = None
    for a, n in SYMS.items():
        if a <= va < a + 0x1000 and (best is None or a > best[0]):
            best = (a, n)
    if best:
        return f"{best[1]}+0x{va - best[0]:X}"
    return None

def region(va):
    if DLL_BASE <= va < DLL_BASE + 0x900000:
        return "RB3Enhanced.dll"
    if LOAD <= va < LOAD + 0xF00000:
        return "default.xex"
    return "?"


# ---- console image cache ----------------------------------------------------

def ensure_base():
    if os.path.exists(BASE_CACHE) and os.path.getsize(BASE_CACHE) > 0:
        return open(BASE_CACHE, "rb").read()
    os.makedirs(os.path.dirname(BASE_CACHE), exist_ok=True)
    xex = BASE_CACHE + ".xex"
    sys.stderr.write("[xdbg] base cache missing; pulling default.xex from console...\n")
    r = subprocess.run(["lftp", "-u", "xboxftp,xboxftp", "-e",
                        f"set net:timeout 10; get /Usb0/Games/rb3/default.xex -o {xex}; bye", XBOX])
    if r.returncode != 0 or not os.path.exists(xex):
        sys.exit("[xdbg] failed to pull default.xex (is Aurora/FTP up?)")
    sys.stderr.write("[xdbg] extracting base image (xextool -b)...\n")
    subprocess.run(["wine", XEXTOOL, "-b", BASE_CACHE, xex],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    if not os.path.exists(BASE_CACHE):
        sys.exit("[xdbg] xextool -b failed")
    return open(BASE_CACHE, "rb").read()

_BASE = None
def base():
    global _BASE
    if _BASE is None:
        _BASE = ensure_base()
    return _BASE

def at(va, n):
    o = va - LOAD
    return base()[o:o + n]


# ---- disassembly ------------------------------------------------------------

def _annot(ins):
    """Annotate branch targets with symbols."""
    if ins.mnemonic in ("bl", "b", "beq", "bne", "blt", "bgt", "ble", "bge") and ins.op_str.startswith("0x"):
        try:
            tgt = int(ins.op_str.split(",")[-1].strip(), 16)
        except ValueError:
            return ""
        s = sym_for(tgt)
        return f"   ; {s}" if s else ""
    return ""

def dis(va, n, fault=None):
    if _MD is None:
        sys.exit("[xdbg] capstone not available — run under xex-patcher/.venv")
    code = at(va, n * 4)
    for ins in _MD.disasm(code, va):
        mark = "  <== FAULT" if ins.address == fault else ""
        print(f"  {ins.address:08X}  {ins.bytes.hex().upper():8}  {ins.mnemonic:8}{ins.op_str}{_annot(ins)}{mark}")

def find_fn_start(va, limit=0x800):
    """Heuristic: scan back for `mflr r12` (7D8802A6), the standard prologue head."""
    a = va
    while a > va - limit:
        if at(a, 4) == bytes.fromhex("7D8802A6"):
            return a
        a -= 4
    return None

def cmd_dis(args):
    va = int(args[0], 16); n = int(args[1]) if len(args) > 1 else 32
    s = sym_for(va)
    print(f"# {region(va)}  0x{va:08X}" + (f"  ({s})" if s else ""))
    dis(va, n)

def cmd_fn(args):
    va = int(args[0], 16)
    start = find_fn_start(va) or va
    s = sym_for(start)
    print(f"# function @ 0x{start:08X}" + (f"  ({s})" if s else "") + f"  [{region(start)}]")
    # disasm until blr/next-prologue (cap 0x200 instrs)
    code = at(start, 0x200 * 4)
    for ins in _MD.disasm(code, start):
        mark = "  <== target" if ins.address == va else ""
        print(f"  {ins.address:08X}  {ins.bytes.hex().upper():8}  {ins.mnemonic:8}{ins.op_str}{_annot(ins)}{mark}")
        if ins.mnemonic == "blr" and ins.address > start + 8:
            break

def cmd_xref(args):
    va = int(args[0], 16)
    data = base()
    hits = []
    for off in range(0, len(data) - 4, 4):
        w = struct.unpack(">I", data[off:off + 4])[0]
        if (w & 0xFC000003) == 0x48000001:      # bl with (AA=0)
            disp = w & 0x03FFFFFC
            if disp & 0x02000000:
                disp -= 0x04000000
            src = LOAD + off
            if src + disp == va:
                hits.append(src)
    print(f"# {len(hits)} call sites of 0x{va:08X}" + (f" ({sym_for(va)})" if sym_for(va) else ""))
    for h in hits:
        s = sym_for(h)
        print(f"  bl from 0x{h:08X}" + (f"  ({s})" if s else ""))

def cmd_str(args):
    va = int(args[0], 16)
    b = at(va, 128); z = b.find(b"\x00")
    print(repr(b[:z if z >= 0 else 128]))


# ---- XBDM live --------------------------------------------------------------

def xbdm(cmd):
    out = subprocess.run(["python3", XBDM, XBOX, cmd], capture_output=True, text=True).stdout
    return out

def parse_ctx(text):
    """Parse getcontext output into {reg: value}. XBDM prints Gpr as 0q<16hex>."""
    regs = {}
    for m in re.finditer(r"(Iar|Lr|Ctr|Msr|Cr|Xer|Gpr\d+)=0[qx]([0-9a-fA-F]+)", text):
        v = int(m.group(2), 16) & 0xFFFFFFFF     # low 32 bits (PPC32 view)
        regs[m.group(1)] = v
    return regs

def cmd_regs(args):
    thread = args[0] if args else "0xf9000000"
    r = parse_ctx(xbdm(f"getcontext thread={thread} control int"))
    for k in ["Iar", "Lr"] + [f"Gpr{i}" for i in range(32)]:
        if k in r:
            print(f"  {k:6} = 0x{r[k]:08X}" + (f"   ; {sym_for(r[k])}" if sym_for(r[k]) else ""))

def getmem(va, n):
    out = xbdm(f"getmem addr=0x{va:08X} length=0x{n:X}")
    # XBDM returns lines of hex bytes; collect them
    data = b""
    for ln in out.splitlines():
        ln = ln.strip()
        if re.fullmatch(r"[0-9a-fA-F]+", ln) and len(ln) % 2 == 0:
            data += bytes.fromhex(ln)
    return data

def cmd_mem(args):
    va = int(args[0], 16); n = int(args[1], 0) if len(args) > 1 else 0x80
    data = getmem(va, n)
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        words = " ".join(chunk[j:j + 4].hex().upper() for j in range(0, len(chunk), 4))
        asc = "".join(chr(c) if 32 <= c < 127 else "." for c in chunk)
        ann = ""
        if len(chunk) >= 4:
            w = struct.unpack(">I", chunk[:4])[0]
            if sym_for(w):
                ann = f"  ; +0={sym_for(w)}"
        print(f"  {va+i:08X}  {words:35}  {asc}{ann}")

def cmd_deref(args):
    """Follow a live pointer chain: deref VA, then apply each +offset hop."""
    va = int(args[0], 16)
    offs = [int(x, 0) for x in args[1:]]
    cur = va
    print(f"  start 0x{cur:08X}")
    for i, off in enumerate(offs):
        data = getmem(cur + off, 4)
        if len(data) < 4:
            print(f"  +0x{off:X} -> <unreadable>"); return
        nxt = struct.unpack(">I", data)[0]
        s = sym_for(nxt)
        print(f"  *(0x{cur:08X}+0x{off:X}) = 0x{nxt:08X}" + (f"  ({s})" if s else "") +
              ("  <== NULL" if nxt == 0 else ""))
        cur = nxt
        if cur == 0:
            return

def cmd_sym(args):
    q = args[0]
    if q.startswith("0x"):
        va = int(q, 16); s = sym_for(va)
        print(f"0x{va:08X} = {s or '<unknown>'}  [{region(va)}]")
    else:
        for a, n in sorted(SYMS.items()):
            if q.lower() in n.lower():
                print(f"  0x{a:08X}  {n}")

def cmd_crash(args):
    """Capture the current first-chance fault end to end."""
    thread = args[0] if args else "0xf9000000"
    r = parse_ctx(xbdm(f"getcontext thread={thread} control int"))
    if "Iar" not in r:
        print("no fault context (is the console stopped at a first-chance exception?)")
        print(xbdm("getexecstate"))
        return
    pc = r["Iar"]
    print(f"== FAULT @ 0x{pc:08X}  [{region(pc)}]" + (f"  {sym_for(pc)}" if sym_for(pc) else "") + " ==")
    print(f"   Lr=0x{r.get('Lr',0):08X}  sp=0x{r.get('Gpr1',0):08X}")
    print("-- registers (nonzero) --")
    for i in range(32):
        v = r.get(f"Gpr{i}")
        if v:
            print(f"   r{i:<2}=0x{v:08X}" + (f"  {sym_for(v)}" if sym_for(v) else ""))
    if LOAD <= pc < LOAD + 0xF00000:
        print("-- fault instruction + context --")
        start = find_fn_start(pc) or (pc - 0x20)
        if sym_for(start):
            print(f"   (in {sym_for(start)} @ 0x{start:08X})")
        dis(max(start, pc - 0x20), 16, fault=pc)
    # stack walk: scan the stack for return addresses into code
    sp = r.get("Gpr1", 0)
    if sp:
        print("-- stack return addresses (default.xex / RB3Enhanced.dll) --")
        stk = getmem(sp, 0x200)
        seen = set()
        for i in range(0, len(stk) - 4, 4):
            w = struct.unpack(">I", stk[i:i + 4])[0]
            if (LOAD <= w < LOAD + 0xF00000 or DLL_BASE <= w < DLL_BASE + 0x900000) and w not in seen:
                seen.add(w)
                print(f"   0x{w:08X}  [{region(w)}]" + (f"  {sym_for(w)}" if sym_for(w) else ""))


CMDS = {"crash": cmd_crash, "dis": cmd_dis, "fn": cmd_fn, "xref": cmd_xref,
        "mem": cmd_mem, "deref": cmd_deref, "str": cmd_str, "sym": cmd_sym, "regs": cmd_regs}

def main():
    if len(sys.argv) < 2 or sys.argv[1] not in CMDS:
        print(__doc__)
        sys.exit(0 if len(sys.argv) < 2 else 2)
    CMDS[sys.argv[1]](sys.argv[2:])

if __name__ == "__main__":
    main()
