#!/usr/bin/env python3
"""
Export Ghidra-discovered types as C headers for m2c context.

Since the Ghidra MCP doesn't expose direct type manager APIs, this script
extracts type information from:
1. Decompiled function code (struct/class definitions, typedefs)
2. Symbol searches (globals, function prototypes)
3. Function signatures from decompilation results

Usage:
    export_types.py --function "Game::Poll"     # Types relevant to a function
    export_types.py --all                        # Export all discoverable types
    export_types.py --function "Foo::Bar" -o types.h  # Output to file

The output is compatible with m2c --context format.
"""

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from mcp_client import MCPClient, MCPError, DEFAULT_BINARY


# =============================================================================
# Data Structures
# =============================================================================

@dataclass
class TypeDef:
    """A typedef declaration."""
    name: str
    base_type: str

    def to_c(self) -> str:
        return f"typedef {self.base_type} {self.name};"


@dataclass
class StructField:
    """A struct/class field."""
    name: str
    type_name: str
    offset: Optional[int] = None
    comment: Optional[str] = None

    def to_c(self) -> str:
        offset_comment = f" /* 0x{self.offset:x} */" if self.offset is not None else ""
        comment = f" // {self.comment}" if self.comment else ""
        return f"    {self.type_name} {self.name};{offset_comment}{comment}"


@dataclass
class StructDef:
    """A struct or class definition."""
    name: str
    fields: list[StructField] = field(default_factory=list)
    is_class: bool = False
    base_class: Optional[str] = None
    size: Optional[int] = None

    def to_c(self) -> str:
        lines = []
        keyword = "class" if self.is_class else "struct"
        inheritance = f" : public {self.base_class}" if self.base_class else ""
        size_comment = f" /* size: 0x{self.size:x} */" if self.size is not None else ""

        lines.append(f"{keyword} {self.name}{inheritance} {{{size_comment}")
        if self.is_class:
            lines.append("public:")
        for fld in self.fields:
            lines.append(fld.to_c())
        lines.append("};")
        return "\n".join(lines)


@dataclass
class EnumValue:
    """An enum constant."""
    name: str
    value: int

    def to_c(self) -> str:
        return f"    {self.name} = {self.value},"


@dataclass
class EnumDef:
    """An enum definition."""
    name: str
    values: list[EnumValue] = field(default_factory=list)

    def to_c(self) -> str:
        lines = [f"enum {self.name} {{"]
        for val in self.values:
            lines.append(val.to_c())
        lines.append("};")
        return "\n".join(lines)


@dataclass
class FunctionProto:
    """A function prototype."""
    name: str
    return_type: str
    params: list[tuple[str, str]]  # [(type, name), ...]
    is_method: bool = False
    class_name: Optional[str] = None

    def to_c(self) -> str:
        param_str = ", ".join(
            f"{ptype} {pname}" if pname else ptype
            for ptype, pname in self.params
        )
        if not param_str:
            param_str = "void"
        return f"{self.return_type} {self.name}({param_str});"


@dataclass
class GlobalVar:
    """A global variable declaration."""
    name: str
    type_name: str

    def to_c(self) -> str:
        return f"extern {self.type_name} {self.name};"


