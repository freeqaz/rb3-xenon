#pragma once
#include "utl/MakeString.h"
#include "utl/Locale.h"
#include "utl/Symbol.h"
#include "obj/Data.h"

class SuperFormatString : public FormatString {
public:
    SuperFormatString(const char *, const DataArray *, bool, Locale &, Symbol);
    const char *FinalStr();

private:
    bool mTokensOnly; // 0x1014
    bool mHasPercentFormat; // 0x1015
};
