#!/usr/bin/env python3
"""
Struct offset resolution tool for DC3 decomp.

Parses annotated headers to build a struct offset lookup database.
When objdiff reports offset mismatches, agents can query which struct field
is being accessed.

Usage:
    ./tools/struct_db.py build [paths...] [--db struct_db.sqlite] [-v]
    ./tools/struct_db.py lookup <class> <offset> [--db struct_db.sqlite]
    ./tools/struct_db.py info <class> [--db struct_db.sqlite]
    ./tools/struct_db.py list [--pattern PAT] [--db struct_db.sqlite]
    ./tools/struct_db.py import-ghidra [--db struct_db.sqlite] [--dry-run]
"""

import argparse
import fnmatch
import re
import sqlite3
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple


@dataclass
class Member:
    """A class/struct member with offset annotation."""
    name: str
    type_str: str
    offset: int
    line_number: int
    raw_line: str = ""  # Original line for array detection


@dataclass
class ClassInfo:
    """Parsed class/struct information."""
    name: str
    file_path: str
    parents: List[str] = field(default_factory=list)
    is_virtual_inheritance: Dict[str, bool] = field(default_factory=dict)
    members: List[Member] = field(default_factory=list)
    is_struct: bool = False


# Regex patterns
# Class/struct declaration with optional inheritance
CLASS_DECL_RE = re.compile(
    r'^\s*(?:class|struct)\s+(\w+)(?:\s*:\s*(.+?))?\s*\{',
    re.MULTILINE
)

# Member with offset annotation
# Handles: ObjPtr<T>, std::vector<T>, Type*, Type&, arrays, etc.
# Pattern captures: (type) (*|&)? (name) [array]? ; // 0xHEX
MEMBER_RE = re.compile(
    r'^\s*([^;]+?)\s*([*&]?)\s*(\w+)(?:\s*\[[^\]]*\])?\s*;\s*//\s*0x([0-9a-fA-F]+)',
    re.MULTILINE
)

# Parse inheritance list like "public Foo, virtual public Bar, private Baz"
INHERIT_RE = re.compile(
    r'(virtual\s+)?(public|private|protected)\s+([\w:]+)'
)


