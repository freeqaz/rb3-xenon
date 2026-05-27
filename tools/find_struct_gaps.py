#!/usr/bin/env python3
"""Scan headers for struct layout mismatches by comparing offset comments to expected sizeof."""
import re, sys, os
from pathlib import Path

# Known type sizes (ILP32)
TYPE_SIZES = {
    'bool': 1, 'char': 1, 'unsigned char': 1, 'signed char': 1, 'u8': 1, 's8': 1,
    'short': 2, 'unsigned short': 2, 'u16': 2, 's16': 2,
    'int': 4, 'unsigned int': 4, 'float': 4, 'u32': 4, 's32': 4,
    'long': 4, 'unsigned long': 4,
    'long long': 8, 'unsigned long long': 8, 'double': 8, 'u64': 8, 's64': 8,
    'Symbol': 4, 'DataNode': 16,
    'Vector2': 8, 'Vector3': 12, 'Vector4': 16,
    'Hmx::Color': 16, 'Color': 16,
    'Hmx::Quat': 16, 'Quat': 16,
    'XMVECTOR': 16, 'XMMATRIX': 64,
    'Transform': 64, 'Plane': 16,
    'String': 8, 'FilePath': 8,
    'DateTime': 8,
    'Sphere': 16, 'Box': 24,
}

# Parameterized type sizes
TEMPLATE_SIZES = {
    'ObjPtr': 0x14,
    'ObjOwnerPtr': 0x14,
    'ObjPtrList': 0xC,
    'ObjList': 0xC,
    'ObjDirPtr': 0x10,
    'ObjVector': 0xC,
}

def parse_offset(comment):
    """Extract hex offset from comment like // 0x38"""
    m = re.search(r'0x([0-9a-fA-F]+)', comment)
    if m:
        return int(m.group(1), 16)
    return None

def guess_size(type_str):
    """Guess the size of a type from its name."""
    type_str = type_str.strip()
    
    # Direct match
    if type_str in TYPE_SIZES:
        return TYPE_SIZES[type_str]
    
    # Pointer
    if type_str.endswith('*') or type_str.endswith('* const'):
        return 4
    
    # Enum (assume int)
    # We can't easily detect enums here, but many are 4 bytes
    
    # Template types
    for tmpl, size in TEMPLATE_SIZES.items():
        if type_str.startswith(tmpl + '<') or type_str.startswith(tmpl + ' <'):
            return size
    
    # std::vector
    if 'std::vector' in type_str or 'vector<' in type_str:
        return 0xC
    
    # std::list
    if 'std::list' in type_str or 'list<' in type_str:
        return 0xC
    
    # std::pair - can't easily determine
    
    return None

def parse_array_count(decl):
    """Extract array count from declaration like 'int foo[20]' or 'float bar[kNumJoints]'."""
    m = re.search(r'\[(\d+)\]', decl)
    if m:
        return int(m.group(1))
    # Known constants
    known = {
        'kNumJoints': 20, 'kNumBones': 19, 'kNumCoordSys': 8,
        'kMaxNumNormBones': 3, 'kMaxNumErrorNodes': 33,
    }
    m = re.search(r'\[(\w+)\]', decl)
    if m and m.group(1) in known:
        return known[m.group(1)]
    return None

# Regex for member with offset comment
MEMBER_RE = re.compile(
    r'^\s+'                          # leading whitespace
    r'(?:mutable\s+)?'              # optional mutable
    r'(?:static\s+)?'               # skip statics (they don't affect layout)
    r'([\w:<>, *&]+?)\s+'           # type
    r'(\w+)'                         # name
    r'(\[[\w]+\])?'                 # optional array
    r'\s*;'                          # semicolon
    r'\s*//\s*(0x[0-9a-fA-F]+)'    # offset comment
)

