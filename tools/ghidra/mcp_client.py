#!/usr/bin/env python3
"""
MCP client for Ghidra pyghidra-mcp server.

Handles session initialization, JSON-RPC formatting, and response parsing.

Compatible with FastMCP transport (new pyghidra-mcp using Uvicorn on port 8002).
"""

import json
import os
import sys
from pathlib import Path
from typing import Any, Optional

try:
    import requests
except ImportError:
    print("Error: requests library not found. Install with: pip install requests", file=sys.stderr)
    sys.exit(1)


# Default configuration
# Note: rb3-xenon uses port 8002 (DC3 owns 8000, rb3-Wii reserves 8001)
MCP_URL = "http://127.0.0.1:8002/mcp"
SESSION_CACHE_FILE = Path("/tmp/claude/ghidra_mcp_session_rb3xenon.txt")
# Binary name is dynamically resolved - pyghidra-mcp uses SHA1 hash suffix
DEFAULT_BINARY = None  # Will be resolved via list_binaries()


class MCPError(Exception):
    """Exception raised for MCP-related errors."""
    pass


class MCPClient:
    """Client for communicating with Ghidra MCP server."""

    def __init__(self, url: str = MCP_URL, binary: Optional[str] = DEFAULT_BINARY):
        self.url = url
        self._binary = binary  # May be None, resolved lazily
        self._binary_resolved = binary is not None
        self.session_id: Optional[str] = None
        self._request_id = 0

    @property
    def binary(self) -> str:
        """Get binary name, resolving dynamically if needed."""
        if not self._binary_resolved:
            self._resolve_binary()
        return self._binary or "default.xex-997567"  # Fallback

    def _resolve_binary(self) -> None:
        """Resolve binary name by querying list_binaries."""
        if self._binary_resolved:
            return

        try:
            # Ensure we have a session first
            if not self.session_id:
                self.initialize()

            binaries = self.list_binaries()

            # Find binary matching "default.xex" pattern
            # pyghidra-mcp generates names like "default.xex-997567"
            if isinstance(binaries, dict) and "binaries" in binaries:
                binary_list = binaries["binaries"]
            elif isinstance(binaries, list):
                binary_list = binaries
            else:
                binary_list = []

            for binary in binary_list:
                name = binary.get("name", "") if isinstance(binary, dict) else str(binary)
                if name.startswith("default.xex"):
                    self._binary = name.lstrip("/")
                    self._binary_resolved = True
                    return

            # If no match found, use first available binary
            if binary_list:
                first = binary_list[0]
                name = first.get("name", "") if isinstance(first, dict) else str(first)
                self._binary = name.lstrip("/")

            self._binary_resolved = True

        except MCPError:
            # Fall back to default
            self._binary = "default.xex-997567"
            self._binary_resolved = True

    def _next_id(self) -> int:
        """Get next request ID."""
        self._request_id += 1
        return self._request_id

    def _load_cached_session(self) -> Optional[str]:
        """Load cached session ID if it exists."""
        try:
            if SESSION_CACHE_FILE.exists():
                return SESSION_CACHE_FILE.read_text().strip()
        except (IOError, OSError):
            pass
        return None

    def _save_session(self, session_id: str) -> None:
        """Save session ID to cache file."""
        try:
            SESSION_CACHE_FILE.parent.mkdir(parents=True, exist_ok=True)
            SESSION_CACHE_FILE.write_text(session_id)
        except (IOError, OSError) as e:
            # Non-fatal - just means we won't cache
            print(f"Warning: Could not cache session: {e}", file=sys.stderr)

    def _parse_sse_response(self, response: requests.Response) -> dict:
        """
        Parse SSE-formatted response and extract JSON data.

        Compatible with both old streamable-http and new FastMCP/Uvicorn transports.
        FastMCP may return plain JSON directly instead of SSE format.
        """
        content = response.text

        # Handle SSE format: look for 'data:' lines
        data_line = None
        for line in content.split('\n'):
            line = line.strip()
            if line.startswith('data:'):
                data_line = line[5:].strip()
                break

        if data_line:
            try:
                return json.loads(data_line)
            except json.JSONDecodeError as e:
                raise MCPError(f"Failed to parse SSE data as JSON: {e}\nData: {data_line}")

        # If no SSE format, try parsing as plain JSON (FastMCP compatibility)
        try:
            return response.json()
        except json.JSONDecodeError as e:
            raise MCPError(f"Failed to parse response as JSON: {e}\nResponse: {content[:500]}")

    def _make_request(self, method: str, params: Optional[dict] = None, timeout: int = 60) -> dict:
        """Make a JSON-RPC request to the MCP server.

        Args:
            method: JSON-RPC method name
            params: Optional parameters dict
            timeout: Request timeout in seconds (default 60, use higher for long operations)
        """
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        }

        if self.session_id:
            headers["mcp-session-id"] = self.session_id

        payload = {
            "jsonrpc": "2.0",
            "id": self._next_id(),
            "method": method,
        }
        if params:
            payload["params"] = params

        try:
            response = requests.post(self.url, headers=headers, json=payload, timeout=timeout)
        except requests.exceptions.ConnectionError:
            raise MCPError(
                f"Could not connect to MCP server at {self.url}. "
                f"Is pyghidra-mcp running? Check with: ./tools/ghidra/pyghidra-service.sh status"
            )
        except requests.exceptions.Timeout:
            raise MCPError("Request timed out")
        except requests.exceptions.RequestException as e:
            raise MCPError(f"Request failed: {e}")

        # Update session ID if provided
        new_session = response.headers.get("mcp-session-id")
        if new_session:
            self.session_id = new_session
            self._save_session(new_session)

        if response.status_code != 200:
            raise MCPError(f"HTTP {response.status_code}: {response.text[:500]}")

        return self._parse_sse_response(response)

    def _send_initialized_notification(self) -> None:
        """Send the 'initialized' notification required by MCP spec.

        After the initialize request/response, the client must send this
        notification before making any tool calls. Without it, the server
        stays in 'Initializing' state and rejects requests.
        """
        headers = {
            "Content-Type": "application/json",
            "Accept": "application/json, text/event-stream",
        }
        if self.session_id:
            headers["mcp-session-id"] = self.session_id

        # Notifications have no "id" field per JSON-RPC spec
        payload = {
            "jsonrpc": "2.0",
            "method": "notifications/initialized",
        }

        try:
            requests.post(self.url, headers=headers, json=payload, timeout=10)
        except requests.exceptions.RequestException:
            pass  # Best-effort, server may not require it

    def initialize(self, force: bool = False) -> str:
        """Initialize MCP session. Returns session ID."""
        # Try cached session first
        if not force:
            cached = self._load_cached_session()
            if cached:
                self.session_id = cached
                # Re-initialize with the cached session to validate it.
                # Just probing with a tool call would fail if the server
                # restarted (no init handshake done for this connection).
                try:
                    result = self._make_request("initialize", {
                        "protocolVersion": "2024-11-05",
                        "capabilities": {},
                        "clientInfo": {"name": "ghidra-cli", "version": "1.0"}
                    })
                    if "error" not in result and self.session_id:
                        self._send_initialized_notification()
                        return self.session_id
                except MCPError:
                    pass
                # Session expired or server restarted, fall through
                self.session_id = None

        result = self._make_request("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {"name": "ghidra-cli", "version": "1.0"}
        })

        if "error" in result:
            raise MCPError(f"Initialize failed: {result['error']}")

        if not self.session_id:
            raise MCPError("Server did not return session ID")

        self._send_initialized_notification()
        return self.session_id

    def call_tool(self, tool_name: str, arguments: dict, timeout: int = 60) -> Any:
        """Call an MCP tool and return the structured content.

        Args:
            tool_name: Name of the MCP tool to call
            arguments: Tool arguments dict
            timeout: Request timeout in seconds (default 60, use higher for long operations)
        """
        # Ensure we have a session
        if not self.session_id:
            self.initialize()

        result = self._make_request("tools/call", {
            "name": tool_name,
            "arguments": arguments
        }, timeout=timeout)

        if "error" in result:
            error = result["error"]
            if isinstance(error, dict):
                msg = error.get("message", str(error))
            else:
                msg = str(error)
            raise MCPError(f"Tool call failed: {msg}")

        # Extract result
        result_data = result.get("result", {})

        # Check for isError flag in result
        if result_data.get("isError"):
            content = result_data.get("content", [])
            if isinstance(content, list) and len(content) > 0:
                first = content[0]
                if isinstance(first, dict) and "text" in first:
                    raise MCPError(first["text"])
            raise MCPError("Tool returned an error")

        # Prefer structuredContent, fall back to content
        if "structuredContent" in result_data:
            return result_data["structuredContent"]
        elif "content" in result_data:
            content = result_data["content"]
            if isinstance(content, list) and len(content) > 0:
                # Content is typically [{"type": "text", "text": "..."}]
                first = content[0]
                if isinstance(first, dict) and "text" in first:
                    try:
                        return json.loads(first["text"])
                    except json.JSONDecodeError:
                        return first["text"]
                return first
            return content

        return result_data

    # Convenience methods for each tool

    def decompile_function(self, name_or_address: str) -> dict:
        """Decompile a function by name or address."""
        return self.call_tool("decompile_function", {
            "binary_name": self.binary,
            "name_or_address": name_or_address
        })

    def search_symbols(self, query: str, offset: int = 0, limit: int = 25) -> dict:
        """Search for symbols by name."""
        return self.call_tool("search_symbols_by_name", {
            "binary_name": self.binary,
            "query": query,
            "offset": offset,
            "limit": limit
        })

    def search_strings(self, query: str, limit: int = 100) -> dict:
        """Search for strings in the binary."""
        return self.call_tool("search_strings", {
            "binary_name": self.binary,
            "query": query,
            "limit": limit
        })

    def search_code(self, query: str, limit: int = 5) -> dict:
        """Search for code patterns."""
        return self.call_tool("search_code", {
            "binary_name": self.binary,
            "query": query,
            "limit": limit
        })

    def list_xrefs(self, name_or_address: str) -> dict:
        """List cross-references to/from a symbol or address."""
        return self.call_tool("list_cross_references", {
            "binary_name": self.binary,
            "name_or_address": name_or_address
        })

    def get_callgraph(self, function_name: str, direction: str = "calling") -> dict:
        """Generate call graph for a function."""
        return self.call_tool("gen_callgraph", {
            "binary_name": self.binary,
            "function_name": function_name,
            "direction": direction
        })

    def list_exports(self, query: str = ".*", offset: int = 0, limit: int = 25) -> dict:
        """List exported symbols."""
        return self.call_tool("list_exports", {
            "binary_name": self.binary,
            "query": query,
            "offset": offset,
            "limit": limit
        })

    def list_imports(self, query: str = ".*", offset: int = 0, limit: int = 25) -> dict:
        """List imported symbols."""
        return self.call_tool("list_imports", {
            "binary_name": self.binary,
            "query": query,
            "offset": offset,
            "limit": limit
        })

    def read_bytes(self, address: str, size: int = 32) -> dict:
        """Read bytes from an address."""
        return self.call_tool("read_bytes", {
            "binary_name": self.binary,
            "address": address,
            "size": size
        })

    def list_structures(self, query: str = ".*", offset: int = 0, limit: int = 100) -> dict:
        """Get structure data types from Ghidra's Data Type Manager."""
        return self.call_tool("list_structures", {
            "binary_name": self.binary,
            "query": query,
            "offset": offset,
            "limit": limit
        })

    def extract_structures(self, max_functions: int = 0, timeout_per_func: int = 30) -> dict:
        """Extract structures by batch-decompiling functions. Long-running.

        This operation can take hours for large binaries. The timeout is set conservatively
        high to avoid premature connection errors. Progress is logged server-side.
        """
        # Estimate timeout: ~30s per function + overhead
        # For all functions (~66K), this could take 30+ hours, so use a very high timeout
        # The server will stream progress, so we just need to wait
        estimated_timeout = 3600 * 10 if max_functions == 0 else max(600, max_functions * timeout_per_func)

        return self.call_tool("extract_structures", {
            "binary_name": self.binary,
            "max_functions": max_functions,
            "timeout_per_func": timeout_per_func,
        }, timeout=estimated_timeout)

    def create_structures(self, class_defs: list[dict]) -> dict:
        """Create structure data types in Ghidra's DTM.

        Seeds the DTM with structure definitions from rb3-xenon headers.

        Args:
            class_defs: List of class definitions. Each dict:
                {
                    "name": str,
                    "members": [{"name": str, "type_str": str, "offset": int, "size": int?}],
                    "total_size": int?
                }

        Returns:
            {"created": int, "errors": int}
        """
        return self.call_tool("create_structures", {
            "binary_name": self.binary,
            "class_defs": class_defs,
        })

    def apply_this_types(self, class_methods: dict[str, list[str]]) -> dict:
        """Apply this pointer types to member functions.

        Sets the first parameter (this) to the appropriate class pointer type.

        Args:
            class_methods: {class_name: [address_hex, ...]}
                Addresses should be hex strings without 0x prefix (e.g., "823486e0")

        Returns:
            {"applied": int, "missing_type": int, "no_function": int}
        """
        return self.call_tool("apply_this_types", {
            "binary_name": self.binary,
            "class_methods": class_methods,
        })

    def bulk_create_functions(self, addresses: list[str]) -> dict:
        """Create Function objects at addresses where Ghidra auto-analysis missed them.

        Args:
            addresses: List of hex address strings without 0x prefix

        Returns:
            {"created": int, "already_exist": int, "failed": int}
        """
        return self.call_tool("bulk_create_functions", {
            "binary_name": self.binary,
            "addresses": addresses,
        }, timeout=600)

    def apply_demangled_signatures(self, symbols: list[dict], batch_size: int = 2000) -> dict:
        """Apply full function signatures by demangling MSVC mangled names.

        Batches large requests to avoid timeouts.

        Args:
            symbols: List of {"mangled": str, "address": str} dicts
            batch_size: Number of symbols per batch (default 2000)

        Returns:
            Aggregated {"applied": int, "partial": int, "no_function": int, "demangle_failed": int, "skipped": int}
        """
        totals = {"applied": 0, "partial": 0, "no_function": 0, "demangle_failed": 0, "skipped": 0}

        for i in range(0, len(symbols), batch_size):
            batch = symbols[i:i + batch_size]
            result = self.call_tool("apply_demangled_signatures", {
                "binary_name": self.binary,
                "symbols": batch,
            }, timeout=600)
            for key in totals:
                totals[key] += result.get(key, 0)

        return totals

    def list_binaries(self) -> dict:
        """List all project binaries."""
        return self.call_tool("list_project_binaries", {})


