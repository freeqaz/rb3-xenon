#include "utl/SuperFormatString.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/Locale.h"
#include "utl/LocaleOrdinal.h"
#include "obj/Data.h"
#include <stdio.h>
#include <string.h>

// RB3 retail 0x82BBABF8 calls the CRT _snprintf directly (no Hx_snprintf
// wrapper, so a truncated write is not re-terminated); native keeps the wrapper.
#ifdef HX_NATIVE
#define SFS_SNPRINTF Hx_snprintf
#else
#define SFS_SNPRINTF _snprintf
#endif

#define BUF_SIZE 0x800

// RB3 retail 0x82BBABF8.  Placeholders are `{type:name}` (or
// `{type:width:name}` for int/float/ordinal), filled from the matching
// `(name value)` pair of `da`, or localised directly for `token`.
SuperFormatString::SuperFormatString(const char *cc, const DataArray *da, bool b) {
    char param[8];
    char tempFmt[2048];
    char phInfo[64];
    if (!da && !b) {
        InitializeWithFmt(cc, true);
        return;
    } else {
        char *tempFmtPos = tempFmt;
        char *tempFmtEnd = tempFmt + 2048;
        char *phInfoPos = phInfo;
        char *paramPos = param;
        int phType = 0;
        int state = 0;
        for (const char *p = cc; *p != 0; p++) {
            switch (state) {
            case 0:
                if (*p == '{') {
                    if (p[1] == '{') {
                        p++;
                        *tempFmtPos++ = *p;
                    } else {
                        state = 1;
                    }
                } else {
                    *tempFmtPos++ = *p;
                }
                break;
            case 1:
                if (*p == ':') {
                    MILO_ASSERT(phInfoPos - phInfo < 64, 0x5A);
                    *phInfoPos = '\0';
                    phInfoPos = phInfo;
                    state = 3;
                    bool phInfoCmp = strcmp(phInfoPos, "string") == 0;
                    if (phInfoCmp) {
                        phType = 0;
                        continue;
                    }
                    phInfoCmp = strcmp(phInfoPos, "int") == 0;
                    if (phInfoCmp) {
                        phType = 1;
                        *paramPos++ = '%';
                        state = 2;
                        continue;
                    }
                    phInfoCmp = strcmp(phInfoPos, "sep_int") == 0;
                    if (phInfoCmp) {
                        phType = 2;
                        continue;
                    }
                    phInfoCmp = strcmp(phInfoPos, "float") == 0;
                    if (phInfoCmp) {
                        phType = 3;
                        *paramPos++ = '%';
                        state = 2;
                        continue;
                    }
                    phInfoCmp = strcmp(phInfoPos, "token") == 0;
                    if (phInfoCmp) {
                        phType = 4;
                        continue;
                    }
                    phInfoCmp = strcmp(phInfoPos, "ordinal") == 0;
                    if (phInfoCmp) {
                        state = 2;
                        phType = 5;
                        continue;
                    }
                    MILO_FAIL("bad SuperFormatString placeholder type '%s'", phInfoPos);
                } else {
                    *phInfoPos++ = *p;
                }
                break;
            case 2:
                if (*p == ':') {
                    if (phType == 3) {
                        *paramPos++ = 'f';
                        *paramPos = '\0';
                    } else if (phType == 1) {
                        *paramPos++ = 'i';
                        *paramPos = '\0';
                    }
                    MILO_ASSERT(paramPos - param < 8, 0x8F);
                    if (phType == 5) {
                        MILO_ASSERT(param + 2 == paramPos, 0x95);
                    }
                    paramPos = param;
                    state = 3;
                } else {
                    *paramPos++ = *p;
                }
                break;
            case 3:
                if (*p == '}') {
                    MILO_ASSERT(phInfoPos - phInfo < 64, 0xA3);
                    *phInfoPos = '\0';
                    phInfoPos = phInfo;
                    state = 0;
                    DataArray *theArr = 0;
                    bool isToken = phType == 4;
                    if (!b && !isToken) {
                        theArr = da->FindArray(phInfoPos, false);
                    }
                    if (theArr || isToken) {
                        DataNode node((isToken) ? DataNode(0) : theArr->Evaluate(1));
                        bool nodeBad = false;
                        switch (phType) {
                        case 0:
                            nodeBad = node.Type() != kDataString
                                && node.Type() != kDataSymbol;
                            break;
                        case 1:
                            nodeBad = node.Type() != kDataInt;
                            break;
                        case 2:
                            nodeBad = node.Type() != kDataInt;
                            break;
                        case 3:
                            nodeBad = node.Type() != kDataFloat
                                && node.Type() != kDataInt;
                            break;
                        case 4:
                            nodeBad = false;
                            break;
                        case 5:
                            nodeBad = node.Type() != kDataInt;
                            break;
                        default:
                            break;
                        }

                        if (!nodeBad) {
                            int snResult = 0;
                            LocaleGender gender;
                            LocaleNumber num;
                            switch (phType) {
                            case 0:
                                if (node.Type() == kDataString) {
                                    snResult = SFS_SNPRINTF(
                                        tempFmtPos,
                                        tempFmtEnd - tempFmtPos,
                                        "%s",
                                        node.Str()
                                    );
                                } else {
                                    snResult = SFS_SNPRINTF(
                                        tempFmtPos,
                                        tempFmtEnd - tempFmtPos,
                                        "%s",
                                        Localize(node.Sym(), 0)
                                    );
                                }
                                break;
                            case 1:
                                snResult = SFS_SNPRINTF(
                                    tempFmtPos, tempFmtEnd - tempFmtPos, param, node.Int()
                                );
                                break;
                            case 2:
                                snResult = SFS_SNPRINTF(
                                    tempFmtPos,
                                    tempFmtEnd - tempFmtPos,
                                    "%s",
                                    LocalizeSeparatedInt(node.Int())
                                );
                                break;
                            case 3:
                                snResult = SFS_SNPRINTF(
                                    tempFmtPos,
                                    tempFmtEnd - tempFmtPos,
                                    param,
                                    node.Float()
                                );
                                break;
                            case 4:
                                snResult = SFS_SNPRINTF(
                                    tempFmtPos,
                                    tempFmtEnd - tempFmtPos,
                                    "%s",
                                    Localize(Symbol(phInfo), 0)
                                );
                                break;
                            case 5:
                                gender = (LocaleGender)(param[0] != 'm');
                                num = (LocaleNumber)(param[1] != 's');
                                snResult = SFS_SNPRINTF(
                                    tempFmtPos,
                                    tempFmtEnd - tempFmtPos,
                                    "%s",
                                    LocalizeOrdinal(
                                        node.Int(),
                                        gender,
                                        num,
                                        false
                                    )
                                );
                                break;
                            }

                            tempFmtPos += snResult;
                            continue;
                        }
                        MILO_WARN(
                            "parameter for placeholder '%s' was the wrong type\n", phInfo
                        );
                    } else {
                        MILO_WARN(
                            "couldn't find parameter for placeholder '%s'\n", phInfo
                        );
                    }
                    tempFmtPos += SFS_SNPRINTF(
                        tempFmtPos, tempFmtEnd - tempFmtPos, "{missing:%s}", phInfo
                    );
                } else {
                    *phInfoPos++ = *p;
                }
                break;
            default:
                break;
            }
        }

        if (state != 0) {
            *phInfoPos = '\0';
            MILO_WARN("bad formatting for placeholder '%s'\n", phInfo);
            tempFmtPos +=
                SFS_SNPRINTF(tempFmtPos, tempFmtEnd - tempFmtPos, "{badfmt:%s", phInfo);
        }
        *tempFmtPos = 0;
        MILO_ASSERT(tempFmtPos - tempFmt < BUF_SIZE, 0x10B);
        InitializeWithFmt(tempFmt, b == 0);
    }
}

const char *SuperFormatString::RawFmt() const { return mFmt; }
