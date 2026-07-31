// rb3-xenon native — M1 song-manager loader, now fed from the REAL .ark.
//
// M14 (lane CD-3). Previously this driver took a loose songs.dta on the command
// line, and the only test data to hand was an RB1-era rbtp2 dta with no
// (song_id N) node — which RB3's load path REQUIRES, so the driver fabricated
// synthetic ids to get past SongMetadata's ctor. Mounting the archive removes
// that crutch: the real RB3 songs.dta carries real song_ids, so BandSongMgr is
// finally driven on genuine retail metadata.
//
// That crutch is now an INSTRUMENT rather than a workaround. Injection is
// conditional (only for song arrays that genuinely lack song_id) and the number
// injected is COUNTED and GATED at zero in archive mode. So if the archive ever
// silently degraded to test-shaped data, the gate fires instead of the driver
// quietly papering over it. A workaround you can measure is worth more than one
// you remove.
//
// The mount is the same three calls as rb3-midi/rb3-ark:
//   NativeSetDataDir(dir); SetUsingCD(true); NativeArchiveInit();
// ★ SetUsingCD(true) is load-bearing — os/File.cpp:646 gates ArkFile creation on
// it, so without it NewFile falls through to AsyncFile and the archive-relative
// path fails against the host filesystem, looking like a missing file rather
// than a misconfiguration. DataReadFile("songs/songs.dta") additionally exercises
// the archive's .dta -> gen/<base>.dtb redirect (CachedDataFile()).
//
// FALSIFIABILITY. Gates print individually and the exit code is derived from the
// failure count, never from absence of stderr. Two negative controls are executed
// and reported in the lane log rather than merely offered:
//   --expect <N>  with a wrong N        => song-count gate FAILS, rc=1.
//   --loose <a real but DIFFERENT dtb>  => real data, different count, gate FAILS,
//                                          rc=1. (The Wii songs.dtb is a genuine
//                                          file, not a synthetic mutation.)
//
// Usage:
//   rb3-song <dataDir> [maxSongs] [--expect <N>]
//   rb3-song --loose <songs.dta|dtb> [maxSongs] [--expect <N>]

#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "os/Archive.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/Symbol.h"

#include "ark_verify.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

extern void InitMakeString();
extern void InitM1Symbols();     // native/src/m1_symbols.cpp — interns Symbol globals
extern DataArray *gSystemConfig; // src/system/os/System.cpp
// native/src/platform/File_Native.cpp
extern void NativeSetDataDir(const char *dir);
// native/src/platform/System_Native.cpp
extern void NativeArchiveInit();

using arkverify::Gates;

// The BandSongMgr load loop calls SystemConfig(missing_song_data)->FindArray(
// songSym, false). Give it a minimal, valid config so that resolves to "no
// override" (null) instead of dereferencing a null gSystemConfig.
static DataArray *MakeMinimalSystemConfig() {
    DataArray *missing = new DataArray(1);
    missing->Node(0) = DataNode(Symbol("missing_song_data"));
    DataArray *cfg = new DataArray(1);
    cfg->Node(0) = DataNode(missing, kDataArray);
    return cfg;
}

static bool HasSongId(DataArray *song) {
    return song->FindArray(Symbol("song_id"), false) != nullptr;
}

// Append (song_id <id>) — only ever called for arrays that genuinely lack one.
static void InjectSongId(DataArray *song, int id) {
    DataArray *idArr = new DataArray(2);
    idArr->Node(0) = DataNode(Symbol("song_id"));
    idArr->Node(1) = DataNode(id);
    song->Insert(song->Size(), DataNode(idArr, kDataArray));
}

