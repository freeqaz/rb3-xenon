"""Call-log tuple field indices.

These live here, and not in ``engine.py``, so that consumers which only need to
*read* a call log do not have to import the emulator to do it.

``comparator.py`` is the case that motivated the split: it is pure logic over
recorded results (its 41 tests run entirely on mocks) and its only tie to
``engine`` was this handful of integer constants. Importing ``engine`` pulls in
the Unicorn bindings at module scope, so on a box without the emulator every
one of those pure-logic tests failed with an ImportError from three frames
down. Splitting the constants out makes the comparator suite unconditional.

``engine`` re-exports these names, so ``from .engine import CL_INDEX`` keeps
working for existing callers.
"""

from __future__ import annotations

__all__ = [
    "CL_INDEX", "CL_TRAMP_ADDR", "CL_SRC_OFFSET",
    "CL_R3", "CL_R4", "CL_R5", "CL_R6",
]

CL_INDEX = 0
CL_TRAMP_ADDR = 1
CL_SRC_OFFSET = 2
CL_R3 = 3
CL_R4 = 4
CL_R5 = 5
CL_R6 = 6
