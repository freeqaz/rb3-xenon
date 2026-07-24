// rb3-xenon native — M4 profile/save driver.
//
// Runs RB3's *real* FixedSizeSaveable serialization machinery natively and
// round-trips it: construct saveables with non-default state, serialize them
// through the genuine engine path, destroy, reload into fresh instances, and
// verify field-by-field.  No reimplementation — every serialization primitive
// (FixedSizeSaveableStream, operator<< / operator>>, the SaveSize contract,
// SaveFixedSymbol/LoadFixedSymbol, the hash_map<Symbol,int> symbol-ID table,
// SaveSymbolTable/LoadSymbolTable, PadStream/DepadStream, stream encryption)
// is stock rb3-xenon engine code out of src/system/meta + src/system/utl.
//
// The four round-trips escalate in machinery coverage:
//   RT1  GameplayOptions          — a real FixedSizeSaveable subclass; plain
//                                    operator<< / operator>> + SaveSize contract.
//   RT2  StandIn                  — SaveFixedSymbol + HxGuid (exercises the
//                                    symbol-pad + guid-rev serialization).
//   RT3  memcard envelope + set   — the full SaveLoadManager/HamMemcardAction
//                                    flow: versioned header, InitializeTable,
//                                    real FixedSizeSaveable::SaveStd(set<Symbol>)
//                                    (populates the hash_map symbol-ID table via
//                                    SaveSymbolID/AddSymbol), SaveTable → reload
//                                    LoadTable + LoadStd.  This is the exact
//                                    machinery a BandProfile's campaignKeys /
//                                    modifiers / lessonCompletions ride through.
//   RT4  memcard envelope + crypt — GameplayOptions wrapped in the versioned +
//                                    table + *encrypted* envelope (EnableWrite/
//                                    ReadEncryption), i.e. how the profile score
//                                    block is stored on card.
//
// Xbox XContent/STFS packaging is intentionally out of scope: we serialize to a
// plain heap buffer (and dump it to disk) rather than a device container.

#include "meta/FixedSizeSaveable.h"
#include "meta/FixedSizeSaveableStream.h"
#include "meta_band/GameplayOptions.h"
#include "meta_band/StandIn.h"
#include "utl/Symbol.h"
#include "utl/HxGuid.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <set>
#include <vector>

extern void InitMakeString(); // src/system/utl/Str.cpp
void DataInit();              // src/system/obj/Data.cpp

// ---------------------------------------------------------------------------
// LoadMemcardAction — FixedSizeSaveable friends this exact class name (the
// "// hack" friend decl) so the load path can set the private sCurrentMemcardLoadVer
// static from the version int read out of the save header.  This mirrors the
// real RB3 LoadMemcardAction (SaveLoadManager.cpp) / DC3 HamMemcardAction; here
// it is the minimal friend needed to drive the flow against a plain buffer.
// ---------------------------------------------------------------------------
class LoadMemcardAction {
public:
    static void SetLoadVer(int v) { FixedSizeSaveable::sCurrentMemcardLoadVer = v; }
};

static const int kSaveVer = 92;   // RB3/DC3 memcard save version
static const int kMaxSymbols = 64; // symbol-table capacity for the envelope

static int gFail = 0;
#define CHECK(cond, msg)                                                                 \
    do {                                                                                 \
        bool _c = (cond);                                                                \
        printf("    [%s] %s\n", _c ? "PASS" : "FAIL", msg);                              \
        if (!_c)                                                                         \
            gFail++;                                                                     \
    } while (0)

static void hexdump(const char *label, const unsigned char *p, int n, int cap) {
    printf("  %s (%d bytes", label, n);
    if (n > cap)
        printf(", first %d shown", cap);
    printf("):\n");
    int show = n < cap ? n : cap;
    for (int i = 0; i < show; i += 16) {
        printf("    %04x  ", i);
        for (int j = 0; j < 16; j++) {
            if (i + j < show)
                printf("%02x ", p[i + j]);
            else
                printf("   ");
        }
        printf(" |");
        for (int j = 0; j < 16 && i + j < show; j++) {
            unsigned char c = p[i + j];
            printf("%c", (c >= 0x20 && c < 0x7f) ? c : '.');
        }
        printf("|\n");
    }
}