def create_client(binary: str = DEFAULT_BINARY) -> MCPClient:
    """Create and initialize an MCP client."""
    client = MCPClient(binary=binary)
    client.initialize()
    return client


if __name__ == "__main__":
    # Test connection
    try:
        client = create_client()
        print(f"Connected to MCP server, session: {client.session_id}")
        binaries = client.list_binaries()
        print(f"Available binaries: {json.dumps(binaries, indent=2)}")
    except MCPError as e:
        print(f"Error: {e}", file=sys.stderr)
        print("\nTroubleshooting:", file=sys.stderr)
        print("  1. Check if service is running: ./tools/ghidra/pyghidra-service.sh status", file=sys.stderr)
        print("  2. Start service if needed: ./tools/ghidra/pyghidra-service.sh start", file=sys.stderr)
        print("  3. View logs: ./tools/ghidra/pyghidra-service.sh logs", file=sys.stderr)
        print(f"  4. Verify port {MCP_URL} is correct (rb3-xenon uses port 8002)", file=sys.stderr)
        sys.exit(1)


# =============================================================================
# Troubleshooting Common Connection Issues
# =============================================================================
#
# Connection Errors:
# - "Could not connect": Service not running. Use pyghidra-service.sh start
# - "Session expired": Restart service or use force=True on initialize()
# - "HTTP 404": Wrong URL path. FastMCP uses /mcp
# - "HTTP 500": Service crashed. Check logs with pyghidra-service.sh logs
#
# Port Configuration:
# - rb3-xenon uses port 8002 (DC3 owns 8000, rb3-Wii reserves 8001)
# - Old pyghidra-mcp: streamable-http on port 8765, endpoint /mcp
# - New pyghidra-mcp: FastMCP/Uvicorn on port 8002, endpoint /mcp
#
# Session Management:
# - Sessions are cached in /tmp/claude/ghidra_mcp_session_rb3xenon.txt
# - If session expires, client auto-reinitializes
# - Force new session: client.initialize(force=True)
#
# Response Format:
# - FastMCP may return plain JSON instead of SSE format
# - Client handles both formats transparently via _parse_sse_response()
#
# Service Startup:
# - Use: ./tools/ghidra/pyghidra-service.sh start
# - Check status: ./tools/ghidra/pyghidra-service.sh status
# - View logs: ./tools/ghidra/pyghidra-service.sh logs
# - Service takes ~5-10 seconds to fully initialize after startup
#
# =============================================================================