class StructDB:
    """SQLite-backed struct offset database."""

    def __init__(self, db_path: str = "struct_db.sqlite"):
        self.db_path = db_path
        self.conn: Optional[sqlite3.Connection] = None

    def connect(self):
        """Connect to the database."""
        self.conn = sqlite3.connect(self.db_path)
        self.conn.execute("PRAGMA journal_mode = WAL")
        self.conn.row_factory = sqlite3.Row

    def close(self):
        """Close the database connection."""
        if self.conn:
            self.conn.close()
            self.conn = None

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()

    def create_schema(self):
        """Create database schema."""
        cursor = self.conn.cursor()
        cursor.executescript("""
            CREATE TABLE IF NOT EXISTS classes (
                id INTEGER PRIMARY KEY,
                name TEXT NOT NULL,
                file_path TEXT,
                is_struct INTEGER DEFAULT 0,
                UNIQUE(name, file_path)
            );

            CREATE TABLE IF NOT EXISTS inheritance (
                child_id INTEGER REFERENCES classes(id),
                parent_name TEXT NOT NULL,
                is_virtual INTEGER DEFAULT 0,
                order_idx INTEGER
            );

            CREATE TABLE IF NOT EXISTS members (
                class_id INTEGER REFERENCES classes(id),
                name TEXT NOT NULL,
                type_str TEXT,
                offset INTEGER NOT NULL
            );

            CREATE INDEX IF NOT EXISTS idx_members_class_offset
            ON members(class_id, offset);

            CREATE INDEX IF NOT EXISTS idx_classes_name
            ON classes(name);

            CREATE TABLE IF NOT EXISTS layout_issues (
                id INTEGER PRIMARY KEY,
                class_id INTEGER REFERENCES classes(id),
                member_name TEXT NOT NULL,
                issue_type TEXT NOT NULL,
                expected_size INTEGER,
                actual_gap INTEGER,
                details TEXT,
                UNIQUE(class_id, member_name, issue_type)
            );

            CREATE INDEX IF NOT EXISTS idx_layout_issues_class
            ON layout_issues(class_id);
        """)
        self.conn.commit()

    def clear(self):
        """Clear all data from the database."""
        cursor = self.conn.cursor()
        cursor.executescript("""
            DELETE FROM members;
            DELETE FROM inheritance;
            DELETE FROM classes;
        """)
        self.conn.commit()

    def insert_class(self, info: ClassInfo) -> int:
        """Insert a class and return its ID."""
        cursor = self.conn.cursor()

        # Insert or replace the class
        cursor.execute("""
            INSERT OR REPLACE INTO classes (name, file_path, is_struct)
            VALUES (?, ?, ?)
        """, (info.name, info.file_path, 1 if info.is_struct else 0))

        class_id = cursor.lastrowid

        # Delete existing inheritance and members for this class
        cursor.execute("DELETE FROM inheritance WHERE child_id = ?", (class_id,))
        cursor.execute("DELETE FROM members WHERE class_id = ?", (class_id,))

        # Insert inheritance
        for idx, parent in enumerate(info.parents):
            is_virtual = info.is_virtual_inheritance.get(parent, False)
            cursor.execute("""
                INSERT INTO inheritance (child_id, parent_name, is_virtual, order_idx)
                VALUES (?, ?, ?, ?)
            """, (class_id, parent, 1 if is_virtual else 0, idx))

        # Insert members
        for member in info.members:
            cursor.execute("""
                INSERT INTO members (class_id, name, type_str, offset)
                VALUES (?, ?, ?, ?)
            """, (class_id, member.name, member.type_str, member.offset))

        return class_id

    def get_class_id(self, class_name: str) -> Optional[int]:
        """Get class ID by name."""
        cursor = self.conn.cursor()
        cursor.execute(
            "SELECT id FROM classes WHERE name = ?",
            (class_name,)
        )
        row = cursor.fetchone()
        return row['id'] if row else None

    def get_class_info(self, class_name: str) -> Optional[Dict]:
        """Get full class info including parents and members."""
        cursor = self.conn.cursor()

        cursor.execute(
            "SELECT * FROM classes WHERE name = ?",
            (class_name,)
        )
        class_row = cursor.fetchone()
        if not class_row:
            return None

        class_id = class_row['id']

        # Get parents
        cursor.execute("""
            SELECT parent_name, is_virtual FROM inheritance
            WHERE child_id = ? ORDER BY order_idx
        """, (class_id,))
        parents = [(row['parent_name'], bool(row['is_virtual']))
                   for row in cursor.fetchall()]

        # Get members
        cursor.execute("""
            SELECT name, type_str, offset FROM members
            WHERE class_id = ? ORDER BY offset
        """, (class_id,))
        members = [dict(row) for row in cursor.fetchall()]

        return {
            'name': class_row['name'],
            'file_path': class_row['file_path'],
            'is_struct': bool(class_row['is_struct']),
            'parents': parents,
            'members': members
        }

    def resolve_inheritance_chain(self, class_name: str) -> List[str]:
        """Get ordered list of all parent classes (depth-first)."""
        cursor = self.conn.cursor()
        visited = set()
        chain = []

        def visit(name: str):
            if name in visited:
                return
            visited.add(name)

            # Get class ID
            cursor.execute("SELECT id FROM classes WHERE name = ?", (name,))
            row = cursor.fetchone()
            if not row:
                return

            class_id = row['id']

            # Get parents in order
            cursor.execute("""
                SELECT parent_name FROM inheritance
                WHERE child_id = ? ORDER BY order_idx
            """, (class_id,))

            for parent_row in cursor.fetchall():
                parent_name = parent_row['parent_name']
                visit(parent_name)
                if parent_name not in chain:
                    chain.append(parent_name)

        visit(class_name)
        return chain

    def lookup(self, class_name: str, offset: int) -> Optional[Tuple[str, str, str]]:
        """
        Look up field at offset, checking inheritance chain.
        Returns (class_name, member_name, type_str) or None.
        """
        cursor = self.conn.cursor()

        # Classes to check: the class itself plus all parents
        classes_to_check = [class_name] + self.resolve_inheritance_chain(class_name)

        for check_class in classes_to_check:
            cursor.execute("""
                SELECT c.name, m.name, m.type_str
                FROM members m
                JOIN classes c ON m.class_id = c.id
                WHERE c.name = ? AND m.offset = ?
            """, (check_class, offset))

            row = cursor.fetchone()
            if row:
                return (row[0], row[1], row[2])

        return None

    def list_classes(self, pattern: Optional[str] = None) -> List[Dict]:
        """List all classes, optionally filtered by pattern."""
        cursor = self.conn.cursor()
        cursor.execute("SELECT name, file_path, is_struct FROM classes ORDER BY name")

        results = []
        for row in cursor.fetchall():
            name = row['name']
            if pattern and not fnmatch.fnmatch(name, pattern):
                continue
            results.append({
                'name': name,
                'file_path': row['file_path'],
                'is_struct': bool(row['is_struct'])
            })

        return results

    def build_from_paths(self, paths: List[Path], verbose: bool = False):
        """Parse headers from paths and build database."""
        self.create_schema()
        self.clear()

        # Collect all header files
        header_files = []
        for path in paths:
            if path.is_file():
                header_files.append(path)
            elif path.is_dir():
                header_files.extend(path.rglob("*.h"))

        total_classes = 0
        total_members = 0

        for header_path in header_files:
            classes = parse_header(header_path)
            for cls in classes:
                self.insert_class(cls)
                total_classes += 1
                total_members += len(cls.members)
                if verbose:
                    print(f"  {cls.name}: {len(cls.members)} members")

        self.conn.commit()
        return total_classes, total_members