// =====================================================================  RT1
static void rt1_gameplay_options() {
    printf("\n--- RT1: GameplayOptions (plain operator<< / operator>> + SaveSize) ---\n");
    int savesize = GameplayOptions::SaveSize(kSaveVer);
    printf("  SaveSize(%d) = %d bytes\n", kSaveVer, savesize);

    GameplayOptions src;
    // Non-default state (avoid SetVocalVolume -> TheProfileMgr; poke the field).
    src.SetLefty(true);
    src.SetVocalStyle(kVocalStyleStatic);
    src.mVocalVolume = 3;
    printf("  saved:    lefty=%d vocalVolume=%d vocalStyle=%d\n", src.GetLefty(),
           src.GetVocalVolume(0), (int)src.GetVocalStyle());

    std::vector<unsigned char> buf(savesize + 16, 0xAB);
    {
        FixedSizeSaveableStream fsss(&buf[0], buf.size(), true);
        fsss << src; // operator<< : SaveFixed + "Bad save file size" contract
        printf("  wrote %d bytes (contract satisfied — no MILO_FAIL)\n", fsss.Tell());
        hexdump("payload", &buf[0], savesize, 64);
    }

    GameplayOptions dst;
    dst.mVocalVolume = -999; // clobber to prove reload overwrites
    LoadMemcardAction::SetLoadVer(kSaveVer);
    {
        FixedSizeSaveableStream fsss(&buf[0], buf.size(), true);
        fsss >> dst; // operator>> : LoadFixed + contract
    }
    printf("  reloaded: lefty=%d vocalVolume=%d vocalStyle=%d\n", dst.GetLefty(),
           dst.GetVocalVolume(0), (int)dst.GetVocalStyle());
    CHECK(dst.GetLefty() == src.GetLefty(), "lefty round-trips");
    CHECK(dst.GetVocalVolume(0) == src.mVocalVolume, "vocalVolume round-trips");
    CHECK(dst.GetVocalStyle() == src.GetVocalStyle(), "vocalStyle round-trips");
}

// =====================================================================  RT2
static void rt2_standin() {
    printf("\n--- RT2: StandIn (SaveFixedSymbol + HxGuid serialization) ---\n");
    int savesize = StandIn::SaveSize(kSaveVer);
    printf("  SaveSize(%d) = %d bytes (kSymbolSize 0x32 + HxGuid::SaveSize 0x14)\n",
           kSaveVer, savesize);

    StandIn src;
    src.SetName("cortez_prefab"); // prefab-character variant (name set, guid null)
    printf("  saved:    name='%s' isPrefab=%d isNone=%d\n", src.GetName().Str(),
           src.IsPrefabCharacter(), src.IsNone());

    std::vector<unsigned char> buf(savesize + 16, 0xCD);
    {
        FixedSizeSaveableStream fsss(&buf[0], buf.size(), true);
        fsss << src;
        printf("  wrote %d bytes\n", fsss.Tell());
        hexdump("payload", &buf[0], savesize, 80);
    }

    StandIn dst;
    dst.SetName("garbage");
    LoadMemcardAction::SetLoadVer(kSaveVer);
    {
        FixedSizeSaveableStream fsss(&buf[0], buf.size(), true);
        fsss >> dst;
    }
    printf("  reloaded: name='%s' isPrefab=%d\n", dst.GetName().Str(),
           dst.IsPrefabCharacter());
    CHECK(dst.GetName() == src.GetName(), "StandIn name round-trips");
    CHECK(dst.IsPrefabCharacter() == src.IsPrefabCharacter(), "prefab flag round-trips");
    CHECK(dst.GetGuid() == src.GetGuid(), "guid round-trips");
}

// =====================================================================  RT3
// The full SaveLoadManager / HamMemcardAction envelope around a real
// FixedSizeSaveable::SaveStd(set<Symbol>) — the code path that stores a
// BandProfile's campaign keys / unlocked modifiers.  Exercises the symbol-ID
// table end to end: SaveSymbolID -> AddSymbol -> hash_map<Symbol,int> ->
// SaveTable -> LoadTable -> hash_map<int,Symbol> -> GetSymbol.
static void rt3_symbol_table_envelope() {
    printf("\n--- RT3: memcard envelope + FixedSizeSaveable::SaveStd(set<Symbol>) ---\n");
    printf("        [exercises the hash_map<Symbol,int> symbol-ID table + SaveTable]\n");
    const int kMaxKeys = 20;

    std::set<Symbol> keys;
    keys.insert(Symbol("campaign_rock_show"));
    keys.insert(Symbol("campaign_arena"));
    keys.insert(Symbol("campaign_stadium"));
    keys.insert(Symbol("modifier_double_notes"));
    printf("  saved %d campaign keys:", (int)keys.size());
    for (std::set<Symbol>::iterator it = keys.begin(); it != keys.end(); ++it)
        printf(" '%s'", it->Str());
    printf("\n");

    int tableRegion = FixedSizeSaveableStream::GetSymbolTableSize(kSaveVer);
    int bufLen = 4 /*ver*/ + tableRegion + (4 + kMaxKeys * 4) + 64;
    std::vector<unsigned char> buf(bufLen, 0);
    printf("  buffer=%d bytes (ver 4 + symtable region %d + set payload)\n", bufLen,
           tableRegion);

    // ---- SAVE (mirrors SaveMemcardAction::PreAction) ----
    int wrote = 0;
    {
        FixedSizeSaveableStream fsss(&buf[0], bufLen, true);
        int ver = kSaveVer;
        fsss << ver;
        fsss.InitializeTable(); // reserves the symbol-table region at m_iTableOffset
        FixedSizeSaveable::SaveStd(fsss, keys, kMaxKeys); // real engine; fills the ID table
        fsss.SaveTable(); // back-patch the now-populated table into its region
        wrote = fsss.Tell();
        printf("  wrote %d bytes; symbol-ID table populated during save\n", wrote);
    }
    hexdump("header+table", &buf[0], 4 + tableRegion, 96);

    // ---- LOAD (mirrors LoadMemcardAction::PostAction) ----
    std::set<Symbol> out;
    {
        FixedSizeSaveableStream fsss(&buf[0], bufLen, true);
        int ver = 0;
        fsss >> ver;
        LoadMemcardAction::SetLoadVer(ver); // the friend hack, from the header int
        printf("  read version=%d; sCurrentMemcardLoadVer set\n", ver);
        fsss.LoadTable(ver); // rebuild hash_map<int,Symbol> from the stored table
        FixedSizeSaveable::LoadStd(fsss, out, kMaxKeys); // IDs -> GetSymbol -> set
    }
    printf("  reloaded %d keys:", (int)out.size());
    for (std::set<Symbol>::iterator it = out.begin(); it != out.end(); ++it)
        printf(" '%s'", it->Str());
    printf("\n");
    CHECK(out.size() == keys.size(), "campaign-key set size round-trips");
    CHECK(out == keys, "campaign-key set contents round-trip (symbol-ID table)");

    // Dump the whole envelope for external inspection.
    FILE *f = fopen("/tmp/rb3_save_rt3.bin", "wb");
    if (f) {
        fwrite(&buf[0], 1, wrote, f);
        fclose(f);
        printf("  wrote envelope to /tmp/rb3_save_rt3.bin (%d bytes)\n", wrote);
    }
}

