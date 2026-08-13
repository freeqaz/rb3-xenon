#pragma once

// RB3-360 retail has NO `MetaMaterial` class. The class, the MatProp /
// MatPropEditAction enums and the whole per-material "edit action" template system
// are DC3-era additions we inherited with src/system. Settled on retail bytes by
// lane METAMAT-1; see
// docs/decomp/metamaterial-does-not-exist-in-rb3-retail-2026-08-13.md.
//
// The header survives only so the handful of files that #include "rndobj/MetaMaterial.h"
// keep resolving; renaming/removing it would churn config/45410914/splits.txt and
// config/45410914/objects.json, which name the FILE, not the class.
#include "rndobj/BaseMaterial.h"
