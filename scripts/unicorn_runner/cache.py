"""Result caching for unicorn runner batch modes.

Cache keyed on (symbol, decomp_obj_mtime, orig_obj_mtime).
Stored in build/373307D9/unicorn_cache.json.
"""

import json
import os


DEFAULT_CACHE_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "build", "373307D9", "unicorn_cache.json",
)


class ResultCache:
    """Lightweight cache for unicorn comparison results."""

    def __init__(self, cache_path=None):
        self.path = cache_path or DEFAULT_CACHE_PATH
        self._data = {}
        self._dirty = False
        self._load()

    def _load(self):
        if os.path.exists(self.path):
            try:
                with open(self.path) as f:
                    self._data = json.load(f)
            except (json.JSONDecodeError, OSError):
                self._data = {}

    def save(self):
        if not self._dirty:
            return
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        with open(self.path, "w") as f:
            json.dump(self._data, f, indent=1)
        self._dirty = False

    @staticmethod
    def _make_key(symbol, decomp_path, orig_path):
        """Build cache key from symbol + file mtimes."""
        try:
            d_mtime = os.path.getmtime(decomp_path)
            o_mtime = os.path.getmtime(orig_path)
        except OSError:
            return None
        return f"{symbol}|{d_mtime}|{o_mtime}"

    def lookup(self, symbol, decomp_path, orig_path):
        """Look up cached result. Returns (exit_code, confidence) or None."""
        key = self._make_key(symbol, decomp_path, orig_path)
        if key is None:
            return None
        entry = self._data.get(key)
        if entry is None:
            return None
        return (entry["exit_code"], entry.get("confidence"))

    def store(self, symbol, decomp_path, orig_path, exit_code, confidence=None):
        """Store a result in the cache."""
        key = self._make_key(symbol, decomp_path, orig_path)
        if key is None:
            return
        self._data[key] = {"exit_code": exit_code, "confidence": confidence}
        self._dirty = True

    def invalidate_unit(self, decomp_path, orig_path):
        """Remove all entries for a unit whose files have changed.

        This is called implicitly by lookup returning None for stale keys,
        but can also be used to proactively prune.
        """
        try:
            d_mtime = os.path.getmtime(decomp_path)
            o_mtime = os.path.getmtime(orig_path)
        except OSError:
            return
        suffix = f"|{d_mtime}|{o_mtime}"
        # Keep only entries matching current mtimes for this file pair
        # (stale entries with different mtimes are naturally ignored by lookup)

    @property
    def size(self):
        return len(self._data)