@dataclass
class TypeContext:
    """Collection of all extracted types."""
    typedefs: list[TypeDef] = field(default_factory=list)
    structs: list[StructDef] = field(default_factory=list)
    enums: list[EnumDef] = field(default_factory=list)
    functions: list[FunctionProto] = field(default_factory=list)
    globals: list[GlobalVar] = field(default_factory=list)
    forward_decls: set[str] = field(default_factory=set)

    def merge(self, other: "TypeContext") -> None:
        """Merge another context into this one."""
        seen_typedefs = {t.name for t in self.typedefs}
        seen_structs = {s.name for s in self.structs}
        seen_enums = {e.name for e in self.enums}
        seen_funcs = {f.name for f in self.functions}
        seen_globals = {g.name for g in self.globals}

        for t in other.typedefs:
            if t.name not in seen_typedefs:
                self.typedefs.append(t)
                seen_typedefs.add(t.name)

        for s in other.structs:
            if s.name not in seen_structs:
                self.structs.append(s)
                seen_structs.add(s.name)

        for e in other.enums:
            if e.name not in seen_enums:
                self.enums.append(e)
                seen_enums.add(e.name)

        for f in other.functions:
            if f.name not in seen_funcs:
                self.functions.append(f)
                seen_funcs.add(f.name)

        for g in other.globals:
            if g.name not in seen_globals:
                self.globals.append(g)
                seen_globals.add(g.name)

        self.forward_decls.update(other.forward_decls)

    def to_c(self) -> str:
        """Generate C header content."""
        lines = []
        lines.append("/* Auto-generated type context from Ghidra */")
        lines.append("/* For use with m2c --context */")
        lines.append("")

        # Forward declarations
        if self.forward_decls:
            lines.append("/* Forward declarations */")
            for name in sorted(self.forward_decls):
                lines.append(f"struct {name};")
            lines.append("")

        # Typedefs
        if self.typedefs:
            lines.append("/* Typedefs */")
            for t in self.typedefs:
                lines.append(t.to_c())
            lines.append("")

        # Enums
        if self.enums:
            lines.append("/* Enums */")
            for e in self.enums:
                lines.append(e.to_c())
                lines.append("")

        # Structs and classes
        if self.structs:
            lines.append("/* Structs and classes */")
            for s in self.structs:
                lines.append(s.to_c())
                lines.append("")

        # Globals
        if self.globals:
            lines.append("/* Global variables */")
            for g in self.globals:
                lines.append(g.to_c())
            lines.append("")

        # Function prototypes
        if self.functions:
            lines.append("/* Function prototypes */")
            for f in self.functions:
                lines.append(f.to_c())
            lines.append("")

        return "\n".join(lines)


# =============================================================================
# Standard Type Definitions
# =============================================================================

def get_standard_typedefs() -> list[TypeDef]:
    """Return standard Xbox 360/PowerPC typedefs."""
    return [
        TypeDef("u8", "unsigned char"),
        TypeDef("s8", "signed char"),
        TypeDef("u16", "unsigned short"),
        TypeDef("s16", "signed short"),
        TypeDef("u32", "unsigned int"),
        TypeDef("s32", "signed int"),
        TypeDef("u64", "unsigned long long"),
        TypeDef("s64", "signed long long"),
        TypeDef("f32", "float"),
        TypeDef("f64", "double"),
        TypeDef("uint", "unsigned int"),
        TypeDef("uchar", "unsigned char"),
        TypeDef("ushort", "unsigned short"),
        TypeDef("ulong", "unsigned long"),
        TypeDef("bool", "unsigned char"),  # PowerPC bool is often 1 byte
    ]


# =============================================================================
# Code Parsing Functions
# =============================================================================

def parse_function_signature(signature: str) -> Optional[FunctionProto]:
    """
    Parse a Ghidra function signature into a FunctionProto.

    Examples:
        "void Game::Poll(void)"
        "int CharMirror::Load(BinStream *)"
        "undefined4 FUN_82001234(undefined4 param_1)"
    """
    # Clean up the signature
    signature = signature.strip()
    if not signature:
        return None

    # Try to match: return_type name(params)
    # Handle potential namespace/class qualifiers
    match = re.match(
        r'^(\w+(?:\s*\*)?)\s+'  # return type (with optional pointer)
        r'([\w:]+)'             # function name (with optional ::)
        r'\s*\((.*)\)\s*$',     # parameters
        signature
    )
    if not match:
        return None

    return_type = match.group(1).strip()
    func_name = match.group(2).strip()
    param_str = match.group(3).strip()

    # Parse parameters
    params = []
    if param_str and param_str.lower() != "void":
        # Simple parameter parsing - split by comma
        for param in param_str.split(","):
            param = param.strip()
            if not param:
                continue
            # Try to split type and name
            # Handle: "int x", "int *x", "SomeType * name"
            parts = param.rsplit(None, 1)
            if len(parts) == 2:
                ptype, pname = parts
                # Handle pointer attached to name
                if pname.startswith("*"):
                    ptype += " *"
                    pname = pname[1:]
                params.append((ptype.strip(), pname.strip()))
            else:
                # Just a type, no name
                params.append((param, ""))

    # Check if it's a method
    is_method = "::" in func_name
    class_name = None
    if is_method:
        parts = func_name.rsplit("::", 1)
        class_name = parts[0]

    return FunctionProto(
        name=func_name,
        return_type=return_type,
        params=params,
        is_method=is_method,
        class_name=class_name
    )


