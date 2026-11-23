#!/usr/bin/env python3
"""Translate rb3-app's generated ELF stub inventory to Darwin assembly."""

import re
import sys
from pathlib import Path


# libstdc++ and libc++ mangle the vector parameters differently.
LIBCXX_NAMES = {
    "_ZN12VocalOverlay14AppendPartDataERKSt6vectorIP9VocalPartSaIS2_EE":
        "_ZN12VocalOverlay14AppendPartDataERKNSt3__16vectorIP9VocalPartNS0_9allocatorIS3_EEEE",
    "_ZN12VocalOverlay18AppendAssignedPartEPK6SingerRKSt6vectorIP9VocalPartSaIS5_EE":
        "_ZN12VocalOverlay18AppendAssignedPartEPK6SingerRKNSt3__16vectorIP9VocalPartNS3_9allocatorIS6_EEEE",
    "_ZN13QuazalSession15HaveClientsLeftERSt6vectorIiSaIiEE":
        "_ZN13QuazalSession15HaveClientsLeftERNSt3__16vectorIiNS0_9allocatorIiEEEE",
}


def convert(source):
    out = []
    for line in source.splitlines():
        if not line.strip() or line.startswith("//"):
            out.append(line)
        elif line.startswith(".section .rodata."):
            out.append(".section __TEXT,__cstring,cstring_literals")
        elif line.startswith(".section .data.rel.ro."):
            out.append(".section __DATA,__const")
        elif line.startswith((".section .note.GNU-stack", ".type ")):
            continue
        elif line.startswith(".L"):
            out.append(line[1:])
        elif line.startswith(".globl "):
            name = line.removeprefix(".globl ")
            out.append(".globl _" + LIBCXX_NAMES.get(name, name))
        elif " // ring: " in line:
            name, note = line.split(":", 1)
            out.append("_" + LIBCXX_NAMES.get(name, name) + ":" + note)
        elif match := re.fullmatch(r"    leaq \.(L\w+)\(%rip\), %(rdi|rsi)", line):
            label, register = match.groups()
            arm = {"rdi": "x0", "rsi": "x1"}[register]
            out.extend(("#if defined(__aarch64__)", f"    adrp {arm}, {label}@PAGE",
                        f"    add {arm}, {arm}, {label}@PAGEOFF", "#else",
                        f"    leaq {label}(%rip), %{register}", "#endif"))
        elif line == "    jmp rb3_app_stub_reached":
            out.extend(("#if defined(__aarch64__)", "    b _rb3_app_stub_reached",
                        "#else", "    jmp _rb3_app_stub_reached", "#endif"))
        elif match := re.fullmatch(r"    \.quad (_Z\w+|rb3_app_vcall_unreached)(\+16)?", line):
            out.append("    .quad _" + match[1] + (match[2] or ""))
        elif line.startswith("    .quad .L"):
            out.append(line.replace(".quad .L", ".quad L", 1))
        elif line.startswith((".text", ".p2align ", "    .rept ", "    .endr", "    .quad 0, 0")):
            out.append(line)
        else:
            raise ValueError(f"Unrecognized stub assembly: {line}")
    out.append(".subsections_via_symbols")
    return "\n".join(out) + "\n"


if __name__ == "__main__":
    source, output = map(Path, sys.argv[1:])
    output.write_text(convert(source.read_text()))