int main(int argc, char **argv) {
    const char *dataDir = nullptr;
    const char *loosePath = nullptr;
    int maxSongs = 20;
    int expectSongs = -1; // -1 => default per mode

    const char *positional[4] = { nullptr, nullptr, nullptr, nullptr };
    int nPos = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--loose") == 0 && i + 1 < argc) {
            loosePath = argv[++i];
        } else if (strcmp(argv[i], "--expect") == 0 && i + 1 < argc) {
            expectSongs = atoi(argv[++i]);
        } else if (nPos < 4) {
            positional[nPos++] = argv[i];
        }
    }

    const bool arkMode = (loosePath == nullptr);
    int posIdx = 0;
    if (arkMode) {
        if (nPos < 1) {
            fprintf(stderr,
                    "usage: %s <dataDir> [maxSongs] [--expect <N>]\n"
                    "       %s --loose <songs.dta|dtb> [maxSongs] [--expect <N>]\n",
                    argv[0], argv[0]);
            return 1;
        }
        dataDir = positional[posIdx++];
    }
    if (posIdx < nPos)
        maxSongs = atoi(positional[posIdx]);
    if (maxSongs <= 0)
        maxSongs = 20;
    // 138 is the retail RB3 disc song count, independently established by the
    // rb3-ark driver and by tools/ark_extract.py's DTB extraction.
    if (expectSongs < 0)
        expectSongs = arkMode ? 138 : 0; // 0 => "no expectation" in loose mode

    Gates g;

    InitMakeString();
    Symbol::Init();
    InitM1Symbols(); // must follow Symbol::Init() (interns the Symbol globals)
    gSystemConfig = MakeMinimalSystemConfig();

    printf("=== rb3-xenon native M14: BandSongMgr fed from the .ark ===\n");
    printf("mode : %s\n", arkMode ? "ARCHIVE" : "loose file (legacy)");

    const char *dtaPath = loosePath;
    if (arkMode) {
        printf("data : %s\n", dataDir);
        NativeSetDataDir(dataDir);
        SetUsingCD(true); // ★ must precede any NewFile(); see the header comment.
        NativeArchiveInit();
        if (!TheArchive) {
            fprintf(stderr, "FATAL: TheArchive is null after NativeArchiveInit()\n");
            return 1;
        }
        // Asked for the way game code asks; CachedDataFile() rewrites this to
        // songs/gen/songs.dtb when UsingCD() && !FileIsLocal, so this also
        // exercises the archive's .dta -> .dtb redirection.
        dtaPath = "songs/songs.dta";
    }
    printf("dta  : %s\n\n", dtaPath);

    DataArray *root = DataReadFile(dtaPath, true);
    printf("--- gates ---\n");
    g.Check("DataReadFile", root != nullptr, dtaPath);
    if (!root)
        return g.Finish();

    {
        char d[96];
        snprintf(d, sizeof(d), "%d top-level node(s)", root->Size());
        g.Check("parsed non-empty", root->Size() > 0, d);
    }

    // Only fabricate ids where the data genuinely lacks them, and count it.
    int nextId = 100;
    int injected = 0, songArrays = 0;
    for (int i = 0; i < root->Size(); i++) {
        if (root->Node(i).Type() != kDataArray)
            continue;
        songArrays++;
        DataArray *song = root->Array(i);
        if (!HasSongId(song)) {
            InjectSongId(song, nextId++);
            injected++;
        }
    }
    {
        char d[128];
        snprintf(d, sizeof(d),
                 "%d of %d song array(s) needed a synthetic song_id", injected,
                 songArrays);
        // In archive mode this MUST be zero: retail metadata carries real ids.
        // In loose mode injection is expected, so the gate is informational.
        if (arkMode)
            g.Check("song_ids are REAL (none injected)", injected == 0, d);
        else
            printf("  [info] %s\n", d);
    }

    // Drive the real manager load path.
    BandSongMgr mgr;
    mgr.SongMgr::Init(); // base state only; skips ContentMgr/config Init()
    mgr.AddSongData(root, nullptr, kLocationRoot);

    const std::set<int> &avail = mgr.GetAvailableSongSet();
    printf("\nManager loaded %d song(s).\n", (int)avail.size());
    printf("  %-8s %-22s %-30s %-24s %s\n", "id", "shortname", "title", "artist",
           "id<->name");

    int shown = 0, roundTripOK = 0, withMeta = 0;
    for (std::set<int>::const_iterator it = avail.begin();
         it != avail.end() && shown < maxSongs; ++it) {
        int id = *it;
        // ALL of these go THROUGH the manager:
        Symbol shortName = mgr.GetShortNameFromSongID(id, false);
        BandSongMetadata *md = (BandSongMetadata *)mgr.Data(id);
        const char *title = md ? md->Title() : "(null)";
        const char *artist = md ? md->Artist() : "(null)";
        int backId = mgr.GetSongIDFromShortName(shortName, false); // round-trip
        bool ok = (backId == id);
        if (ok)
            roundTripOK++;
        if (md && title && *title && artist && *artist)
            withMeta++;
        printf("  %-8d %-22s %-30s %-24s %s\n", id, shortName.Str(), title, artist,
               ok ? "OK" : "MISMATCH");
        shown++;
    }

    printf("\n--- gates: manager ---\n");
    {
        char d[96];
        snprintf(d, sizeof(d), "manager reports %d song(s)", (int)avail.size());
        g.Check("manager loaded songs", !avail.empty(), d);
    }
    if (expectSongs > 0) {
        char d[96];
        snprintf(d, sizeof(d), "expected %d, manager loaded %d", expectSongs,
                 (int)avail.size());
        g.Check("song count == expected", (int)avail.size() == expectSongs, d);
    }
    {
        char d[96];
        snprintf(d, sizeof(d), "%d of %d round-trip(s) OK", roundTripOK, shown);
        g.Check("id <-> shortname round-trips", shown > 0 && roundTripOK == shown, d);
    }
    {
        char d[112];
        snprintf(d, sizeof(d), "%d of %d shown song(s) have title AND artist",
                 withMeta, shown);
        g.Check("metadata non-empty", shown > 0 && withMeta == shown, d);
    }

    return g.Finish();
}