def extract_types_from_code(code: str) -> TypeContext:
    """
    Extract type information from decompiled code.

    This parses Ghidra's decompiled output to find:
    - Struct/class declarations
    - Typedef usage patterns
    - Global variable references
    - Function calls (for prototypes)
    """
    ctx = TypeContext()

    # Pattern for Ghidra's struct field comments: /* 0xoffset */
    field_pattern = re.compile(
        r'^\s*(\w+(?:\s*\*)?)\s+(\w+);\s*/\*\s*0x([0-9a-fA-F]+)\s*\*/',
        re.MULTILINE
    )

    # Extract type references from variable declarations
    # Pattern: Type * varname or Type varname
    var_pattern = re.compile(r'\b(\w+)\s*\*?\s*\b(\w+)\s*[=;]')

    # Find global variable patterns (typically uppercase or prefixed with The/g)
    global_pattern = re.compile(r'\b(The\w+|g\w+|DAT_\w+)\b')

    # Find function call patterns
    call_pattern = re.compile(r'\b(\w+::\w+)\s*\(')

    # Track seen types for forward declarations
    type_refs = set()

    for match in var_pattern.finditer(code):
        type_name = match.group(1)
        if type_name[0].isupper() and type_name not in (
            "TRUE", "FALSE", "NULL", "BOOL"
        ):
            type_refs.add(type_name)

    for match in global_pattern.finditer(code):
        global_name = match.group(1)
        # Try to infer type from naming convention
        if global_name.startswith("The"):
            type_name = global_name[3:]  # TheDebug -> Debug
            ctx.globals.append(GlobalVar(global_name, type_name))
            type_refs.add(type_name)

    # Add forward declarations for referenced types
    ctx.forward_decls = type_refs

    return ctx


def extract_class_info_from_decompilation(result: dict) -> TypeContext:
    """
    Extract class/struct information from a decompilation result.

    Ghidra often includes struct definitions in decompiled code or
    shows them in the signature.
    """
    ctx = TypeContext()

    # Get function signature
    signature = result.get("signature", "")
    if signature:
        proto = parse_function_signature(signature)
        if proto:
            ctx.functions.append(proto)

            # If it's a method, create a forward declaration for the class
            if proto.class_name:
                ctx.forward_decls.add(proto.class_name)

            # Add forward declarations for parameter types
            for ptype, _ in proto.params:
                # Extract base type (remove pointers, const, etc.)
                base = ptype.replace("*", "").replace("const", "").strip()
                if base and base[0].isupper() and base not in (
                    "TRUE", "FALSE", "NULL", "BOOL"
                ):
                    ctx.forward_decls.add(base)

    # Parse the decompiled code for additional type info
    code = result.get("decompiled_code", result.get("code", ""))
    if code:
        code_ctx = extract_types_from_code(code)
        ctx.merge(code_ctx)

    return ctx


# =============================================================================
# Main Export Functions
# =============================================================================

