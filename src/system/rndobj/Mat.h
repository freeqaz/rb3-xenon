#pragma once

// RB3-360 retail has ONE material class, RndMat, deriving directly from
// Hmx::Object. The BaseMaterial/RndMat two-class split was a DC3-era refactor we
// inherited with src/system; lane BASEMAT-1 settled it on retail bytes and
// BASEMAT-2 merged the two. See
// docs/decomp/basematerial-is-a-dc3-refactor-2026-08-13.md.
//
// The merged class is DEFINED in rndobj/BaseMaterial.h because BaseMaterial.cpp is
// the root of the decomp unit that scatter-includes Mat.cpp and carries retail's
// material Save (0x82435dc0). The file names are unit-boundary artifacts and no
// longer name classes; renaming them would churn config/45410914/splits.txt, which
// another lane owns. This header stays so the ~30 files that include "rndobj/Mat.h"
// keep working.
#include "rndobj/BaseMaterial.h"
#include "rndobj/MetaMaterial.h"
