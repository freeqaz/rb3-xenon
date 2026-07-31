#!/usr/bin/env python3
"""Independent, from-first-principles reader for RB3 Xbox 360 .ark archives.

WHY THIS EXISTS
---------------
It is the *reference* side of the native port's archive tests. It deliberately
shares NO code with the C++ engine: it re-implements the v6 .hdr container, the
Rand2 stream cipher, the string heap and the multi-ark cumulative offset walk
directly from the format, in a different language. So when this and
`native/build/rb3-midi` agree on the bytes of an archive member, two genuinely
independent implementations agreed -- which is the only kind of agreement worth
anything. An in-process self-check where the same code produces and verifies a
value cannot fail for the reason we care about.

It is also the tool that lets an EXTERNAL checker adjudicate: dump bytes here,
dump bytes from the engine, and let coreutils `sha256sum`/`cmp` decide. Neither
program's own opinion of itself is consulted.

FORMAT (from src/system/os/Archive.cpp + utl/BinStream.cpp, little-endian)
-------------------------------------------------------------------------
  u32  seed                      <- PLAINTEXT. BinStream::EnableReadEncryption()
                                    reads this before any decryption, so it is
                                    the one field not XORed.
  ...everything below is XORed byte-by-byte with (u8)Rand2::Int()...
  i32  version                   (must be 6)
  i32  guid[4]
  i32  numArkfiles
  vec  arkfileSizes    : u32
  vec  arkfileNames    : String  (i32 length, then that many bytes, NO NUL)
  vec  arkfileCachePriority : i32
  i32  heapSize; u8 heap[heapSize]      <- NUL-terminated strings
  i32  tableSize; i32 tableOffsets[tableSize]   <- offset 0 means "empty slot"
  vec  fileEntries : { u64 offset; i32 hashedName; i32 hashedPath;
                       i32 size; i32 ucSize }

  `hashedName`/`hashedPath` are INDICES INTO tableOffsets, not hash values to be
  recomputed -- which is why this tool never needs ArkHash::GetHashValue. Every
  entry can be resolved by pure table lookup, so we can enumerate the whole
  archive rather than probe it path by path.

  `offset` is GLOBAL across the concatenated ark parts. Locating a member means
  walking the cumulative part sizes; songs live past 2^31, so this arithmetic
  must be 64-bit (Python ints are, natively -- the C++ side is where that is a
  real hazard, and comparing against this is how we detect it).

USAGE
  ark_extract.py <dataDir> --list [globPattern]
  ark_extract.py <dataDir> --info <arkPath>
  ark_extract.py <dataDir> --extract <arkPath> --out <file>
Exit codes: 0 ok, 1 not found / error.
"""

import argparse
import fnmatch
import os
import struct
import sys


