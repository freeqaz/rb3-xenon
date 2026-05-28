#pragma once
#include "utl/Symbol.h"
#include "utl/StringTable.h"
#include "obj/Data.h"

enum LocaleGender {
    LocaleGenderMasculine = 0,
    LocaleGenderFeminine = 1,
};

enum LocaleNumber {
    LocaleSingular = 0,
    LocalePlural = 1,
};

namespace LocaleChunkSort {
    struct OrderedLocaleChunk {
        OrderedLocaleChunk() : node1(0), node2(0), node3(0) {}
        DataNode node1;
        DataNode node2;
        DataNode node3;

        MEM_ARRAY_OVERLOAD(OrderedLocaleChunk, 0x1d)
    };

    void Sort(OrderedLocaleChunk *, int);

    template <int N>
    int FastSort(const void *a, const void *b);
}

class Locale {
private:
    int mSize; // 0x0
    Symbol *mSymTable; // 0x4
    const char **mStrTable; // 0x8
    StringTable *mStringData; // 0xc
    bool *mUploadedFlags; // 0x10
    Symbol mFile; // 0x14
    int mNumFilesLoaded; // 0x18
    bool mInitialized; // 0x1c - checked in Init
    DataArray *mMagnuStrings; // 0x20
public:
#ifdef HX_NATIVE
    // Native builds need explicit init since globals aren't BSS-zeroed
    Locale() : mSize(0), mSymTable(0), mStrTable(0), mStringData(0),
        mUploadedFlags(0), mNumFilesLoaded(0), mInitialized(true), mMagnuStrings(0) {}
#else
    // PPC: BSS zeroes all members. Only Symbol mFile needs construction (sets gNullStr).
    // mInitialized is UB in original binary — never written, happens to be nonzero.
    Locale() {}
#endif
    ~Locale() {
        if (mMagnuStrings) {
            mMagnuStrings->Release();
            mMagnuStrings = 0;
        }
    }

    void Init();
    void Terminate();

    static const char *sIgnoreMissingText;

    void SetMagnuStrings(DataArray *);
    const char *Localize(Symbol, bool) const;

    static void SetLocaleVerboseNotify(bool set) { Locale::sVerboseNotify = set; }
    static bool GetLocaleVerboseNotify() { return sVerboseNotify; }

    static bool sVerboseNotify;

protected:
    bool FindDataIndex(Symbol, int &, bool) const;
};

extern Locale TheLocale;
extern bool gShowTokensCheat;

const char *Localize(Symbol token, bool *success, Locale &locale);
// RB3 2-argument form (no Locale& parameter) — calls through to TheLocale.
// Matches rb3-Wii Locale.h: const char *Localize(Symbol, bool *);
inline const char *Localize(Symbol token, bool *success) {
    return Localize(token, success, TheLocale);
}
const char *LocalizeSeparatedInt(int num, Locale &locale);
const char *LocalizeFloat(const char *fmt, float num);
void SyncReloadLocale();
