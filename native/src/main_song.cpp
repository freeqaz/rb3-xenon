// rb3-xenon native — M1 song-manager loader.
//
// Boots the minimum Milo engine, parses a RB3 songs.dta, and drives the REAL
// BandSongMgr / BandSongMetadata load path (the SongMgr/BandSongMgr cluster),
// then prints each song's metadata *retrieved through the manager* — proving
// the M1 cluster works end to end without a renderer, audio, or ContentMgr.
//
// Note on test data: the rbtp2 songs.dta is RB1-era and has no (song_id N)
// node. RB3's real load path REQUIRES one — SongMetadata's ctor MILO_FAILs
// without it — so this driver injects a synthetic per-song song_id (exactly
// what a shipping RB3 songs.dta carries). Everything else (name/artist/rank/
// genre/etc.) is parsed by the manager from the real dta.

#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "utl/Symbol.h"
#include <cstdio>
#include <cstdlib>

extern void InitMakeString();
extern void InitM1Symbols();     // native/src/m1_symbols.cpp — interns Symbol globals
extern DataArray *gSystemConfig; // src/system/os/System.cpp

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

// Append (song_id <id>) to a song array so the manager's GetSongID()/
// SongMetadata ctor can key on it. See the file header note.
static void InjectSongId(DataArray *song, int id) {
    DataArray *idArr = new DataArray(2);
    idArr->Node(0) = DataNode(Symbol("song_id"));
    idArr->Node(1) = DataNode(id);
    song->Insert(song->Size(), DataNode(idArr, kDataArray));
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <songs.dta> [maxSongs]\n", argv[0]);
        return 1;
    }
    int maxSongs = (argc >= 3) ? atoi(argv[2]) : 20;

    InitMakeString();
    Symbol::Init();
    InitM1Symbols(); // must follow Symbol::Init() (interns the Symbol globals)
    gSystemConfig = MakeMinimalSystemConfig();

    DataArray *root = DataReadFile(argv[1], true);
    if (!root) {
        fprintf(stderr, "FAILED to parse %s\n", argv[1]);
        return 1;
    }
    printf("Parsed %s: %d top-level nodes\n", argv[1], root->Size());

    // Assign synthetic song IDs (100, 101, ...) to each song array.
    int nextId = 100;
    for (int i = 0; i < root->Size(); i++) {
        if (root->Node(i).Type() != kDataArray)
            continue;
        InjectSongId(root->Array(i), nextId++);
    }

    // Drive the real manager load path.
    BandSongMgr mgr;
    mgr.SongMgr::Init(); // base state only; skips ContentMgr/config Init()
    mgr.AddSongData(root, nullptr, kLocationRoot);

    const std::set<int> &avail = mgr.GetAvailableSongSet();
    printf("Manager loaded %d song(s).\n", (int)avail.size());
    printf("  %-4s %-22s %-28s %-22s %s\n", "id", "shortname", "title", "artist", "id<->name");

    int shown = 0;
    int roundTripOK = 0;
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
        printf("  %-4d %-22s %-28s %-22s %s\n", id, shortName.Str(), title, artist,
               ok ? "OK" : "MISMATCH");
        shown++;
    }

    printf("Done. Showed %d song(s); %d/%d id<->shortname round-trips OK.\n", shown,
           roundTripOK, shown);
    return 0;
}
