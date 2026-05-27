#!/usr/bin/env python3
"""
DirectGhidraClient: Direct Python→Java→Ghidra bridge for decompilation context.

Provides direct access to Ghidra without HTTP overhead, optimized for orchestrator
use cases. Manages JVM lifecycle and provides instance caching.

Key features:
- Lazy JVM initialization (starts once, reused for all calls)
- Single instance pattern (shared across orchestrator)
- Direct pyghidra integration (no HTTP MCP overhead)
- Graceful failure handling (exception thrown for orchestrator to catch)
- Multi-strategy function lookup (mangled, demangled, address-based)
"""

import logging
import sys
from pathlib import Path
from typing import List, Optional, Tuple

# Default binary path for rb3-xenon project
_PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent
DEFAULT_BINARY_PATH = _PROJECT_ROOT / "orig" / "45410914" / "default.xex"

# Configure logging
logger = logging.getLogger(__name__)


class DirectGhidraClientError(Exception):
    """Exception raised when Ghidra is unavailable or operations fail."""

    pass


class DirectGhidraClient:
    """Direct Python→Java→Ghidra bridge without HTTP overhead.

    Manages:
    - Lazy JVM startup (starts once per instance)
    - Binary import with PowerPC:BE:64:Xenon language
    - Decompilation and cross-reference queries
    - Single instance caching strategy
    """

    # Class-level cache for singleton pattern
    _instance: Optional["DirectGhidraClient"] = None
    _jvm_started = False

    def __init__(
        self,
        binary_path: str,
        project_dir: str,
        project_name: str = "DirectGhidraClient",
        verbose: bool = False,
        map_file: Optional[Path] = None,
    ):
        """Initialize DirectGhidraClient.

        Args:
            binary_path: Path to binary file (e.g., "/path/to/default.xex")
            project_dir: Directory for Ghidra project (e.g., "/tmp/ghidra_projects")
            project_name: Name of Ghidra project
            verbose: Enable verbose logging
            map_file: Optional path to linker map file for multi-strategy symbol lookup
                      (rb3-xenon has no leaked .map; use tools/fingerprint_match.py instead)

        Raises:
            DirectGhidraClientError: If JVM initialization or binary import fails
        """
        self.binary_path = Path(binary_path)
        self.project_dir = Path(project_dir)
        self.project_name = project_name
        self.verbose = verbose
        self.map_file = map_file  # May be None — rb3-xenon has no .map file

        # Validate binary exists
        if not self.binary_path.exists():
            raise DirectGhidraClientError(
                f"Binary file not found: {self.binary_path}"
            )

        # Start JVM once (class-level)
        if not DirectGhidraClient._jvm_started:
            self._start_jvm()
            DirectGhidraClient._jvm_started = True

        # Initialize Ghidra context and import binary
        self._initialize_ghidra()

    def _start_jvm(self) -> None:
        """Start JVM for Ghidra access.

        This is called once per process and reused for all DirectGhidraClient
        instances.

        Raises:
            DirectGhidraClientError: If JVM startup fails
        """
        try:
            import pyghidra

            if self.verbose:
                logger.info("Starting JVM for pyghidra...")
            pyghidra.start(verbose=False)  # Use non-verbose to reduce noise
            if self.verbose:
                logger.info("JVM started successfully")
        except Exception as e:
            raise DirectGhidraClientError(
                f"Failed to start JVM for Ghidra: {e}"
            ) from e

    def _initialize_ghidra(self) -> None:
        """Initialize Ghidra context and import binary.

        Sets up PyGhidraContext, creates or opens existing project, and imports
        binary with language auto-detection or explicit PowerPC spec.

        Raises:
            DirectGhidraClientError: If project creation or binary import fails
        """
        try:
            from pyghidra_mcp.context import PyGhidraContext
            from pyghidra_mcp.tools import GhidraTools

            if self.verbose:
                logger.info(f"Creating/opening Ghidra project: {self.project_name}")

            # Create PyGhidraContext (manages project lifecycle)
            self.ctx = PyGhidraContext(
                project_name=self.project_name,
                project_path=self.project_dir,
                force_analysis=False,
                verbose_analysis=False,
            )

            # Check if binary is already loaded
            programs = self.ctx.programs
            program_name = None
            program_info = None

            # If project already has programs, use the first one
            if programs:
                if self.verbose:
                    logger.info(f"Project already has loaded programs: {list(programs.keys())}")
                program_name = list(programs.keys())[0]
                program_info = programs[program_name]
            else:
                # Import binary - try with language spec first, fall back to auto-detect
                if self.verbose:
                    logger.info(f"Importing binary: {self.binary_path}")

                language = None
                compiler = None

                # For XEX files, try PowerPC language if available
                if str(self.binary_path).endswith(".xex"):
                    # Try explicit language first
                    try:
                        if self.verbose:
                            logger.info("Attempting import with PowerPC:BE:64:Xenon")
                        self.ctx.import_binary(
                            binary_path=self.binary_path,
                            language="PowerPC:BE:64:Xenon",
                            compiler=None,
                        )
                        if self.verbose:
                            logger.info("Import succeeded with PowerPC:BE:64:Xenon")
                    except Exception as e:
                        # Fall back to auto-detection
                        if self.verbose:
                            logger.info(
                                f"PowerPC:BE:64:Xenon failed ({e}), trying auto-detect"
                            )
                        try:
                            self.ctx.import_binary(
                                binary_path=self.binary_path,
                                language=None,
                                compiler=None,
                            )
                            if self.verbose:
                                logger.info("Import succeeded with auto-detect")
                        except Exception as e2:
                            raise DirectGhidraClientError(
                                f"Failed to import binary with both explicit and auto-detect: {e2}"
                            ) from e2
                else:
                    # Non-XEX files, use auto-detection
                    self.ctx.import_binary(
                        binary_path=self.binary_path,
                        language=None,
                        compiler=None,
                    )

                # Get the imported program
                programs = self.ctx.programs
                if not programs:
                    raise DirectGhidraClientError("No programs loaded after import")

                program_name = list(programs.keys())[0]
                program_info = programs[program_name]

            if not program_info:
                raise DirectGhidraClientError("Failed to load program info")

            if self.verbose:
                logger.info(f"Using program: {program_name}")

            # Analyze the project only if needed (builds indices)
            # Skip if already analyzed
            if not program_info.analysis_complete:
                if self.verbose:
                    logger.info("Analyzing binary...")
                self.ctx.analyze_project()

            # Initialize tools for decompilation and xrefs
            # map_file may be None for rb3-xenon (no leaked .map available)
            self.tools = GhidraTools(program_info, cache_manager=None, map_file=self.map_file)

        except DirectGhidraClientError:
            raise
        except Exception as e:
            raise DirectGhidraClientError(
                f"Failed to initialize Ghidra: {e}"
            ) from e

    def decompile_function(self, symbol: str) -> str:
        """Get original decompilation from Ghidra.

        Args:
            symbol: Function symbol name or address (e.g., "fn_82001234" or "0x82001234")

        Returns:
            Pseudo-C decompilation code

        Raises:
            DirectGhidraClientError: If symbol not found or decompilation fails
        """
        try:
            if self.verbose:
                logger.info(f"Decompiling: {symbol}")

            result = self.tools.decompile_function_by_name_or_addr(symbol)

            if self.verbose:
                logger.info(f"Decompilation successful: {len(result.code)} chars")

            return result.code

        except ValueError as e:
            raise DirectGhidraClientError(f"Symbol not found: {symbol}") from e
        except Exception as e:
            raise DirectGhidraClientError(
                f"Decompilation failed for {symbol}: {e}"
            ) from e

    def list_cross_references(self, symbol: str) -> Tuple[List[str], List[str]]:
        """Get callers and callees for a function.

        Args:
            symbol: Function symbol name or address

        Returns:
            Tuple of (callers, callees) lists

        Raises:
            DirectGhidraClientError: If symbol not found or xref lookup fails
        """
        try:
            if self.verbose:
                logger.info(f"Looking up cross-references for: {symbol}")

            xrefs = self.tools.list_cross_references(symbol)

            # Separate into callers (references TO this function)
            # and callees (references FROM this function)
            callers = []
            callees = []

            for xref in xrefs:
                func_name = xref.function_name
                if func_name:
                    # For now, we treat all xrefs as callers (incoming references)
                    # A more sophisticated approach could use reference type to
                    # distinguish, but this is sufficient for context injection
                    callers.append(func_name)

            if self.verbose:
                logger.info(
                    f"Found {len(callers)} callers, {len(callees)} callees"
                )

            return callers, callees

        except ValueError as e:
            raise DirectGhidraClientError(f"Symbol not found: {symbol}") from e
        except Exception as e:
            raise DirectGhidraClientError(
                f"Cross-reference lookup failed for {symbol}: {e}"
            ) from e

    def close(self) -> None:
        """Close Ghidra project and cleanup.

        Note: JVM remains running for potential reuse in the same process.
        """
        try:
            if hasattr(self, "ctx") and self.ctx:
                if self.verbose:
                    logger.info("Closing Ghidra project...")
                self.ctx.close()
                if self.verbose:
                    logger.info("Project closed")
        except Exception as e:
            logger.error(f"Error closing Ghidra project: {e}")

    @classmethod
    def get_instance(
        cls,
        binary_path: str,
        project_dir: str,
        project_name: str = "DirectGhidraClient",
        verbose: bool = False,
        map_file: Optional[Path] = None,
    ) -> "DirectGhidraClient":
        """Get or create singleton instance.

        Implements instance caching for orchestrator use case (single client
        shared across multiple function analyses).

        Args:
            binary_path: Path to binary file
            project_dir: Directory for Ghidra project
            project_name: Name of Ghidra project
            verbose: Enable verbose logging
            map_file: Optional path to linker map file for symbol lookup
                      (rb3-xenon has no leaked .map; omit this argument)

        Returns:
            DirectGhidraClient instance

        Raises:
            DirectGhidraClientError: If initialization fails
        """
        if cls._instance is None:
            cls._instance = cls(
                binary_path=binary_path,
                project_dir=project_dir,
                project_name=project_name,
                verbose=verbose,
                map_file=map_file,
            )
        return cls._instance


