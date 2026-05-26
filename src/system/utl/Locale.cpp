#include "utl/Locale.h"

#include "DataPointMgr.h"
#include "obj/DataFile.h"
#include "obj/DataFunc.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/Str.h"
#include "utl/DataPointMgr.h"
#include "xdk/xbdm/xbdm.h"
#include <vector>

Locale TheLocale;

// ~Locale() is inline in Locale.h (compiler inlines it into atexit destructor)
bool gShowTokensCheat = false;
bool Locale::sVerboseNotify;
const char *Locale::sIgnoreMissingText;

DataNode DataSetLocaleVerboseNotify(DataArray *arr) {
    Locale::SetLocaleVerboseNotify(arr->Int(1));
    return DataNode(0);
}

DataNode DataToggleShowTokensCheat(DataArray *arr) {
    gShowTokensCheat = !gShowTokensCheat;
    return DataNode(0);
}

static int LocaleChunkSortFunc(const void *a, const void *b) {
    const LocaleChunkSort::OrderedLocaleChunk *chunkA =
        (const LocaleChunkSort::OrderedLocaleChunk *)a;
    const LocaleChunkSort::OrderedLocaleChunk *chunkB =
        (const LocaleChunkSort::OrderedLocaleChunk *)b;
    Symbol symA = chunkA->node1.LiteralSym(0);
    Symbol symB = chunkB->node1.LiteralSym(0);
    if (symA < symB)
        return -1;
    if (symA > symB)
        return 1;
    return chunkA->node2.Int(0) - chunkB->node2.Int(0);
}

void LocaleChunkSort::Sort(OrderedLocaleChunk *chunks, int count) {
    qsort(chunks, count, sizeof(OrderedLocaleChunk), LocaleChunkSortFunc);
}

namespace LocaleChunkSort {
template <int N>
int FastSort(const void *a, const void *b) {
    int offset = (int)a - (int)b;
    int i = 0;
    do {
        int valA = *(int *)((char *)b + offset);
        int valB = *(int *)b;
        if (valA < valB)
            return -1;
        if (valA > valB)
            return 1;
        i++;
        b = (const char *)b + 8;
    } while (i < N);
    return 0;
}

template int FastSort<3>(const void *, const void *);
}

const char *Locale::Localize(Symbol token, bool success) const {
    if (token.Null()) {
        return "";
    }
    if (!mSymTable) {
        MILO_ASSERT(mSymTable, 0x1D8);
    }

    static Symbol sEng("eng");

    if (mMagnuStrings != nullptr) {
        if (SystemLanguage() == sEng) {
            DataArray *langArray = mMagnuStrings->FindArray(token, false);
            if (langArray) {
                return langArray->Str(1);
            }
        }
    }

    int idx;
    if (FindDataIndex(token, idx, success)) {
        return mStrTable[idx];
    }

    if (UsingCD()) {
        SendDebugDataPoint("debug/locale/token", "token", token, "success", false);
    }

    return nullptr;
}

