"""Type-aware object memory fixture generation.

Generates typed object memory from struct_db class layouts so that
member fields contain type-appropriate values (valid floats, NULL pointers,
zeroed containers) rather than arbitrary fill bytes. This eliminates
false-positive divergences caused by uninitialized memory interpretation.

Validated: 97.7% of divergent functions flip to equivalent (84/86),
with 0 regressions, across 7 units.
"""

import re
import struct

from .memory_map import REGION_SIZE, VTABLE_BASE


# Scalar type → byte size
TYPE_SIZES = {
    'bool': 1, 'unsigned char': 1, 'char': 1, 'signed char': 1,
    'short': 2, 'unsigned short': 2, 'signed short': 2,
    'int': 4, 'unsigned int': 4, 'long': 4, 'unsigned long': 4, 'float': 4,
    'double': 8, 'long long': 8, 'unsigned long long': 8,
}

# Types that should be zeroed (containers, smart pointers)
ZERO_PREFIXES = (
    'class vector', 'class list', 'class ObjPtr', 'class ObjOwnerPtr',
    'class ObjPtrList', 'class String', 'class Symbol', 'class Timer',
)


def normalize_type(type_str):
    """Strip const/volatile/class prefixes for size lookup."""
    t = type_str.strip()
    t = re.sub(r'\b(const|volatile)\b', '', t).strip()
    t = re.sub(r'^(class|struct)\s+', '', t)
    return t


def infer_size(member, next_offset, type_str):
    """Infer member size from offset gap or type heuristics."""
    if next_offset is not None:
        gap = next_offset - member['offset']
        if gap > 0:
            return gap
    norm = normalize_type(type_str)
    if norm in TYPE_SIZES:
        return TYPE_SIZES[norm]
    if '*' in type_str or '&' in type_str:
        return 4
    if 'Color' in type_str:
        return 16
    if 'Vector3' in type_str:
        return 12
    if 'Vector2' in type_str:
        return 8
    if 'Quat' in type_str or 'Hmx::Quat' in type_str:
        return 16
    if 'Transform' in type_str:
        return 64
    return 4  # default pointer-sized


