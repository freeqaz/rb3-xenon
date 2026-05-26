#include "utl/SuperFormatString.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/Locale.h"
#include "utl/LocaleOrdinal.h"
#include "obj/Data.h"
#include <string.h>

#define BUF_SIZE 0x800

SuperFormatString::SuperFormatString(
    const char *cc, const DataArray *da, bool b, Locale &locale, Symbol lang
) {
    char param[8];
    char tempFmt[2048];
    char phInfo[64];
    mTokensOnly = b;
    mHasPercentFormat = false;
    if (!da && !b) {
        InitializeWithFmt(cc, true);
        return;
    } else {
        int phType = 0;
        int state = 0;
        char *paramPos = param;
        char *phInfoPos = phInfo;
        char *tempFmtEnd = tempFmt + 2048;
        char *tempFmtPos = tempFmt;
        bool sawPercent = false;
        bool sawDouble = false;
        for (const char *p = cc; *p != 0; p++) {
            switch (state) {
            case 0:
                if (*p == '{') {
                    if (p[1] != '{') {
                        state = 1;
                    } else {
                        *tempFmtPos++ = '{';
                        p++;
                    }
                } else {
                    if (*p == '%' && !sawPercent) {
                        if (p[1] == '%' && !sawDouble) {
                            sawPercent = true;
                            mHasPercentFormat = true;
                        } else {
                            sawDouble = true;
                            mHasPercentFormat = false;
                        }
                    } else {
                        sawPercent = false;
                    }
                    *tempFmtPos++ = *p;
                }
                break;
            case 1:
                if (*p == ':') {
                    MILO_ASSERT(phInfoPos - phInfo < 64, 0x5A);
                    *phInfoPos = '\0';
                    phInfoPos = phInfo;
                    state = 3;
                    auto _tmp0 = strcmp(phInfoPos, "string");
                    bool phInfoCmp = _tmp0 == 0;
                    if (phInfoCmp) {
                        phType = 0;
                        continue;
                    }
                    phInfoCmp = strcmp(phInfoPos, "int") == 0;
                    if (phInfoCmp) {
                        *paramPos++ = '%';
                        state = 2;
                        phType = 1;
                        continue;
                    }
                    phInfoCmp = strcmp(phInfoPos, "sep_int") == 0;
                    if (phInfoCmp) {
                        phType = 2;
                        continue;
                    }
                    phInfoCmp = strcmp(phInfoPos, "float") == 0;
                    if (phInfoCmp) {
                        *paramPos++ = '%';
                        phType = 3;
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
                        phType = 5;
                        state = 2;
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
                            if (node.Type() != kDataString) {
                                nodeBad = node.Type() != kDataSymbol;
                            }
                            break;
                        case 1:
                            nodeBad = node.Type() != kDataInt;
                            break;
                        case 2:
                            nodeBad = node.Type() != kDataInt;
                            break;
                        case 3:
                            if (node.Type() != kDataFloat) {
                                nodeBad = node.Type() != kDataInt;
                            }
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
                            int x;
                            switch (phType) {
                            case 0:
                                if (node.Type() == kDataString) {
                                    snResult = Hx_snprintf(
                                        tempFmtPos,
                                        tempFmtEnd - tempFmtPos,
                                        "%s",
                                        node.Str()
                                    );
                                } else {
                                    snResult = Hx_snprintf(
                                        tempFmtPos,
                                        tempFmtEnd - tempFmtPos,
                                        "%s",
                                        Localize(node.Sym(), 0, locale)
                                    );
                                }
                                break;
                            case 1:
                                snResult = Hx_snprintf(
                                    tempFmtPos, tempFmtEnd - tempFmtPos, param, node.Int()
                                );
                                break;
                            case 2:
                                snResult = Hx_snprintf(
                                    tempFmtPos,
                                    tempFmtEnd - tempFmtPos,
                                    "%s",
                                    LocalizeSeparatedInt(node.Int(), locale)
                                );
                                break;
                            case 3:
                                snResult = Hx_snprintf(
                                    tempFmtPos,
                                    tempFmtEnd - tempFmtPos,
                                    param,
                                    node.Float()
                                );
                                break;
                            case 4:
                                snResult = Hx_snprintf(
                                    tempFmtPos,
                                    tempFmtEnd - tempFmtPos,
                                    "%s",
                                    Localize(Symbol(phInfo), 0, locale)
                                );
                                break;
                            case 5:
                                gender = (LocaleGender)(param[0] != 'm');
                                num = (LocaleNumber)(param[1] != 's');
                                x = node.Int();
                                snResult = Hx_snprintf(
                                    tempFmtPos,
                                    tempFmtEnd - tempFmtPos,
                                    "%s",
                                    LocalizeOrdinal(x, gender, num, false, lang, locale)
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
                    tempFmtPos += Hx_snprintf(
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
                Hx_snprintf(tempFmtPos, tempFmtEnd - tempFmtPos, "{badfmt:%s", phInfo);
        }
        *tempFmtPos = 0;
        MILO_ASSERT(tempFmtPos - tempFmt < BUF_SIZE, 0x10B);
        InitializeWithFmt(tempFmt, b == 0);
    }
}

const char *SuperFormatString::FinalStr() {
    if (!(!mTokensOnly)) {
        return mFmt;
    }
    const char *result = Str();
    if (!mHasPercentFormat) {
        return result;
    }
    String str(result);
    str += "%s";
    return MakeString(str.c_str(), "");
}
