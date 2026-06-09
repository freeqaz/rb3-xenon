"""C2FuncMap: accumulated c2.dll function knowledge base.

Maps c2.dll RVAs to labels and evidence from experiments. Addresses with
consistent callgrind divergence across multiple experiments get labeled as
register allocator candidates.
"""

import json
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from .invoker import PROJECT_ROOT, C2_IMAGE_BASE, C2_TEXT_START, C2_TEXT_END


DEFAULT_FUNCMAP_PATH = PROJECT_ROOT / "tools" / "c2_funcmap.json"


class C2FuncMap:
    """Database of c2.dll address observations."""

    def __init__(self, path: Optional[Path] = None):
        self.path = path or DEFAULT_FUNCMAP_PATH
        self.entries: Dict[str, dict] = {}  # hex RVA -> entry
        if self.path.exists():
            self._load()

    def _load(self):
        with open(self.path) as f:
            data = json.load(f)
        self.entries = data.get("entries", {})

    def save(self):
        data = {
            "c2_image_base": hex(C2_IMAGE_BASE),
            "c2_text_range": [hex(C2_TEXT_START), hex(C2_TEXT_END)],
            "entries": self.entries,
        }
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with open(self.path, "w") as f:
            json.dump(data, f, indent=2)

    def add_observation(
        self,
        rva: int,
        evidence_tag: str,
        delta: int,
        label: Optional[str] = None,
    ):
        """Record that an address behaved differently in an experiment.

        Args:
            rva: Relative virtual address within c2.dll (addr - C2_IMAGE_BASE)
            evidence_tag: Identifier for the experiment (e.g. "test_a_vs_b")
            delta: Hit count difference (positive = more in A, negative = more in B)
            label: Optional human label for this address
        """
        key = f"0x{rva:x}"
        if key not in self.entries:
            self.entries[key] = {
                "rva": rva,
                "va": rva + C2_IMAGE_BASE,
                "observations": [],
            }
        entry = self.entries[key]
        entry["observations"].append({
            "tag": evidence_tag,
            "delta": delta,
        })
        if label:
            entry["label"] = label

    def get_breakpoints(self, min_evidence: int = 2) -> List[Tuple[int, dict]]:
        """Get addresses with enough evidence for GDB breakpoints.

        Returns list of (virtual_address, entry) pairs sorted by observation count.
        """
        results = []
        for key, entry in self.entries.items():
            if len(entry["observations"]) >= min_evidence:
                results.append((entry["va"], entry))
        results.sort(key=lambda x: len(x[1]["observations"]), reverse=True)
        return results

    def get_hot_clusters(
        self, min_evidence: int = 2, cluster_gap: int = 64
    ) -> List[List[Tuple[int, dict]]]:
        """Group breakpoint-worthy addresses into clusters of adjacent addresses.

        Addresses within `cluster_gap` bytes are grouped together. Returns
        clusters sorted by total observation count.
        """
        bps = self.get_breakpoints(min_evidence)
        if not bps:
            return []

        clusters: List[List[Tuple[int, dict]]] = []
        current: List[Tuple[int, dict]] = [bps[0]]

        for va, entry in bps[1:]:
            if va - current[-1][0] <= cluster_gap:
                current.append((va, entry))
            else:
                clusters.append(current)
                current = [(va, entry)]
        clusters.append(current)

        # Sort by total observations in cluster
        clusters.sort(
            key=lambda c: sum(len(e["observations"]) for _, e in c),
            reverse=True,
        )
        return clusters

    def summary(self) -> str:
        """Return a summary of the funcmap."""
        total = len(self.entries)
        labeled = sum(1 for e in self.entries.values() if "label" in e)
        obs_counts = [len(e["observations"]) for e in self.entries.values()]
        max_obs = max(obs_counts) if obs_counts else 0
        bp_worthy = sum(1 for c in obs_counts if c >= 2)

        return (
            f"C2 FuncMap: {total} addresses, {labeled} labeled, "
            f"{bp_worthy} with 2+ observations (max {max_obs})"
        )