def fill_member(mem, offset, type_str, size, rng):
    """Fill a single member with a type-appropriate value at offset."""
    if offset + size > len(mem):
        return

    norm = normalize_type(type_str)

    # Pointers → NULL
    if '*' in type_str or '&' in type_str:
        struct.pack_into(">I", mem, offset, 0)
        return

    # Containers and smart pointers → zero
    for prefix in ZERO_PREFIXES:
        if type_str.startswith(prefix):
            mem[offset:offset + size] = b'\x00' * size
            return

    # Enum → 0
    if type_str.startswith('enum '):
        struct.pack_into(">I", mem, offset, 0)
        return

    # Bool
    if norm == 'bool' or (norm == 'unsigned char' and size == 1):
        mem[offset] = rng.choice([0, 1])
        return

    # Float — valid non-NaN
    if norm == 'float':
        val = rng.choice([0.0, 1.0, -1.0, 0.5, 2.0])
        struct.pack_into(">f", mem, offset, val)
        return

    # Double
    if norm == 'double':
        val = rng.choice([0.0, 1.0, -1.0])
        struct.pack_into(">d", mem, offset, val)
        return

    # Color → valid RGBA
    if 'Color' in type_str:
        for i in range(min(4, size // 4)):
            struct.pack_into(">f", mem, offset + i * 4, 1.0)
        return

    # Vector3 → valid
    if 'Vector3' in type_str:
        for i in range(min(3, size // 4)):
            struct.pack_into(">f", mem, offset + i * 4, 0.0)
        return

    # Signed integers
    if norm in ('int', 'long', 'signed int', 'signed long'):
        val = rng.choice([0, 1, -1, 2, 10])
        struct.pack_into(">i", mem, offset, val)
        return

    # Unsigned integers
    if norm in ('unsigned int', 'unsigned long'):
        val = rng.choice([0, 1, 2, 10])
        struct.pack_into(">I", mem, offset, val)
        return

    # Short
    if norm in ('short', 'signed short'):
        val = rng.choice([0, 1, -1])
        struct.pack_into(">h", mem, offset, val)
        return

    if norm == 'unsigned short':
        val = rng.choice([0, 1, 2])
        struct.pack_into(">H", mem, offset, val)
        return

    # Char
    if norm in ('char', 'signed char'):
        mem[offset] = rng.choice([0, 1])
        return

    if norm == 'unsigned char':
        mem[offset] = rng.choice([0, 1, 0xFF])
        return

    # Default: leave as fill byte (unknown type)


def extract_class_from_symbol(mangled):
    """Extract class name from MSVC mangled symbol.

    ?Method@ClassName@@... → 'ClassName'
    ??0ClassName@@...      → 'ClassName' (constructor)
    ??1ClassName@@...      → 'ClassName' (destructor)
    """
    # Constructor/destructor: ??0ClassName@@ or ??1ClassName@@
    m = re.match(r'\?\?[0-9](\w+)@@', mangled)
    if m:
        return m.group(1)
    # Regular method: ?Method@ClassName@@
    m = re.match(r'\?\w+@(\w+)@@', mangled)
    if m:
        return m.group(1)
    return None


def extract_class_from_unit(unit_name):
    """Extract primary class name from unit path.

    'default/system/world/LightPreset' → 'LightPreset'
    'default/lazer/meta_ham/CampaignPerformer' → 'CampaignPerformer'
    """
    if not unit_name:
        return None
    # Last component of the path
    return unit_name.rstrip('/').rsplit('/', 1)[-1]


def generate_sentinel_object(size=REGION_SIZE):
    """Generate sentinel memory where each 4-byte word encodes its own offset.

    value = OBJECT_BASE + offset. Vtable pointer preserved at offset 0.
    Used by the field access prober to detect which struct offsets are read.

    Args:
        size: Total memory region size (default: REGION_SIZE = 64KB)

    Returns:
        bytearray of `size` bytes with sentinel pattern.
    """
    from .memory_map import OBJECT_BASE

    mem = bytearray(size)
    for off in range(0, size, 4):
        struct.pack_into(">I", mem, off, OBJECT_BASE + off)
    # Preserve vtable pointer at offset 0
    struct.pack_into(">I", mem, 0, VTABLE_BASE)
    return mem


def generate_typed_object(class_name, db, rng, fill_byte=0x00, size=REGION_SIZE):
    """Generate type-aware object memory from struct_db class layout.

    Args:
        class_name: Class name to look up in db
        db: Connected StructDB instance
        rng: random.Random instance for reproducible values
        fill_byte: Base fill byte for unknown regions
        size: Total memory region size

    Returns:
        bytearray of `size` bytes, or None if class not found in db.
    """
    info = db.get_class_info(class_name)
    if not info:
        return None

    # Collect all members including inherited
    all_members = list(info['members'])
    chain = db.resolve_inheritance_chain(class_name)
    for parent in chain:
        if parent == 'virtual':
            continue
        pinfo = db.get_class_info(parent)
        if pinfo and pinfo['members']:
            all_members.extend(pinfo['members'])

    if not all_members:
        return None

    # Sort by offset, deduplicate (parent and child may declare same offset)
    all_members.sort(key=lambda m: m['offset'])
    seen_offsets = set()
    unique_members = []
    for m in all_members:
        if m['offset'] not in seen_offsets:
            seen_offsets.add(m['offset'])
            unique_members.append(m)
    all_members = unique_members

    # Start with fill byte
    mem = bytearray([fill_byte]) * size

    # Vtable pointer at offset 0 (engine also writes this, but be consistent)
    struct.pack_into(">I", mem, 0, VTABLE_BASE)

    # Fill known members with type-appropriate values
    for i, member in enumerate(all_members):
        next_offset = all_members[i + 1]['offset'] if i + 1 < len(all_members) else None
        member_size = infer_size(member, next_offset, member['type_str'])
        fill_member(mem, member['offset'], member['type_str'], member_size, rng)

    return mem