class TypeExporter:
    """Exports types from Ghidra via MCP."""

    def __init__(self, client: MCPClient, verbose: bool = False):
        self.client = client
        self.verbose = verbose
        self.context = TypeContext()
        # Add standard typedefs
        self.context.typedefs = get_standard_typedefs()

    def log(self, msg: str) -> None:
        """Print verbose message."""
        if self.verbose:
            print(f"[export_types] {msg}", file=sys.stderr)

    def export_function_types(self, function_name: str) -> TypeContext:
        """
        Export types relevant to a specific function.

        This decompiles the function and extracts type information from:
        - The function signature
        - Referenced types in the code
        - Called functions (recursively, if requested)
        """
        ctx = TypeContext()
        ctx.typedefs = get_standard_typedefs()

        self.log(f"Decompiling function: {function_name}")

        try:
            result = self.client.decompile_function(function_name)
        except MCPError as e:
            print(f"Warning: Could not decompile {function_name}: {e}",
                  file=sys.stderr)
            return ctx

        # Extract types from the decompilation
        func_ctx = extract_class_info_from_decompilation(result)
        ctx.merge(func_ctx)

        # Get cross-references to find related functions/globals
        self.log(f"Getting cross-references for: {function_name}")
        try:
            xrefs = self.client.list_xrefs(function_name)
            refs = xrefs.get("references", xrefs.get("xrefs", []))

            for ref in refs[:10]:  # Limit to avoid too many requests
                ref_name = ref.get("name", ref.get("symbol", ""))
                if "::" in ref_name:
                    # It's a method, add forward declaration for the class
                    class_name = ref_name.split("::")[0]
                    ctx.forward_decls.add(class_name)

                # If it starts with "The", it's likely a global
                if ref_name.startswith("The"):
                    type_name = ref_name[3:]
                    ctx.globals.append(GlobalVar(ref_name, type_name))
                    ctx.forward_decls.add(type_name)

        except MCPError as e:
            self.log(f"Warning: Could not get xrefs: {e}")

        return ctx

    def export_all_types(self, symbol_pattern: str = ".*",
                         limit: int = 100) -> TypeContext:
        """
        Export all discoverable types from the binary.

        This searches for symbols and attempts to extract type information
        from function signatures and decompilations.
        """
        ctx = TypeContext()
        ctx.typedefs = get_standard_typedefs()

        # Search for functions
        self.log(f"Searching for symbols matching: {symbol_pattern}")

        try:
            # Get exports (these are likely our best-quality symbols)
            exports = self.client.list_exports(
                query=symbol_pattern, limit=limit
            )
            symbols = exports.get("exports", exports.get("symbols", []))

            self.log(f"Found {len(symbols)} exported symbols")

            for i, sym in enumerate(symbols):
                sym_name = sym.get("name", "")
                sym_type = sym.get("type", "")

                if not sym_name:
                    continue

                # Skip non-function symbols for decompilation
                if sym_type not in ("Function", "function", ""):
                    continue

                # Extract class name from mangled/demangled name
                if "::" in sym_name:
                    class_name = sym_name.split("::")[0]
                    ctx.forward_decls.add(class_name)

                # Decompile a sample of functions to get signatures
                if i < 20:  # Limit decompilations to avoid slowdown
                    self.log(f"Decompiling: {sym_name}")
                    try:
                        result = self.client.decompile_function(sym_name)
                        func_ctx = extract_class_info_from_decompilation(result)
                        ctx.merge(func_ctx)
                    except MCPError as e:
                        self.log(f"  Skipped: {e}")

        except MCPError as e:
            print(f"Warning: Could not list exports: {e}", file=sys.stderr)

        # Also search for common global patterns
        self.log("Searching for global variables...")
        try:
            for pattern in ["The", "g_"]:
                globals_result = self.client.search_symbols(
                    pattern, limit=50
                )
                symbols = globals_result.get(
                    "symbols", globals_result.get("results", [])
                )

                for sym in symbols:
                    name = sym.get("name", "")
                    if name.startswith("The"):
                        type_name = name[3:]
                        ctx.globals.append(GlobalVar(name, type_name))
                        ctx.forward_decls.add(type_name)
                    elif name.startswith("g_"):
                        # Can't infer type easily, skip or use void*
                        pass

        except MCPError as e:
            self.log(f"Warning: Could not search for globals: {e}")

        return ctx


