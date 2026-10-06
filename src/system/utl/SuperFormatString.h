#pragma once
#include "utl/MakeString.h"
#include "utl/Locale.h"
#include "utl/Symbol.h"
#include "obj/Data.h"

// RB3's SuperFormatString (retail ctor 0x82BBABF8).  This is the three-argument
// form: it always localises through TheLocale and carries no state beyond
// FormatString.  DC3's later revision adds an explicit Locale/language pair,
// mTokensOnly / mHasPercentFormat and a FinalStr() that can append "%s"; RB3
// has none of that.  Callers that need the unformatted text use RawFmt().
class SuperFormatString : public FormatString {
public:
    SuperFormatString(const char *, const DataArray *, bool);
    // Out of line in RB3 retail: UILabel::SetTokenFmtImp calls it (0x827F2D78).
    const char *RawFmt() const;
};