# Known type sizes (ILP32 / Xbox 360)
TYPE_SIZES = {
    'bool': 1, 'char': 1, 'unsigned char': 1, 'signed char': 1,
    'u8': 1, 's8': 1,
    'short': 2, 'unsigned short': 2, 'u16': 2, 's16': 2,
    'int': 4, 'unsigned int': 4, 'float': 4,
    'u32': 4, 's32': 4, 'long': 4, 'unsigned long': 4,
    'long long': 8, 'unsigned long long': 8, 'double': 8,
    'u64': 8, 's64': 8,
    'Symbol': 4, 'DataNode': 16,
    'Vector2': 8, 'Vector3': 12, 'PaddedJointPos': 16, 'Vector4': 16,
    'Hmx::Color': 16, 'Color': 16,
    'Hmx::Quat': 16, 'Quat': 16,
    'XMVECTOR': 16, 'XMMATRIX': 64,
    'Transform': 64, 'Plane': 16,
    'String': 8, 'FilePath': 8, 'DateTime': 8,
    'Sphere': 16, 'Box': 24,
}

# Template type sizes (the template itself, not the parameter)
TEMPLATE_SIZES = {
    'ObjPtr': 0x14, 'ObjOwnerPtr': 0x14,
    'ObjPtrList': 0xC, 'ObjList': 0xC,
    'ObjDirPtr': 0x10, 'ObjVector': 0xC,
}

# Known array size constants
ARRAY_CONSTANTS = {
    'kNumJoints': 20, 'kNumBones': 19, 'kNumCoordSys': 8,
    'kMaxNumErrorNodes': 33, 'kMaxNumNormBones': 3,
    'kNumHam1Nodes': 16,
}

ARRAY_RE = re.compile(r'\[(\w+)\]')


def guess_type_size(type_str: str) -> Optional[int]:
    """Guess the size of a C++ type on ILP32."""
    t = type_str.strip()
    # Remove mutable/const/volatile qualifiers
    for qual in ('mutable ', 'const ', 'volatile '):
        t = t.replace(qual, '')
    t = t.strip()

    if t in TYPE_SIZES:
        return TYPE_SIZES[t]
    if t.endswith('*') or t.endswith('* const'):
        return 4
    # Template types
    for tmpl, size in TEMPLATE_SIZES.items():
        if t.startswith(tmpl + '<') or t.startswith(tmpl + ' <'):
            return size
    if 'std::vector' in t or 'vector<' in t:
        return 0xC
    if 'std::list' in t or 'list<' in t:
        return 0xC
    return None


def guess_array_count(raw_line: str) -> Optional[int]:
    """Extract array count from a member declaration line."""
    m = ARRAY_RE.search(raw_line)
    if not m:
        return None
    token = m.group(1)
    if token.isdigit():
        return int(token)
    return ARRAY_CONSTANTS.get(token)