def scan_file(filepath):
    issues = []
    with open(filepath, encoding='utf-8', errors='replace') as f:
        lines = f.readlines()
    
    # Find struct/class blocks with offset-commented members
    members = []  # (line_no, type_str, name, array_count, offset, raw_line)
    
    current_struct = None
    brace_depth = 0
    
    for i, line in enumerate(lines):
        stripped = line.strip()
        
        # Track struct/class names
        m = re.match(r'(?:struct|class)\s+(\w+)', stripped)
        if m and ('{' in stripped or (i+1 < len(lines) and '{' in lines[i+1].strip())):
            current_struct = m.group(1)
            members = []
        
        # Track braces loosely
        brace_depth += stripped.count('{') - stripped.count('}')
        if brace_depth <= 0 and current_struct and members:
            # End of struct - analyze
            analyze_members(filepath, current_struct, members, issues)
            current_struct = None
            members = []
            brace_depth = 0
        
        if stripped.startswith('};') and current_struct and members:
            analyze_members(filepath, current_struct, members, issues)
            current_struct = None
            members = []
        
        # Skip static members, virtual methods, etc
        if 'static ' in stripped and '(' not in stripped:
            continue
        if 'virtual ' in stripped:
            continue
        if '(' in stripped:  # method declaration
            continue
            
        # Match member with offset
        m = MEMBER_RE.match(line)
        if m and current_struct:
            type_str = m.group(1).strip()
            name = m.group(2)
            array_part = m.group(3)
            offset = parse_offset(m.group(4))
            
            array_count = parse_array_count(line) if array_part else None
            members.append((i+1, type_str, name, array_count, offset, stripped))
    
    return issues

def analyze_members(filepath, struct_name, members, issues):
    """Check consecutive member pairs for gap mismatches."""
    for i in range(len(members) - 1):
        line1, type1, name1, arr1, off1, raw1 = members[i]
        line2, type2, name2, arr2, off2, raw2 = members[i+1]
        
        if off1 is None or off2 is None:
            continue
        
        actual_gap = off2 - off1
        if actual_gap <= 0:
            continue
        
        elem_size = guess_size(type1)
        if elem_size is None:
            continue
        
        if arr1:
            expected = elem_size * arr1
        else:
            expected = elem_size
        
        # Account for alignment padding (round up to align of next member)
        next_align = guess_size(type2)
        if next_align and next_align >= 4:
            expected_aligned = (expected + 3) & ~3  # 4-byte align
        elif next_align == 2:
            expected_aligned = (expected + 1) & ~1
        else:
            expected_aligned = expected
        
        if actual_gap != expected and actual_gap != expected_aligned:
            # Check if it's a known padded stride issue
            if arr1 and elem_size == 12 and actual_gap == arr1 * 16:
                pad_note = "LIKELY 16-BYTE STRIDE (needs PaddedJointPos or similar)"
            elif arr1:
                actual_stride = actual_gap / arr1
                pad_note = f"actual stride={actual_stride:.0f}, expected={elem_size}"
            else:
                pad_note = f"gap={actual_gap} (0x{actual_gap:x}), expected={expected} (0x{expected:x})"
            
            relpath = os.path.relpath(filepath, str(Path(__file__).resolve().parent.parent))
            issues.append({
                'file': relpath,
                'struct': struct_name,
                'member': name1,
                'type': type1,
                'array': arr1,
                'line': line1,
                'offset': off1,
                'next_offset': off2,
                'next_member': name2,
                'expected': expected,
                'actual_gap': actual_gap,
                'note': pad_note,
            })

def main():
    src_dir = Path(__file__).resolve().parent.parent / 'src'
    all_issues = []
    
    for hdr in sorted(src_dir.rglob('*.h')):
        issues = scan_file(str(hdr))
        all_issues.extend(issues)
    
    if not all_issues:
        print("No struct layout mismatches found!")
        return
    
    print(f"Found {len(all_issues)} potential struct layout mismatches:\n")
    for iss in all_issues:
        arr_str = f"[{iss['array']}]" if iss['array'] else ""
        print(f"  {iss['file']}:{iss['line']}  {iss['struct']}::{iss['member']}")
        print(f"    {iss['type']}{arr_str} @ 0x{iss['offset']:x} → next {iss['next_member']} @ 0x{iss['next_offset']:x}")
        print(f"    {iss['note']}")
        print()

if __name__ == '__main__':
    main()