# =============================================================================
# CLI Interface
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Export Ghidra types as C headers for m2c context",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s --function "Game::Poll"
      Export types relevant to Game::Poll

  %(prog)s --function "CharMirror::Load" -o context.h
      Export to a file

  %(prog)s --all --limit 50
      Export types from first 50 exported symbols

  %(prog)s --all --pattern "Game.*"
      Export types from symbols matching pattern

Usage with m2c:
  %(prog)s --function "Foo::Bar" -o ctx.h
  python3 ~/code/milohax/m2c/m2c.py -t ppc --context ctx.h input.s
        """
    )

    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--function", "-f",
        metavar="NAME",
        help="Export types relevant to a specific function"
    )
    mode.add_argument(
        "--all", "-a",
        action="store_true",
        help="Export all discoverable types"
    )

    parser.add_argument(
        "--output", "-o",
        metavar="FILE",
        help="Output file (default: stdout)"
    )
    parser.add_argument(
        "--binary", "-b",
        default=DEFAULT_BINARY,
        help=f"Binary name in Ghidra project (default: {DEFAULT_BINARY})"
    )
    parser.add_argument(
        "--pattern", "-p",
        default=".*",
        help="Symbol pattern for --all mode (default: .*)"
    )
    parser.add_argument(
        "--limit", "-l",
        type=int,
        default=100,
        help="Maximum symbols to process for --all mode (default: 100)"
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Print verbose progress messages"
    )
    parser.add_argument(
        "--json", "-j",
        action="store_true",
        help="Output as JSON instead of C header"
    )
    parser.add_argument(
        "--reinit",
        action="store_true",
        help="Force MCP session re-initialization"
    )

    args = parser.parse_args()

    try:
        # Initialize MCP client
        client = MCPClient(binary=args.binary)
        client.initialize(force=args.reinit)

        exporter = TypeExporter(client, verbose=args.verbose)

        # Export types based on mode
        if args.function:
            ctx = exporter.export_function_types(args.function)
        else:  # --all
            ctx = exporter.export_all_types(
                symbol_pattern=args.pattern,
                limit=args.limit
            )

        # Generate output
        if args.json:
            output_data = {
                "typedefs": [
                    {"name": t.name, "base_type": t.base_type}
                    for t in ctx.typedefs
                ],
                "structs": [
                    {
                        "name": s.name,
                        "is_class": s.is_class,
                        "base_class": s.base_class,
                        "size": s.size,
                        "fields": [
                            {
                                "name": f.name,
                                "type": f.type_name,
                                "offset": f.offset
                            }
                            for f in s.fields
                        ]
                    }
                    for s in ctx.structs
                ],
                "enums": [
                    {
                        "name": e.name,
                        "values": [
                            {"name": v.name, "value": v.value}
                            for v in e.values
                        ]
                    }
                    for e in ctx.enums
                ],
                "globals": [
                    {"name": g.name, "type": g.type_name}
                    for g in ctx.globals
                ],
                "functions": [
                    {
                        "name": f.name,
                        "return_type": f.return_type,
                        "params": [
                            {"type": ptype, "name": pname}
                            for ptype, pname in f.params
                        ],
                        "is_method": f.is_method,
                        "class_name": f.class_name
                    }
                    for f in ctx.functions
                ],
                "forward_decls": list(ctx.forward_decls)
            }
            output = json.dumps(output_data, indent=2)
        else:
            output = ctx.to_c()

        # Write output
        if args.output:
            Path(args.output).write_text(output)
            if args.verbose:
                print(f"Wrote output to: {args.output}", file=sys.stderr)
        else:
            print(output)

    except MCPError as e:
        print(f"Error: {e}", file=sys.stderr)
        print("\nTroubleshooting:", file=sys.stderr)
        print("  1. Check if Ghidra MCP is running: "
              "./tools/ghidra/pyghidra-service.sh status", file=sys.stderr)
        print("  2. Start if needed: ./tools/ghidra/pyghidra-service.sh start",
              file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)


if __name__ == "__main__":
    main()