def parse_inheritance(inherit_str: str) -> Tuple[List[str], Dict[str, bool]]:
    """Parse inheritance string into parent list and virtual flags."""
    parents = []
    is_virtual = {}

    for match in INHERIT_RE.finditer(inherit_str):
        virtual = match.group(1) is not None
        parent_name = match.group(3)
        # Strip namespace prefix if present (e.g., Hmx::Object -> Object)
        # Keep full name for now since we might want it
        parents.append(parent_name)
        is_virtual[parent_name] = virtual

    return parents, is_virtual


def parse_header(path: Path) -> List[ClassInfo]:
    """Parse a header file and extract class/struct information."""
    try:
        content = path.read_text(encoding='utf-8', errors='ignore')
    except Exception:
        return []

    classes = []
    lines = content.split('\n')

    # Track nested class context
    class_stack = []
    current_class: Optional[ClassInfo] = None
    brace_depth = 0
    class_start_depth = 0

    i = 0
    while i < len(lines):
        line = lines[i]

        # Check for class/struct declaration
        # We need to be careful about forward declarations
        stripped = line.strip()

        class_match = re.match(
            r'^(class|struct)\s+(\w+)(?:\s*:\s*(.+?))?\s*\{',
            stripped
        )

        if not class_match:
            # Check if declaration spans multiple lines
            if re.match(r'^(class|struct)\s+(\w+)\s*:', stripped):
                # Inheritance on next line
                combined = stripped
                j = i + 1
                while j < len(lines) and '{' not in combined:
                    combined += ' ' + lines[j].strip()
                    j += 1
                class_match = re.match(
                    r'^(class|struct)\s+(\w+)\s*:\s*(.+?)\s*\{',
                    combined
                )

        if class_match:
            keyword = class_match.group(1)
            class_name = class_match.group(2)
            inherit_str = class_match.group(3) or ""

            # Handle nested classes
            if current_class:
                class_stack.append((current_class, class_start_depth))
                class_name = f"{current_class.name}::{class_name}"

            parents, is_virtual_dict = parse_inheritance(inherit_str)

            current_class = ClassInfo(
                name=class_name,
                file_path=str(path),
                parents=parents,
                is_virtual_inheritance=is_virtual_dict,
                is_struct=(keyword == 'struct')
            )
            class_start_depth = brace_depth

        # Track brace depth
        brace_depth += line.count('{') - line.count('}')

        # Check if we've exited the current class
        if current_class and brace_depth <= class_start_depth:
            classes.append(current_class)
            if class_stack:
                current_class, class_start_depth = class_stack.pop()
            else:
                current_class = None

        # Check for member with offset annotation
        if current_class:
            member_match = re.match(
                r'\s*([^;]+?)\s*([*&]?)\s*(\w+)(?:\s*\[[^\]]*\])?\s*;\s*//\s*0x([0-9a-fA-F]+)',
                line
            )
            if member_match:
                type_str = member_match.group(1).strip()
                ptr_ref = member_match.group(2)
                if ptr_ref:
                    type_str += ' ' + ptr_ref
                name = member_match.group(3)
                offset = int(member_match.group(4), 16)

                current_class.members.append(Member(
                    name=name,
                    type_str=type_str,
                    offset=offset,
                    line_number=i + 1,
                    raw_line=line.strip()
                ))

        i += 1

    # Don't forget the last class if file doesn't end cleanly
    if current_class:
        classes.append(current_class)

    return classes


