#pragma once
// RB3 retail has no RndFontBase (see rndobj/Font.h for the RTTI/COL evidence).
// The class survives only as the base of the DC3-only RndFont3d and now lives
// in Font.h next to RndFont; this header remains as a compatibility shim for
// the TUs that still include it.
#include "rndobj/Font.h"