// =====================================================================  RT4
// GameplayOptions through the full versioned + table + *encrypted* envelope —
// the shape of the on-card profile score block (SaveFixed under
// EnableWriteEncryption, per BandProfile::SaveFixed's mScores handling).
static void rt4_encrypted_envelope() {
    printf("\n--- RT4: memcard envelope + stream encryption (Rand2 keystream) ---\n");
    int tableRegion = FixedSizeSaveableStream::GetSymbolTableSize(kSaveVer);
    int bufLen = 4 + tableRegion + 4 /*crypto seed*/ + GameplayOptions::SaveSize(kSaveVer) + 64;
    std::vector<unsigned char> buf(bufLen, 0);

    GameplayOptions src;
    src.SetLefty(true);
    src.SetVocalStyle(kVocalStyleInvalid);
    src.mVocalVolume = 9;

    int wrote = 0;
    {
        FixedSizeSaveableStream fsss(&buf[0], bufLen, true);
        int ver = kSaveVer;
        fsss << ver;
        fsss.InitializeTable();
        fsss.EnableWriteEncryption(); // writes a random seed, then XORs the payload
        fsss << src;
        fsss.DisableEncryption();
        fsss.SaveTable();
        wrote = fsss.Tell();
        printf("  wrote %d bytes (payload encrypted, table+header plaintext)\n", wrote);
    }
    // Prove the payload is actually enciphered: the plaintext lefty/volume bytes
    // must NOT appear verbatim right after the seed.
    hexdump("encrypted tail", &buf[4 + tableRegion], wrote - (4 + tableRegion), 48);

    GameplayOptions dst;
    dst.mVocalVolume = -1;
    {
        FixedSizeSaveableStream fsss(&buf[0], bufLen, true);
        int ver = 0;
        fsss >> ver;
        LoadMemcardAction::SetLoadVer(ver);
        fsss.LoadTable(ver);
        fsss.EnableReadEncryption(); // reads the seed, reproduces the keystream
        fsss >> dst;
        fsss.DisableEncryption();
    }
    printf("  reloaded: lefty=%d vocalVolume=%d vocalStyle=%d\n", dst.GetLefty(),
           dst.GetVocalVolume(0), (int)dst.GetVocalStyle());
    CHECK(dst.GetLefty() == src.GetLefty(), "encrypted lefty round-trips");
    CHECK(dst.GetVocalVolume(0) == src.mVocalVolume, "encrypted vocalVolume round-trips");
    CHECK(dst.GetVocalStyle() == src.GetVocalStyle(), "encrypted vocalStyle round-trips");
}

int main(int argc, char **argv) {
    InitMakeString();
    Symbol::Init();
    DataInit();

    // Publish the global save version + symbol-table capacity, exactly as
    // SaveLoadManager::Init does before any profile serialization.
    FixedSizeSaveable::Init(kSaveVer, kMaxSymbols);

    printf("=== rb3-xenon native M4: profile/save serialization round-trip ===\n");
    printf("save version = %d, max symbols = %d\n", kSaveVer, kMaxSymbols);

    rt1_gameplay_options();
    rt2_standin();
    rt3_symbol_table_envelope();
    rt4_encrypted_envelope();

    printf("\n=== %s (%d check failure%s) ===\n", gFail == 0 ? "ALL ROUND-TRIPS OK" : "FAILURES",
           gFail, gFail == 1 ? "" : "s");
    return gFail == 0 ? 0 : 1;
}