def validate_classes(header_paths: List[Path], rb2_dump_path: Optional[Path] = None) -> List[dict]:
    """Validate struct layouts by checking offset gaps against expected type sizes.

    Returns a list of issue dicts with keys:
        class_name, file_path, member_name, type_str, line,
        offset, next_offset, next_member, expected_size, actual_gap,
        issue_type, details
    """
    issues = []
    rb2_parser = None

    if rb2_dump_path and rb2_dump_path.exists():
        import sys
        project_root = str(Path(__file__).resolve().parent.parent)
        if project_root not in sys.path:
            sys.path.insert(0, project_root)
        try:
            from scripts.orchestrator.rb2_dwarf import RB2DwarfParser
            rb2_parser = RB2DwarfParser(rb2_dump_path)
        except Exception:
            pass

    for path in header_paths:
        classes = parse_header(path)

        # Also do a raw scan to find unannotated member lines between annotated ones
        try:
            raw_lines = path.read_text(encoding='utf-8', errors='ignore').splitlines()
        except Exception:
            raw_lines = []

        # Regex for any member declaration (with or without offset comment)
        any_member_re = re.compile(
            r'^\s+(?:mutable\s+)?(?!virtual\b|static\b|friend\b|typedef\b|enum\b|struct\b|class\b|union\b|using\b|public|private|protected|//|/\*|#)'
            r'[\w:<>, *&]+\s+\w+\s*(?:\[[^\]]*\])?\s*;'
        )

        for cls in classes:
            members = cls.members
            for i in range(len(members) - 1):
                m1 = members[i]
                m2 = members[i + 1]

                if m1.offset is None or m2.offset is None:
                    continue

                actual_gap = m2.offset - m1.offset
                if actual_gap <= 0:
                    continue

                # Check for unannotated members between m1 and m2
                has_unannotated = False
                if raw_lines and m1.line_number < m2.line_number:
                    for ln in range(m1.line_number, m2.line_number - 1):
                        if ln < len(raw_lines):
                            line = raw_lines[ln]
                            # Skip if it has an offset comment (it's m1 or m2)
                            if re.search(r'//\s*0x[0-9a-fA-F]+', line):
                                continue
                            if any_member_re.match(line):
                                has_unannotated = True
                                break

                elem_size = guess_type_size(m1.type_str)
                if elem_size is None:
                    continue

                arr_count = guess_array_count(m1.raw_line)
                expected = elem_size * arr_count if arr_count else elem_size

                # Allow alignment padding (round up to 4)
                expected_aligned = (expected + 3) & ~3

                if actual_gap == expected or actual_gap == expected_aligned:
                    continue

                # If there are unannotated members between, gap is expected to be
                # larger — only flag if gap is SMALLER than expected (real bug) or
                # if it's a clear stride issue
                if has_unannotated:
                    # Still flag stride_16 (likely real even with hidden members)
                    if not (arr_count and elem_size == 12
                            and actual_gap == arr_count * 16):
                        continue

                # Classify the issue
                if arr_count and elem_size == 12 and actual_gap == arr_count * 16:
                    issue_type = 'stride_16'
                    details = (f"Vector3[{arr_count}] uses 16-byte stride "
                              f"(gap=0x{actual_gap:x}, need PaddedJointPos)")
                elif arr_count:
                    actual_stride = actual_gap / arr_count
                    issue_type = 'stride_mismatch'
                    details = (f"stride={actual_stride:.0f}, expected={elem_size} "
                              f"(gap=0x{actual_gap:x})")
                else:
                    issue_type = 'gap_mismatch'
                    details = (f"gap=0x{actual_gap:x} ({actual_gap}), "
                              f"expected=0x{expected:x} ({expected})")

                issue = {
                    'class_name': cls.name,
                    'file_path': str(cls.file_path),
                    'member_name': m1.name,
                    'type_str': m1.type_str,
                    'line': m1.line_number,
                    'offset': m1.offset,
                    'next_offset': m2.offset,
                    'next_member': m2.name,
                    'expected_size': expected,
                    'actual_gap': actual_gap,
                    'issue_type': issue_type,
                    'details': details,
                }

                # Cross-validate with RB2 if available
                if rb2_parser:
                    rb2_info = rb2_parser.get_class(cls.name)
                    if rb2_info and 'members' in rb2_info:
                        for rb2_m in rb2_info['members']:
                            if rb2_m.get('offset') == m1.offset:
                                rb2_size = rb2_m.get('size')
                                if rb2_size and rb2_size != elem_size:
                                    issue['rb2_size'] = rb2_size
                                    issue['details'] += (
                                        f" [RB2 DWARF: size={rb2_size} at same offset]"
                                    )
                                break

                issues.append(issue)

    return issues


