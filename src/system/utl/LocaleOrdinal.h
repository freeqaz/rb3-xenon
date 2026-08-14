#pragma once
#include "utl/Locale.h"

// RB3-360 retail has exactly ONE LocalizeOrdinal, and it takes FOUR arguments
// (fn_827CF0D0). Proven on retail bytes: the prologue captures r4/r5/r6 only
// (`mr r19,r4; mr r20,r5; mr r21,r6`) and never touches r7/r8, and 0 of its 6
// callers stage r7/r8. The 6-argument (Symbol, Locale&) form is DC3-newer;
// both extra parameters were unused in the body anyway.
const char *LocalizeOrdinal(int, LocaleGender, LocaleNumber, bool);