bool Locale::FindDataIndex(Symbol s, int &idx, bool fail) const {
    int low = 0;
    int high = mSize - 1;
    while (high - low >= 0) {
        int mid = (low + high) >> 1;
#ifdef HX_NATIVE
        // On LP64, (int)Symbol truncates 8-byte pointer to 4 bytes.
        // Compare interned string pointers directly (sorted by address).
        if (s.Str() > mSymTable[mid].Str()) {
            low = mid + 1;
        } else if (s.Str() < mSymTable[mid].Str()) {
#else
        if ((int)s > (int)mSymTable[mid]) {
            low = mid + 1;
        } else if ((int)s < (int)mSymTable[mid]) {
#endif
            high = mid - 1;
        } else {
            idx = mid;
            return true;
        }
    }
    if (fail) {
        MILO_FAIL("Couldn't find '%s' in array (file %s)", s.Str(), mFile.Str());
    }
    return false;
}

void Locale::Terminate() {
    delete[] mSymTable;
    mSymTable = 0;
    delete[] mStrTable;
    mStrTable = 0;
    delete[] mUploadedFlags;
    mUploadedFlags = 0;
    RELEASE(mStringData);
    mSize = 0;
    mFile = Symbol();
    mNumFilesLoaded = 0;
}

void Locale::SetMagnuStrings(DataArray *da) {
    if (mMagnuStrings) {
        mMagnuStrings->Release();
        mMagnuStrings = 0;
    }
    mMagnuStrings = da;
}

void Locale::Init() {
    MILO_ASSERT(!mStrTable, 0x58);
    MILO_ASSERT(!mSymTable, 0x59);
    MILO_ASSERT(!mSize, 0x5A);
    MILO_ASSERT(!mStringData, 0x5B);
    mSize = 0;

    MILO_ASSERT(!mNumFilesLoaded, 0x5C);
    int totalStrLen = 0;  // Total length of all unique localized strings
    int numChunks = 0;     // Number of locale entries loaded from files
    LocaleChunkSort::OrderedLocaleChunk *chunks = 0;
    Symbol prevSym;        // Tracks previous symbol to deduplicate

    // Check for alternate devkit locale file
    String devkitPath(FileMakePath(
        "devkit:\\locale", MakeString("%s\\locale_keep.dta", SystemLanguage())
    ));
    FileQualifiedFilename(devkitPath, devkitPath.c_str());

    static Symbol locale("locale");
    DataArrayPtr altCfg((DataNode(devkitPath)), DataNode(locale));

    DataArray *cfg = SystemConfig();
    if (!cfg) {
        goto done;
    }

    cfg = SystemConfig("locale");

    if (DmMapDevkitDrive() >= 0) {
        if (FileExists(devkitPath.c_str(), 0, 0)) {
            MILO_NOTIFY("Using alternate locale file from%s", devkitPath);
            cfg = (DataArray *)altCfg;
        }
    }

    MemPushTemp();
    {
        std::vector<DataArray *> arrVec(cfg->Size() - 1);
        mNumFilesLoaded = arrVec.size();

        int totalChunks = 0;
        // NOTE: mInitialized is uninitialized here (UB). RB3 doesn't have this check.
        // This appears to be dead code or a bug, but matches the original binary.
        if (mInitialized) {
            auto _tmp10 = cfg->Size();
            for (int i = 1; i < _tmp10; i++) {
                const char *path = FileMakePath(FileGetPath(cfg->File()), cfg->Str(i));
                arrVec[i - 1] = DataReadFile(path, true);
                if (!arrVec[i - 1]) {
                    MILO_FAIL("could not load language file %s", path);
                }
                totalChunks += arrVec[i - 1]->Size();
            }

            chunks = new LocaleChunkSort::OrderedLocaleChunk[totalChunks];

            numChunks = 0;
            for (int j = cfg->Size() - 2; j >= 0; j--) {
                DataArray *curArr = arrVec[j];
                for (int k = curArr->Size() - 1; k >= 0; k--, numChunks++) {
                    DataArray *chunkArr = curArr->Node(k).LiteralArray(curArr);
                    int size = chunkArr->Size();
                    if (size < 2) {
                        MILO_FAIL(
                            "%s line %d should have 2 entries, has %d, mismatched quotes?",
                            chunkArr->File(),
                            chunkArr->Line(),
                            size
                        );
                    }
                    chunks[numChunks].node1 = chunkArr->LiteralSym(0);
                    chunks[numChunks].node2 = numChunks;
                    chunks[numChunks].node3 = chunkArr->LiteralStr(1);
                }
                curArr->Release();
            }
        }

        if (cfg->Size() > 1) {
            LocaleChunkSort::Sort(chunks, numChunks);
        }

        mSize = 0;
        for (int i = 0; i < numChunks; i++) {
            Symbol curSym = chunks[i].node1.LiteralSym();
            if (curSym != prevSym) {
                totalStrLen += strlen(chunks[i].node3.LiteralStr());
                prevSym = curSym;
                mSize++;
            }
        }
    }

    mSymTable = new Symbol[mSize];
    mStringData = new StringTable(totalStrLen);
    mStrTable = new const char *[mSize];
    mUploadedFlags = new bool[mSize];

    prevSym = Symbol();

    if ((unsigned int)numChunks > 0) {
        int chunkIdx = 0;
        for (int i = 0; i < numChunks; i++) {
            Symbol curSym = chunks[i].node1.LiteralSym();
            if (curSym != prevSym) {
                mUploadedFlags[chunkIdx] = 0;
                mSymTable[chunkIdx] = curSym;
                mStrTable[chunkIdx] = mStringData->Add(chunks[i].node3.LiteralStr());
                prevSym = curSym;
                chunkIdx++;
            } else
                MILO_WARN("Locale symbol '%s' redefined\n", curSym);
        }
    }

    delete[] chunks;
    MemPopTemp();

done:
    if (cfg && cfg->Size() > 1) {
        mFile = cfg->Str(1);
    }

    DataRegisterFunc("set_locale_verbose_notify", DataSetLocaleVerboseNotify);
    DataRegisterFunc("toggle_show_tokens_cheat", DataToggleShowTokensCheat);
}

// Static buffers and index
struct LocaleFloatState {
    char buffers[4][0x32];
    int idx;
};
static LocaleFloatState gLocalizeFloat = {};
#define gLocalizeFloatBuf gLocalizeFloat
#define gLocalizeFloatIdx gLocalizeFloat.idx

const char *LocalizeFloat(const char *fmt, float num) {
    const char *str = MakeString(fmt, num);

    static Symbol sDecimalSep("locale_decimal_separator");

    const char *decimalStr = TheLocale.Localize(sDecimalSep, false);

    // If the localized decimal separator is not '.', we need to replace it
    if (decimalStr != 0 && *decimalStr != '.') {
        char *buf = gLocalizeFloatBuf.buffers[gLocalizeFloatIdx];
        strncpy(buf, str, 0x32);
        buf[0x31] = '\0';

        // Find and replace '.' with localized separator
        char *p = buf;
        for (;;) {
            if (*p == 0) break;
            if (*p == '.') {
                *p = *decimalStr;
                break;
            }
            p++;
        }

        // Increment index with modulo 4 wrapping
        gLocalizeFloatIdx = (gLocalizeFloatIdx + 1) % 4;

        return buf;
    }

    return str;
}

static char gLocalizeSepBuf[4][0x32];
static int gLocalizeSepIdx = 0;

const char *LocalizeSeparatedInt(int num, Locale &locale) {
    static Symbol sSep("locale_separator");
    bool success = false;
    char digitBuf[2];
    const char *sep = Localize(sSep, &success, locale);
    if (!success) {
        sep = ",";
    }
    if (strcmp(sep, gNullStr) == 0) {
        return (char *)MakeString("%i", num);
    }
    int sepLen = strlen(sep);
    char *buf = gLocalizeSepBuf[gLocalizeSepIdx];
    buf[0x31] = '\0';
    int pos = 0x31;
    int absNum = num;
    bool negative = num < 0;
    if (negative) {
        absNum = (num ^ (num >> 31)) - (num >> 31);
    }
    int digitCount = 0;
    while (true) {
        if (digitCount != 0 && absNum <= 0)
            break;
        if (digitCount % 3 == 0 && digitCount > 0) {
            for (int j = sepLen - 1; j >= 0; j--) {
                pos--;
                buf[pos] = sep[j];
            }
        }
        Hx_snprintf(digitBuf, 2, "%d", absNum % 10);
        pos--;
        buf[pos] = digitBuf[0];
        digitCount++;
        absNum = absNum / 10;
    }
    if (negative) {
        pos--;
        buf[pos] = '-';
    }
    char *result = &buf[pos];
    gLocalizeSepIdx = (gLocalizeSepIdx + 1) % 4;
    return result;
}

void SyncReloadLocale() {
    static Symbol sLocale("locale");
    DataArray *cfg = SystemConfig(sLocale);
    for (int i = 1; i < cfg->Size(); i++) {
        const char *str = cfg->Str(i);
        char *path =
            (char *)FileMakePath(FileGetPath(cfg->File()), str);
        if (SystemExec(MakeString("p4 sync %s", path)) == 0) {
            TheDebug << MakeString("updated %s\n", path);
        } else {
            TheDebug << MakeString("failed to update %s\n", path);
        }
    }
    TheLocale.Terminate();
    TheLocale.Init();
}

const char *Localize(Symbol token, bool *success, Locale &locale) {
    if (gShowTokensCheat) {
        if (success)
            *success = true;
        return token.Str();
    }
    const char *textStr = locale.Localize(token, false);
    bool localized = textStr != 0;
    if (!localized) {
        textStr = token.Str();
        Locale::sIgnoreMissingText = textStr;
        if (Locale::sVerboseNotify) {
            MILO_NOTIFY("\"%s\" needs localization", token);
        }
    }
    if (success) {
        *success = localized;
    }
    return textStr;
}