def cmd_validate(args):
    """Validate struct layouts and optionally store results in DB."""
    paths = [Path(p) for p in args.paths] if args.paths else [Path("src/")]

    header_files = []
    for p in paths:
        if p.is_file():
            header_files.append(p)
        elif p.is_dir():
            header_files.extend(p.rglob("*.h"))

    rb2_path = None
    default_rb2 = Path.home() / "code/milohax/rb3/doc/rb2_dump.cpp"
    if default_rb2.exists():
        rb2_path = default_rb2

    issues = validate_classes(header_files, rb2_path)

    # Filter by type if requested
    if args.type:
        issues = [i for i in issues if i['issue_type'] == args.type]

    # Filter to only stride issues (most actionable)
    if args.stride_only:
        issues = [i for i in issues if 'stride' in i['issue_type']]

    if not issues:
        print("No layout issues found!")
        return

    print(f"Found {len(issues)} layout issues:\n")

    # Group by issue type
    by_type = {}
    for iss in issues:
        by_type.setdefault(iss['issue_type'], []).append(iss)

    for issue_type, type_issues in sorted(by_type.items()):
        print(f"=== {issue_type} ({len(type_issues)}) ===\n")
        for iss in type_issues:
            relpath = Path(iss['file_path']).name
            print(f"  {relpath}:{iss['line']}  {iss['class_name']}::{iss['member_name']}")
            print(f"    {iss['type_str']} @ 0x{iss['offset']:x} -> "
                  f"{iss['next_member']} @ 0x{iss['next_offset']:x}")
            print(f"    {iss['details']}")
            print()

    # Store in DB if requested
    if args.store:
        with StructDB(args.db) as db:
            db.create_schema()
            cursor = db.conn.cursor()
            stored = 0
            for iss in issues:
                # Find class_id
                row = cursor.execute(
                    "SELECT id FROM classes WHERE name = ?",
                    (iss['class_name'],)
                ).fetchone()
                if not row:
                    continue
                class_id = row[0]
                try:
                    cursor.execute("""
                        INSERT OR REPLACE INTO layout_issues
                        (class_id, member_name, issue_type, expected_size,
                         actual_gap, details)
                        VALUES (?, ?, ?, ?, ?, ?)
                    """, (class_id, iss['member_name'], iss['issue_type'],
                          iss['expected_size'], iss['actual_gap'], iss['details']))
                    stored += 1
                except Exception:
                    pass
            db.conn.commit()
            print(f"\nStored {stored} issues in {args.db}")

    # Summary
    print(f"\n--- Summary ---")
    for t, items in sorted(by_type.items()):
        print(f"  {t}: {len(items)}")


def cmd_build(args):
    """Build database from headers."""
    paths = [Path(p) for p in args.paths] if args.paths else [Path("src/")]

    with StructDB(args.db) as db:
        total_classes, total_members = db.build_from_paths(paths, args.verbose)

    print(f"Built database: {total_classes} classes, {total_members} members")
    print(f"Saved to: {args.db}")


def cmd_lookup(args):
    """Look up field at offset."""
    # Parse offset (hex or decimal)
    if args.offset.startswith('0x'):
        offset = int(args.offset, 16)
    else:
        offset = int(args.offset)

    with StructDB(args.db) as db:
        result = db.lookup(args.class_name, offset)

    if result:
        cls_name, member_name, type_str = result
        print(f"{cls_name}::{member_name} ({type_str})")
    else:
        print(f"No field found at offset 0x{offset:x} in {args.class_name}")


def cmd_info(args):
    """Show class info."""
    with StructDB(args.db) as db:
        info = db.get_class_info(args.class_name)

    if not info:
        print(f"Class not found: {args.class_name}")
        return

    keyword = "struct" if info['is_struct'] else "class"
    print(f"{keyword} {info['name']}")
    print(f"  File: {info['file_path']}")

    if info['parents']:
        print("  Parents:")
        for parent, is_virtual in info['parents']:
            v = " (virtual)" if is_virtual else ""
            print(f"    - {parent}{v}")

    # Get full inheritance chain
    with StructDB(args.db) as db:
        chain = db.resolve_inheritance_chain(args.class_name)
    if chain:
        print(f"  Full inheritance chain: {' -> '.join(chain)}")

    if info['members']:
        print("  Members:")
        for m in info['members']:
            print(f"    0x{m['offset']:02x}: {m['type_str']} {m['name']}")


def cmd_list(args):
    """List classes."""
    with StructDB(args.db) as db:
        classes = db.list_classes(args.pattern)

    if not classes:
        print("No classes found")
        return

    for cls in classes:
        keyword = "struct" if cls['is_struct'] else "class"
        print(f"{keyword} {cls['name']}")


