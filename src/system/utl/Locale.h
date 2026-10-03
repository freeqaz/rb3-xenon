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
#ifdef HX_NATIVE
    bool mInitialized; // checked in Init
    DataArray *mMagnuStrings;
#endif
    // Retail TheLocale is 0x1C bytes (lbl_82E07138) and ends at
    // mNumFilesLoaded: no init flag and no Magnu-string table. Its static
    // initializer (0x82C40F08) constructs only mFile and registers an empty
    // destructor (0x82C4A0D0 is a bare blr).
public:
#ifdef HX_NATIVE
    // Native builds need explicit init since globals aren't BSS-zeroed
    Locale() : mSize(0), mSymTable(0), mStrTable(0), mStringData(0),
        mUploadedFlags(0), mNumFilesLoaded(0), mInitialized(true), mMagnuStrings(0) {}
    ~Locale() {
        if (mMagnuStrings) {
            mMagnuStrings->Release();
            mMagnuStrings = 0;
        }
    }
#else
    // PPC: BSS zeroes all members. Only Symbol mFile needs construction (sets gNullStr).
    Locale() {}
    ~Locale() {}
#endif

    void Init();
    void Terminate();

    static const char *sIgnoreMissingText;

#ifdef HX_NATIVE
    void SetMagnuStrings(DataArray *);
#endif
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
// RB3 2-argument form (no Locale& parameter) — real out-of-line function that
// uses TheLocale: const char *Localize(Symbol, bool *).
const char *Localize(Symbol token, bool *success);
const char *LocalizeSeparatedInt(int num, Locale &locale);
// RB3 1-argument form — real out-of-line function; the
// `locale` parameter of the 2-arg form above is unused in its body (dead
// param), so retail has a genuine no-locale-arg overload with its own
// definition, not an inline forward. Call sites that pass only `num` do NOT
// load &TheLocale into a register in target's disassembly — confirmed by
// tracing Instarank::UpdateRankLabel / UpdateString2Label / UpdateString1Label.
const char *LocalizeSeparatedInt(int num);
const char *LocalizeFloat(const char *fmt, float num);
void SyncReloadLocale();
