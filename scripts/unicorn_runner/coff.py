"""Minimal COFF parser for MSVC Xbox 360 PPC .obj files."""

import struct

IMAGE_FILE_MACHINE_POWERPCBE = 0x01F2

# IMAGE_REL_PPC_* relocation types (from PE/COFF spec + Xbox 360 extensions)
RELOC_NAMES = {
    0x0000: "ABSOLUTE",
    0x0001: "ADDR64",
    0x0002: "ADDR32",
    0x0003: "ADDR24",
    0x0004: "ADDR16",
    0x0005: "ADDR14",
    0x0006: "REL24",
    0x0007: "REL14",
    0x0008: "TOCREL16",
    0x0009: "TOCREL14",
    0x000A: "ADDR32NB",
    0x000B: "SECREL",
    0x000C: "SECTION",
    0x000D: "IFGLUE",
    0x000E: "IMGLUE",
    0x000F: "SECREL16",
    0x0010: "REFHI",
    0x0011: "REFLO",
    0x0012: "PAIR",
    0x0013: "SECRELLO",
    0x0014: "SECRELHI",
    # Xbox 360 specific
    0x0015: "GPREL",
    0x0016: "TOKEN",
}


class COFFParser:
    """Minimal COFF parser for MSVC Xbox 360 PPC .obj files."""

    def __init__(self, filepath):
        with open(filepath, "rb") as f:
            self.data = f.read()
        self.filepath = filepath
        self._parse_header()
        self._parse_sections()
        self._parse_symbols()

    def _parse_header(self):
        # COFF header: 20 bytes
        (self.machine, self.num_sections, self.timestamp,
         self.symtab_offset, self.num_symbols, self.opthdr_size,
         self.characteristics) = struct.unpack_from("<HHIIIHH", self.data, 0)

        assert self.machine == IMAGE_FILE_MACHINE_POWERPCBE, \
            f"Not a PPC BE COFF: machine=0x{self.machine:04X}"

    def _parse_sections(self):
        self.sections = []
        offset = 20 + self.opthdr_size  # After COFF header + optional header

        for i in range(self.num_sections):
            sec = {}
            name_bytes = self.data[offset:offset+8]
            # Handle long names (starts with /)
            if name_bytes[0:1] == b'/':
                str_offset = int(name_bytes[1:].rstrip(b'\x00').decode('ascii'))
                strtab_base = self.symtab_offset + self.num_symbols * 18
                end = self.data.index(b'\x00', strtab_base + str_offset)
                sec['name'] = self.data[strtab_base + str_offset:end].decode('ascii')
            else:
                sec['name'] = name_bytes.rstrip(b'\x00').decode('ascii')

            (sec['vsize'], sec['vaddr'], sec['raw_size'], sec['raw_offset'],
             sec['reloc_offset'], sec['linenum_offset'], sec['num_relocs'],
             sec['num_linenums'], sec['characteristics']) = struct.unpack_from(
                "<IIIIIIHHI", self.data, offset + 8)

            sec['index'] = i + 1  # 1-based
            self.sections.append(sec)
            offset += 40

    def _parse_symbols(self):
        self.symbols = []
        self.symbol_map = {}  # name -> symbol
        self._symbol_by_index = {}  # COFF table index -> symbol (handles aux gaps)
        self._reloc_cache = {}  # section_idx -> list of reloc dicts
        strtab_base = self.symtab_offset + self.num_symbols * 18

        i = 0
        while i < self.num_symbols:
            off = self.symtab_offset + i * 18
            name_bytes = self.data[off:off+8]

            # Symbol name: if first 4 bytes are zero, it's a string table reference
            if name_bytes[:4] == b'\x00\x00\x00\x00':
                str_offset = struct.unpack_from("<I", name_bytes, 4)[0]
                end = self.data.index(b'\x00', strtab_base + str_offset)
                name = self.data[strtab_base + str_offset:end].decode('ascii', errors='replace')
            else:
                name = name_bytes.rstrip(b'\x00').decode('ascii', errors='replace')

            value, sec_num, sym_type, storage_class, num_aux = struct.unpack_from(
                "<IhHBB", self.data, off + 8)

            sym = {
                'name': name,
                'value': value,
                'section': sec_num,
                'type': sym_type,
                'storage_class': storage_class,
                'num_aux': num_aux,
                'index': i,
            }
            self.symbols.append(sym)
            self.symbol_map[name] = sym
            self._symbol_by_index[i] = sym

            # Skip aux symbols
            i += 1 + num_aux

        # Build lookup caches for coloader performance
        self._section_names = frozenset(s['name'] for s in self.sections)
        self._symbols_by_section_offset = {}  # (sec_num, offset) -> symbol name
        for sym in self.symbols:
            if sym['section'] > 0 and not sym['name'].startswith('$'):
                key = (sym['section'], sym['value'])
                # First non-internal symbol at this offset wins
                if key not in self._symbols_by_section_offset:
                    self._symbols_by_section_offset[key] = sym['name']

    def get_section_relocations(self, section_idx):
        """Get relocations for a section (0-based index). Results are cached."""
        cached = self._reloc_cache.get(section_idx)
        if cached is not None:
            return cached
        sec = self.sections[section_idx]
        relocs = []
        for i in range(sec['num_relocs']):
            off = sec['reloc_offset'] + i * 10
            vaddr, sym_idx, reloc_type = struct.unpack_from("<IIH", self.data, off)
            sym = self._symbol_by_index.get(sym_idx, {'name': f'<sym#{sym_idx}>'})
            relocs.append({
                'offset': vaddr,
                'symbol_index': sym_idx,
                'symbol_name': sym['name'],
                'type': reloc_type,
                'type_name': RELOC_NAMES.get(reloc_type, f"UNKNOWN(0x{reloc_type:04X})"),
            })
        self._reloc_cache[section_idx] = relocs
        return relocs

    def get_text_sections(self):
        """Return all .text sections."""
        return [s for s in self.sections if s['name'].startswith('.text')]

    def get_section_data(self, section_idx):
        """Get raw bytes for a section."""
        sec = self.sections[section_idx]
        return self.data[sec['raw_offset']:sec['raw_offset'] + sec['raw_size']]