def cmd_import_ghidra(args):
    """Import structure types from Ghidra via pyghidra-mcp."""
    import sys
    # Ensure project root is in path for tools.ghidra imports
    project_root = str(Path(__file__).resolve().parent.parent)
    if project_root not in sys.path:
        sys.path.insert(0, project_root)

    from tools.ghidra.batch_export_types import (
        show_stats, seed_ghidra_dtm, extract_ghidra_structures,
        import_structures_to_db,
    )
    from tools.ghidra.mcp_client import MCPClient, MCPError

    if args.stats:
        show_stats(Path(args.db))
        return

    # Connect to pyghidra-mcp service
    client = MCPClient(url=args.mcp_url)
    try:
        client.initialize()
    except MCPError as e:
        print(f"ERROR: Could not connect to pyghidra-mcp: {e}")
        print("Start the service with: ./tools/ghidra/pyghidra-service.sh start")
        return

    db_path = Path(args.db)
    map_file = Path(project_root) / "orig" / "45410914" / "ham_xbox_r.map"

    seed_ghidra_dtm(client=client, db_path=db_path, map_file=map_file, dry_run=args.dry_run)

    structures, stats = extract_ghidra_structures(
        client=client,
        max_functions=args.max_functions,
        timeout_per_func=args.timeout_per_func,
    )

    import_structures_to_db(db_path=db_path, structures=structures, dry_run=args.dry_run)


def main():
    parser = argparse.ArgumentParser(
        description="Struct offset database for DC3 decomp"
    )
    parser.add_argument(
        '--db', default='struct_db.sqlite',
        help='Database file path (default: struct_db.sqlite)'
    )

    subparsers = parser.add_subparsers(dest='command', required=True)

    # build command
    build_parser = subparsers.add_parser('build', help='Build database from headers')
    build_parser.add_argument(
        'paths', nargs='*',
        help='Paths to scan (default: src/)'
    )
    build_parser.add_argument(
        '-v', '--verbose', action='store_true',
        help='Verbose output'
    )

    # lookup command
    lookup_parser = subparsers.add_parser('lookup', help='Look up field at offset')
    lookup_parser.add_argument('class_name', help='Class name')
    lookup_parser.add_argument('offset', help='Offset (hex with 0x prefix or decimal)')

    # info command
    info_parser = subparsers.add_parser('info', help='Show class info')
    info_parser.add_argument('class_name', help='Class name')

    # list command
    list_parser = subparsers.add_parser('list', help='List classes')
    list_parser.add_argument(
        '--pattern', '-p',
        help='Filter by glob pattern (e.g., Rnd*)'
    )

    # import-ghidra command
    ghidra_parser = subparsers.add_parser(
        'import-ghidra',
        help='Import structure types by batch-decompiling in Ghidra'
    )
    ghidra_parser.add_argument(
        '--mcp-url',
        default='http://127.0.0.1:8000/mcp',
        help='pyghidra-mcp service URL'
    )
    ghidra_parser.add_argument(
        '--max-functions',
        type=int, default=0,
        help='Max functions to decompile (0 = all)'
    )
    ghidra_parser.add_argument(
        '--timeout-per-func',
        type=int, default=30,
        help='Decompiler timeout per function in seconds'
    )
    ghidra_parser.add_argument(
        '--dry-run',
        action='store_true',
        help='Preview what would be imported without writing'
    )
    ghidra_parser.add_argument(
        '--stats',
        action='store_true',
        help='Show struct_db statistics and exit'
    )

    # validate command
    validate_parser = subparsers.add_parser(
        'validate',
        help='Validate struct layouts (detect stride/gap mismatches)'
    )
    validate_parser.add_argument(
        'paths', nargs='*',
        help='Paths to scan (default: src/)'
    )
    validate_parser.add_argument(
        '--type', '-t',
        help='Filter by issue type (stride_16, stride_mismatch, gap_mismatch)'
    )
    validate_parser.add_argument(
        '--stride-only', '-s', action='store_true',
        help='Only show stride mismatches (most actionable)'
    )
    validate_parser.add_argument(
        '--store', action='store_true',
        help='Store results in layout_issues table'
    )

    args = parser.parse_args()

    if args.command == 'build':
        cmd_build(args)
    elif args.command == 'lookup':
        cmd_lookup(args)
    elif args.command == 'info':
        cmd_info(args)
    elif args.command == 'list':
        cmd_list(args)
    elif args.command == 'import-ghidra':
        cmd_import_ghidra(args)
    elif args.command == 'validate':
        cmd_validate(args)


if __name__ == '__main__':
    main()