def main():
    """CLI for testing DirectGhidraClient."""
    import argparse

    parser = argparse.ArgumentParser(
        description="Test DirectGhidraClient with a symbol"
    )
    parser.add_argument("symbol", help="Function symbol or address to decompile")
    parser.add_argument(
        "--binary",
        default=str(DEFAULT_BINARY_PATH),
        help="Path to binary file",
    )
    parser.add_argument(
        "--project-dir",
        default="/tmp/claude/ghidra_projects",
        help="Ghidra project directory",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable verbose logging"
    )

    args = parser.parse_args()

    # Setup logging
    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)
    else:
        logging.basicConfig(level=logging.INFO)

    try:
        # Test DirectGhidraClient
        print(f"Initializing DirectGhidraClient...")
        print(f"  Binary: {args.binary}")
        print(f"  Project: {args.project_dir}")
        print(f"  Symbol: {args.symbol}")
        print()

        client = DirectGhidraClient(
            binary_path=args.binary,
            project_dir=args.project_dir,
            verbose=args.verbose,
        )

        # Test decompilation
        print("=" * 80)
        print("DECOMPILATION")
        print("=" * 80)
        decompilation = client.decompile_function(args.symbol)
        print(decompilation)
        print()

        # Test cross-references
        print("=" * 80)
        print("CROSS-REFERENCES")
        print("=" * 80)
        callers, callees = client.list_cross_references(args.symbol)

        print(f"Callers ({len(callers)} total):")
        for caller in callers[:20]:
            print(f"  - {caller}")
        if len(callers) > 20:
            print(f"  ... and {len(callers) - 20} more")
        print()

        print(f"Callees ({len(callees)} total):")
        for callee in callees[:20]:
            print(f"  - {callee}")
        if len(callees) > 20:
            print(f"  ... and {len(callees) - 20} more")
        print()

        client.close()
        print("SUCCESS: DirectGhidraClient test completed")

    except DirectGhidraClientError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: Unexpected error: {e}", file=sys.stderr)
        if args.verbose:
            import traceback

            traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