class Rand2:
    """MINSTD / Lehmer PRNG -- transcribed from src/system/math/Rand2.cpp.

    Re-derived here rather than shared so a bug in the engine's copy cannot hide
    by being used on both sides of the comparison.
    """

    def __init__(self, seed):
        # Rand2::Rand2(int): 0 -> 1, negative -> negated.
        seed = struct.unpack("<i", struct.pack("<I", seed & 0xFFFFFFFF))[0]
        if seed == 0:
            self.seed = 1
        elif seed < 0:
            self.seed = -seed
        else:
            self.seed = seed

    def next(self):
        s = self.seed
        test = ((s % 127773) * 16807) - ((s // 127773) * 2836)
        # C truncates toward zero; s is always positive here so // matches.
        if test > 0:
            self.seed = test
        else:
            self.seed = test + 0x7FFFFFFF
        return self.seed


class Reader:
    """Cursor over the decrypted header image."""

    def __init__(self, buf):
        self.buf = buf
        self.pos = 0

    def take(self, n):
        if self.pos + n > len(self.buf):
            raise ValueError(
                "header truncated: wanted %d at %d of %d" % (n, self.pos, len(self.buf))
            )
        b = self.buf[self.pos:self.pos + n]
        self.pos += n
        return b

    def i32(self):
        return struct.unpack("<i", self.take(4))[0]

    def u32(self):
        return struct.unpack("<I", self.take(4))[0]

    def u64(self):
        return struct.unpack("<Q", self.take(8))[0]

    def string(self):
        n = self.i32()
        if n < 0 or n > 10000:
            raise ValueError("implausible string length %d at %d" % (n, self.pos))
        return self.take(n).decode("latin-1")

    def vector(self, fn):
        n = self.u32()
        return [fn() for _ in range(n)]


class Ark:
    def __init__(self, data_dir, basename="gen/main_xbox"):
        self.data_dir = data_dir
        self.basename = basename
        self._read_header()

    def _read_header(self):
        hdr_path = os.path.join(self.data_dir, self.basename + ".hdr")
        with open(hdr_path, "rb") as f:
            raw = f.read()
        if len(raw) < 8:
            raise ValueError("%s too small to be an .hdr" % hdr_path)

        # The seed is plaintext; everything after it is XOR-decrypted.
        seed = struct.unpack("<I", raw[0:4])[0]
        rng = Rand2(seed)
        body = bytearray(raw[4:])
        for i in range(len(body)):
            body[i] ^= rng.next() & 0xFF

        r = Reader(bytes(body))
        self.version = r.i32()
        if self.version != 6:
            raise ValueError("unsupported archive version %d" % self.version)
        # HxGuid's stream operator (utl/HxGuid.cpp:55) reads a REVISION int
        # before the four data words -- so a guid on the wire is 20 bytes, not
        # 16. Missing this desynchronises everything after it; it was caught by
        # the arkfile-name strings decoding as a 2-billion-byte length rather
        # than by anything subtle, which is the good case.
        self.guid_rev = r.i32()
        self.guid = [r.i32() for _ in range(4)]
        self.num_arkfiles = r.i32()
        self.arkfile_sizes = r.vector(r.u32)
        self.arkfile_names = r.vector(r.string)
        self.cache_priority = r.vector(r.i32)

        heap_size = r.i32()
        heap = r.take(heap_size)
        table_size = r.i32()
        table = [r.i32() for _ in range(table_size)]

        def resolve(idx):
            if idx < 0 or idx >= table_size:
                return None
            off = table[idx]
            if off == 0:
                return None
            end = heap.find(b"\0", off)
            if end < 0:
                end = len(heap)
            return heap[off:end].decode("latin-1")

        n_entries = r.u32()
        self.entries = {}
        self.entry_list = []
        for _ in range(n_entries):
            offset = r.u64()
            hname = r.i32()
            hpath = r.i32()
            size = r.i32()
            ucsize = r.i32()
            name = resolve(hname)
            path = resolve(hpath)
            if name is None:
                continue
            full = name if not path else path + "/" + name
            rec = {
                "path": full,
                "offset": offset,
                "size": size,
                "ucsize": ucsize,
            }
            self.entries[full] = rec
            self.entry_list.append(rec)

    def locate(self, ark_path):
        """Global offset -> (part index, offset within that part).

        Mirrors Archive::GetFileInfo's cumulative walk, but computed here from
        the part sizes alone. Independent arithmetic on independently parsed
        data: if the engine truncates to 32 bits, these disagree.
        """
        rec = self.entries.get(ark_path)
        if rec is None:
            return None
        cumulative = 0
        for part in range(self.num_arkfiles):
            end = cumulative + self.arkfile_sizes[part]
            if rec["offset"] < end:
                return {
                    "part": part,
                    "part_name": self.arkfile_names[part],
                    "local_offset": rec["offset"] - cumulative,
                    "global_offset": rec["offset"],
                    "size": rec["size"],
                    "ucsize": rec["ucsize"],
                }
            cumulative = end
        return None

    def read(self, ark_path):
        loc = self.locate(ark_path)
        if loc is None:
            return None, None
        if loc["ucsize"] != 0:
            # Not a silent fallback: the native ArkFile read path has only ever
            # been exercised on stored-uncompressed members. Refuse loudly
            # rather than emit bytes that would make a comparison meaningless.
            raise ValueError(
                "%s is COMPRESSED (ucsize=%d); this reference tool only handles "
                "stored-uncompressed members" % (ark_path, loc["ucsize"])
            )
        part_file = os.path.join(self.data_dir, loc["part_name"])
        with open(part_file, "rb") as f:
            f.seek(loc["local_offset"])
            data = f.read(loc["size"])
        if len(data) != loc["size"]:
            raise ValueError(
                "short read: got %d of %d from %s" % (len(data), loc["size"], part_file)
            )
        return data, loc


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("data_dir")
    ap.add_argument("--basename", default="gen/main_xbox")
    ap.add_argument("--list", nargs="?", const="*", metavar="GLOB")
    ap.add_argument("--info")
    ap.add_argument("--extract")
    ap.add_argument("--out")
    args = ap.parse_args()

    ark = Ark(args.data_dir, args.basename)

    if args.list is not None:
        hits = [e for e in ark.entry_list if fnmatch.fnmatch(e["path"], args.list)]
        hits.sort(key=lambda e: e["path"])
        for e in hits:
            print("%-64s size=%-10d ucsize=%-8d off=%d"
                  % (e["path"], e["size"], e["ucsize"], e["offset"]))
        print("# %d entr%s matched (%d total in archive, %d ark parts)"
              % (len(hits), "y" if len(hits) == 1 else "ies",
                 len(ark.entry_list), ark.num_arkfiles))
        return 0

    if args.info:
        loc = ark.locate(args.info)
        if loc is None:
            print("NOT FOUND: %s" % args.info, file=sys.stderr)
            return 1
        print("path          : %s" % args.info)
        print("ark part      : %d (%s)" % (loc["part"], loc["part_name"]))
        print("offset in part: %d (0x%x)" % (loc["local_offset"], loc["local_offset"]))
        print("global offset : %d" % loc["global_offset"])
        print("size          : %d" % loc["size"])
        print("uncompressed  : %d" % loc["ucsize"])
        return 0

    if args.extract:
        if not args.out:
            print("--extract requires --out", file=sys.stderr)
            return 1
        data, loc = ark.read(args.extract)
        if data is None:
            print("NOT FOUND: %s" % args.extract, file=sys.stderr)
            return 1
        with open(args.out, "wb") as f:
            f.write(data)
        print("extracted %s -> %s (%d bytes, part %d @ %d)"
              % (args.extract, args.out, len(data), loc["part"], loc["local_offset"]))
        return 0

    ap.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
