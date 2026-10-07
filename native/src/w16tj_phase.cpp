// rb3-xenon native -- W16-TJ: three gap rows no native target ran, run on real data.
//
// rb3-render's default run calls RunW16TJPhase() after the W16-TF phase. Before
// this lane GemManager::SetupGems and CustomizePanel::Handle were compiled into
// rb3-render and discarded by --gc-sections, and VocalTrack::UpdateScrolling was
// linked but never entered (docs/decomp/W16TF_NATIVE_RUNTIME_COVERAGE_2026-10-07.md
// section 6). The phase drives each one over shipped data:
//
//   * the SONG FIXTURE: songs/songs.dta loaded into the real BandSongMgr, the
//     song's real DataArraySongInfo, and its .mid read out of the ark by the
//     real SongParser into a real SongDB (SongData::PostLoad, SongDB phrases);
//   * GemManager::SetupGems over that SongDB's guitar and drum gem lists, on a
//     GemTrackDir from the shipped ui/track/gen/trackpanel.milo;
//   * VocalTrack::UpdateScrolling over the song's vocal note lists with a real
//     VocalPlayer, on the shipped ui/track/gen/vocals.milo;
//   * CustomizePanel::Handle over a panel holding real asset providers.
//
// THE REFERENCE. Every expected value is computed by this file from the song's
// raw MIDI bytes (MiniSmf below: its own track walk, tempo map and note pairing,
// sharing no code with SongParser), from songs.dta read directly, or from the
// formula the source states over inputs the code under test does not produce.
// Fixture gates first check that the parsed SongDB agrees with MiniSmf, so a
// later gate failure points at the function under test, not at the parse.

#include "bandobj/GemTrackDir.h"
#include "bandobj/TrackPanelDir.h"
#include "bandobj/VocalTrackDir.h"
#include "bandtrack/GemManager.h"
#include "bandtrack/GemTrack.h"
#include "bandtrack/TrackConfig.h"
#include "bandtrack/VocalTrack.h"
#include "beatmatch/GameGem.h"
#include "beatmatch/GameGemDB.h"
#include "beatmatch/PhraseAnalyzer.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "beatmatch/SongData.h"
#include "beatmatch/SongParser.h"
#include "beatmatch/TuningOffsetList.h"
#include "beatmatch/VocalNote.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Game.h"
#include "game/GameConfig.h"
#include "game/SongDB.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/SongUpgradeMgr.h"
#include "meta_band/CustomizePanel.h"
#include "meta_band/MetaPerformer.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "os/File.h"
#include "utl/BeatMap.h"
#include "utl/FileStream.h"
#include "utl/FilePath.h"
#include "utl/TempoMap.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

extern DataArray *gSystemConfig;
void SetTheBeatMap(BeatMap *);

typedef void (*GateFn)(const char *, bool, const char *);

namespace {

GateFn gGate = nullptr;
char gBuf[768];
int gRan = 0;

void Gate(const char *name, bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Gate(const char *name, bool ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gBuf, sizeof(gBuf), fmt, ap);
    va_end(ap);
    gGate(name, ok, gBuf);
    gRan++;
}

const char *kSong = "bohemianrhapsody";

// ================================================================ MiniSmf ==
// A standard MIDI file reader written for this phase: chunk walk, running
// status, the tempo map from FF 51 events, and note-on/note-off pairing.
struct SmfNote {
    int tick, endTick, pitch;
};
struct SmfText {
    int tick;
    int type;
    std::string text;
};
struct SmfTrack {
    std::string name;
    std::vector<SmfNote> notes;
    std::vector<SmfText> texts;
};
struct MiniSmf {
    int ppq = 0;
    std::vector<std::pair<int, int> > tempos; // (tick, usec per quarter)
    std::vector<SmfTrack> tracks;

    static unsigned Be(const unsigned char *p, int n) {
        unsigned v = 0;
        for (int i = 0; i < n; i++)
            v = (v << 8) | p[i];
        return v;
    }
    static unsigned Vlq(const unsigned char *b, size_t &i) {
        unsigned v = 0;
        while (true) {
            unsigned char c = b[i++];
            v = (v << 7) | (c & 0x7f);
            if (!(c & 0x80))
                return v;
        }
    }
    bool Parse(const std::vector<unsigned char> &b) {
        if (b.size() < 14 || memcmp(b.data(), "MThd", 4) != 0)
            return false;
        int ntr = Be(&b[10], 2);
        ppq = Be(&b[12], 2);
        size_t i = 8 + Be(&b[4], 4);
        for (int t = 0; t < ntr; t++) {
            if (i + 8 > b.size() || memcmp(&b[i], "MTrk", 4) != 0)
                return false;
            size_t end = i + 8 + Be(&b[i + 4], 4);
            if (end > b.size())
                return false;
            size_t j = i + 8;
            int tick = 0;
            unsigned char run = 0;
            SmfTrack tr;
            std::map<int, std::vector<int> > open; // pitch -> start ticks (FIFO)
            while (j < end) {
                tick += Vlq(b.data(), j);
                unsigned char st = b[j];
                if (st == 0xff) {
                    int type = b[j + 1];
                    j += 2;
                    unsigned len = Vlq(b.data(), j);
                    std::string data((const char *)b.data() + j, len);
                    j += len;
                    if (type == 0x51 && len == 3)
                        tempos.push_back(std::make_pair(tick, (int)Be(&b[j - 3], 3)));
                    else if (type == 3 && tr.name.empty())
                        tr.name = data;
                    else if (type >= 1 && type <= 5)
                        tr.texts.push_back(SmfText { tick, type, data });
                } else if (st == 0xf0 || st == 0xf7) {
                    j++;
                    j += Vlq(b.data(), j);
                } else {
                    if (st & 0x80) {
                        run = st;
                        j++;
                    }
                    int hi = run & 0xf0;
                    if (hi == 0xc0 || hi == 0xd0) {
                        j += 1;
                        continue;
                    }
                    int a = b[j], c = b[j + 1];
                    j += 2;
                    if (hi == 0x90 && c > 0) {
                        open[a].push_back(tick);
                    } else if (hi == 0x80 || (hi == 0x90 && c == 0)) {
                        std::vector<int> &q = open[a];
                        if (!q.empty()) {
                            tr.notes.push_back(SmfNote { q.front(), tick, a });
                            q.erase(q.begin());
                        }
                    }
                }
            }
            std::sort(tr.notes.begin(), tr.notes.end(), [](const SmfNote &x, const SmfNote &y) {
                return x.tick != y.tick ? x.tick < y.tick : x.pitch < y.pitch;
            });
            tracks.push_back(tr);
            i = end;
        }
        std::stable_sort(tempos.begin(), tempos.end());
        return ppq > 0;
    }
    const SmfTrack *Track(const char *name) const {
        for (size_t i = 0; i < tracks.size(); i++)
            if (tracks[i].name == name)
                return &tracks[i];
        return nullptr;
    }
    // Piecewise-linear tick -> ms over the tempo events (120 BPM before the first).
    double Ms(double tick) const {
        double ms = 0, lastTick = 0, usec = 500000;
        for (size_t i = 0; i < tempos.size() && tempos[i].first <= tick; i++) {
            ms += (tempos[i].first - lastTick) * usec / (ppq * 1000.0);
            lastTick = tempos[i].first;
            usec = tempos[i].second;
        }
        return ms + (tick - lastTick) * usec / (ppq * 1000.0);
    }
};

bool ReadArkFile(const char *path, std::vector<unsigned char> &out) {
    File *f = NewFile(path, FILE_OPEN_READ); // through the ark
    if (!f)
        return false;
    int n = f->Size();
    out.resize(n);
    int got = f->Read(&out[0], n);
    delete f;
    return got == n;
}

// ======================================================= config splicing ==
// band_keep.dta assembles these sections from per-file includes; this driver
// reads only the preinit half (main_render.cpp StandUpConfig), so each section
// the phase needs is built here from the same shipped files, the same way.
DataArray *FileBody(const char *file) { return DataReadFile(file, true); }

void AppendNodes(DataArray *dst, int &at, DataArray *src) {
    for (int i = 0; i < src->Size(); i++)
        dst->Node(at++) = src->Node(i);
}

DataArray *InstallSection(DataArray *sec) {
    int n = gSystemConfig->Size();
    gSystemConfig->Resize(n + 1);
    gSystemConfig->Node(n) = DataNode(sec, kDataArray);
    sec->Release();
    return sec;
}

// (name (sub1 <file1>) ... <rest file nodes>)
DataArray *SpliceNested(
    const char *name, const char *const *subNames, const char *const *subFiles, int nSub,
    const char *restFile
) {
    DataArray *have = SystemConfig()->FindArray(name, false);
    if (have)
        return have;
    std::vector<DataArray *> subs;
    for (int k = 0; k < nSub; k++) {
        DataArray *body = FileBody(subFiles[k]);
        if (!body)
            return nullptr;
        subs.push_back(body);
    }
    DataArray *rest = restFile ? FileBody(restFile) : nullptr;
    if (restFile && !rest)
        return nullptr;
    DataArray *sec = new DataArray(1 + nSub + (rest ? rest->Size() : 0));
    int at = 0;
    sec->Node(at++) = Symbol(name);
    for (int k = 0; k < nSub; k++) {
        DataArray *s = new DataArray(subs[k]->Size() + 1);
        int a2 = 0;
        s->Node(a2++) = Symbol(subNames[k]);
        AppendNodes(s, a2, subs[k]);
        sec->Node(at++) = DataNode(s, kDataArray);
        s->Release();
        subs[k]->Release();
    }
    if (rest) {
        AppendNodes(sec, at, rest);
        rest->Release();
    }
    InstallSection(sec);
    return SystemConfig()->FindArray(name, false);
}

DataArray *Splice(const char *name, const char *file) {
    return SpliceNested(name, nullptr, nullptr, 0, file);
}

// ========================================================== song fixture ==
struct Fixture {
    MiniSmf smf;
    DataArray *songEntry = nullptr; // songs.dta's (bohemianrhapsody ...)
    SongInfo *info = nullptr;
    SongDB *db = nullptr;
    PlayerTrackConfigList *list = nullptr;
    NullLocalBandUser *guitar = nullptr, *drums = nullptr, *vocals = nullptr;
    int guitarTrack = -1, drumTrack = -1, vocalTrack = -1;
    std::string midPath;
};

// The (song ...) block of songs.dta for kSong, read directly.
DataArray *FindSongEntry(DataArray *root) {
    for (int i = 0; i < root->Size(); i++) {
        if (root->Node(i).Type() != kDataArray)
            continue;
        DataArray *a = root->Array(i);
        if (a->Size() > 0 && a->Node(0).Type() == kDataSymbol && a->Sym(0) == kSong)
            return a;
    }
    return nullptr;
}

bool BuildFixture(Fixture &fx) {
    printf("\n=== W16-TJ: song fixture (%s) ===\n", kSong);
    static const char *bmSub[] = { "controller", "midi_parsers" };
    static const char *bmFiles[] = { "config/beatmatch_controller.dta",
                                     "config/midi_parsers.dta" };
    DataArray *bm = SpliceNested("beatmatcher", bmSub, bmFiles, 2, "config/beatmatcher.dta");
    DataArray *sc = Splice("scoring", "config/scoring.dta");
    DataArray *pl = Splice("player", "config/player.dta");
    // BandSongMgr::AddSongData fills absent song keys from this section.
    Splice("missing_song_data", "songs/missing_song_data.dta");
    DataArray *parser = bm ? bm->FindArray("parser", false) : nullptr;
    Gate("tj-config", bm && sc && pl && parser && parser->FindArray("track_mapping", false),
         "beatmatcher (controller, midi_parsers, beatmatcher.dta), scoring, player spliced "
         "from the shipped config files");
    if (!(bm && sc && pl && parser))
        return false;

    // --- the song manager over the shipped songs.dta -----------------------
    DataArray *root = DataReadFile("songs/songs.dta", true);
    fx.songEntry = root ? FindSongEntry(root) : nullptr;
    if (!fx.songEntry) {
        Gate("tj-songmgr", false, "songs/songs.dta has no (%s ...) entry", kSong);
        return false;
    }
    TheSongMgr.SongMgr::Init(); // base state only, as rb3-song does
    // BandSongMgr::Init's tail also builds the upgrade manager, which
    // SongAudioData consults; build that one piece (no song has an upgrade).
    if (!TheSongMgr.mUpgradeMgr) {
        // TheContentMgr is static storage, so its refresh state is 0 (kDone).
        TheSongMgr.mUpgradeMgr = new SongUpgradeMgr();
    }
    TheSongMgr.AddSongData(root, nullptr, kLocationRoot);
    int id = TheSongMgr.GetSongIDFromShortName(kSong, false);
    BandSongMetadata *md = id ? (BandSongMetadata *)TheSongMgr.Data(id) : nullptr;
    DataArray *idArr = fx.songEntry->FindArray("song_id", false);
    int wantId = idArr ? idArr->Int(1) : -1;
    fx.info = md ? TheSongMgr.SongAudioData(id) : nullptr;
    Gate("tj-songmgr", md && id == wantId && fx.info,
         "%s -> song id %d (songs.dta: %d), metadata %s, song info %s", kSong, id, wantId,
         md ? "found" : "MISSING", fx.info ? fx.info->GetBaseFileName() : "-");
    if (!fx.info)
        return false;

    // --- the raw MIDI, read by MiniSmf --------------------------------------
    fx.midPath = std::string(fx.info->GetBaseFileName()) + ".mid";
    std::vector<unsigned char> bytes;
    bool read = ReadArkFile(fx.midPath.c_str(), bytes) && fx.smf.Parse(bytes);
    Gate("tj-smf", read && fx.smf.Track("PART GUITAR") && fx.smf.Track("PART VOCALS")
             && fx.smf.Track("BEAT"),
         "%s: %d bytes, ppq %d, %d tracks, %d tempo events", fx.midPath.c_str(),
         (int)bytes.size(), fx.smf.ppq, (int)fx.smf.tracks.size(), (int)fx.smf.tempos.size());
    if (!read)
        return false;

    // --- the real parse, into the real SongDB (main_vocal2.cpp's sequence) --
    Game *game = (Game *)calloc(1, sizeof(Game));
    game->mProperties.mEnableStreak = true;
    game->mProperties.mEnableOverdrive = true;
    game->mProperties.mAllowOverdrivePhrases = true;
    game->unkdc = -1.0f; // normal play, not rollback
    TheGame = game;
    fx.db = new SongDB();
    TheSongDB = fx.db;
    SongData &sd = *fx.db->GetData();
    sd.mNumDifficulties = 4;
    sd.mHopoThreshold = fx.info->GetHopoThreshold();
    sd.mSongInfo = fx.info;
    sd.mDetailedGrid = false;
    sd.mBeatMap = new BeatMap();
    SetTheBeatMap(sd.mBeatMap);
    sd.mPhraseAnalyzer = new PhraseAnalyzer(&sd);
    sd.mTuningOffsetList = new TuningOffsetList();
    sd.mKeyboardRangeSections.resize(4);
    {
        FileStream fs(fx.midPath.c_str(), FileStream::kRead, false);
        SongParser parser(sd, 4, sd.mTempoMap, sd.mMeasureMap, 2);
        parser.ReadMidiFile(fs, fx.midPath.c_str(), fx.info);
        int pumps = 0;
        while (!parser.NoMidiReader() && pumps < 4000000) {
            parser.Poll();
            pumps++;
        }
    }
    if (sd.mTempoMap)
        sd.mTempoMap->Finalize();
    for (size_t i = 0; i < sd.mGemDBs.size(); i++)
        sd.mGemDBs[i]->MergeChordGems();

    // Three local players at Expert: guitar, drums, vocals.
    fx.guitar = BandUser::NewNullLocalBandUser();
    fx.drums = BandUser::NewNullLocalBandUser();
    fx.vocals = BandUser::NewNullLocalBandUser();
    fx.guitar->mTrackType = kTrackGuitar;
    fx.drums->mTrackType = kTrackDrum;
    fx.vocals->mTrackType = kTrackVocals;
    // The slot map BandUser::GetSlot reads. A real BandUserMgr's ctor sinks
    // to TheProfileMgr/ThePlatformMgr, which this driver lacks (and SetSlot
    // links the session sync path); a zeroed object with the map written as
    // SetSlot writes it, no session.
    if (!TheBandUserMgr)
        TheBandUserMgr = (BandUserMgr *)calloc(1, sizeof(BandUserMgr));
    TheBandUserMgr->mSlotMap[0] = fx.guitar->GetUserGuid();
    TheBandUserMgr->mSlotMap[1] = fx.drums->GetUserGuid();
    TheBandUserMgr->mSlotMap[2] = fx.vocals->GetUserGuid();
    fx.list = new PlayerTrackConfigList(3);
    fx.list->mDefaultDifficulty = kDifficultyExpert;
    fx.list->AddConfig(fx.guitar->GetUserGuid(), kTrackGuitar, kDifficultyExpert, 0, false);
    fx.list->AddConfig(fx.drums->GetUserGuid(), kTrackDrum, kDifficultyExpert, 1, false);
    fx.list->AddConfig(fx.vocals->GetUserGuid(), kTrackVocals, kDifficultyExpert, 2, false);
    sd.mPlayerTrackConfigList = fx.list;
    GameConfig *cfg = (GameConfig *)calloc(1, sizeof(GameConfig));
    cfg->mPlayerTrackConfigList = fx.list;
    TheGameConfig = cfg;
    sd.PostLoad(fx.list);
    fx.guitarTrack = fx.list->GetTrackNumByUserGuid(fx.guitar->GetUserGuid());
    fx.drumTrack = fx.list->GetTrackNumByUserGuid(fx.drums->GetUserGuid());
    fx.vocalTrack = fx.list->GetTrackNumByUserGuid(fx.vocals->GetUserGuid());
    bool tracksOk = fx.guitarTrack >= 0 && fx.drumTrack >= 0 && fx.vocalTrack >= 0
        && sd.TrackNamed(Symbol("PART GUITAR")) == fx.guitarTrack
        && sd.TrackNamed(Symbol("PART DRUMS")) == fx.drumTrack;
    Gate("tj-parse", tracksOk && sd.GetVocalNoteListCount() > 0,
         "SongParser: guitar track %d, drums %d, vocals %d; %d vocal note list(s); hopo threshold %d",
         fx.guitarTrack, fx.drumTrack, fx.vocalTrack, sd.GetVocalNoteListCount(),
         sd.mHopoThreshold);
    if (!tracksOk)
        return false;
    float lastMs = 0;
    for (int t : { fx.guitarTrack, fx.drumTrack }) {
        const std::vector<GameGem> &g = fx.db->GetGems(t);
        if (!g.empty())
            lastMs = std::max(lastMs, g.back().mMs + g.back().mDurationMs);
    }
    fx.db->mSongDurationMs = lastMs + 3000.0f;
    fx.db->SetupPhrases();
    fx.db->DisableCodaGems();
    return true;
}


// ======================================================= SetupGems gates ==
// MiniSmf's expected 5-lane gems: Expert notes (pitches 96..100) grouped by
// tick, lane = pitch - 96. Duration of a group = its longest note.
struct RefGem {
    int tick, endTick;
    unsigned slots;
};
std::vector<RefGem> RefLaneGems(const SmfTrack &t) {
    std::vector<RefGem> out;
    for (size_t i = 0; i < t.notes.size(); i++) {
        const SmfNote &n = t.notes[i];
        if (n.pitch < 96 || n.pitch > 100)
            continue;
        if (!out.empty() && out.back().tick == n.tick) {
            out.back().slots |= 1u << (n.pitch - 96);
            out.back().endTick = std::max(out.back().endTick, n.endTick);
        } else {
            out.push_back(RefGem { n.tick, n.endTick, 1u << (n.pitch - 96) });
        }
    }
    return out;
}

// HOPO from the MIDI, by the published rule: inside an Expert force-HOPO
// marker (pitch 101) a gem is a HOPO, inside a force-strum marker (102) it is
// not; otherwise it is one when it is a single lane, shares no lane with the
// previous gem, and starts within the song's HOPO threshold of it.
int RefHopoThreshold(const Fixture &fx) {
    DataArray *song = fx.songEntry->FindArray("song", false);
    DataArray *t = song ? song->FindArray("hopo_threshold", false) : nullptr;
    if (t)
        return t->Int(1);
    return SystemConfig("beatmatcher")->FindArray("parser")->FindInt("hopo_threshold");
}

std::vector<bool> RefHopo(const SmfTrack &t, const std::vector<RefGem> &gems, int thresh) {
    std::vector<std::pair<int, int> > on, off;
    for (size_t i = 0; i < t.notes.size(); i++) {
        if (t.notes[i].pitch == 101)
            on.push_back(std::make_pair(t.notes[i].tick, t.notes[i].endTick));
        else if (t.notes[i].pitch == 102)
            off.push_back(std::make_pair(t.notes[i].tick, t.notes[i].endTick));
    }
    auto inside = [](const std::vector<std::pair<int, int> > &r, int tick) {
        for (size_t i = 0; i < r.size(); i++)
            if (r[i].first <= tick && tick < r[i].second)
                return true;
        return false;
    };
    std::vector<bool> out(gems.size(), false);
    for (size_t i = 0; i < gems.size(); i++) {
        int tick = gems[i].tick;
        if (inside(on, tick))
            out[i] = true;
        else if (inside(off, tick))
            out[i] = false;
        else if (i > 0) {
            unsigned s = gems[i].slots, prev = gems[i - 1].slots;
            out[i] = tick - gems[i - 1].tick <= thresh && (s & (s - 1)) == 0 && !(s & prev);
        }
    }
    return out;
}

ObjDirPtr<ObjectDir> gTrackPanel;

GemTrackDir *FirstGemTrackDir() {
    FilePath fp("ui/track/gen/trackpanel.milo_xbox");
    gTrackPanel.LoadFile(fp, false, false, kLoadFront, false);
    TrackPanelDir *tp = dynamic_cast<TrackPanelDir *>(gTrackPanel.Ptr());
    if (!tp)
        return nullptr;
    GemTrackDir *g = tp->Find<GemTrackDir>("track_0", false);
    if (!g)
        return nullptr;
    // The per-slot half of TrackPanelDir::AssignTrack(0, kInstGuitar): its
    // first step sizes mTracks off the game-side TrackPanel, which this
    // driver does not build. The smasher plate comes from the panel's real
    // GemTrackResourceManager (gem_track_resources), as in a session.
    g->SetTrackIdx(0);
    g->SetInstrument(kInstGuitar);
    g->SetUsed(true);
    g->SetupSmasherPlate();
    return g;
}

void SetupGemsChecks(Fixture &fx) {
    printf("\n=== W16-TJ: GemManager::SetupGems over %s PART GUITAR ===\n", kSong);
    GemTrackDir *dir = FirstGemTrackDir();
    if (!dir) {
        Gate("sg-trackdir", false, "no GemTrackDir in ui/track/gen/trackpanel.milo");
        return;
    }
    printf("  GemTrackDir '%s'\n", dir->Name());
    if (!dir->SmasherPlate()) {
        Gate("sg-trackdir", false, "%s has no smasher plate after SetupSmasherPlate", dir->Name());
        return;
    }
    // SetupGems resolves the song through MetaPerformer::Current()->Song() and
    // TheSongMgr. A real MetaPerformer's ctor registers with TheNetSession,
    // which this driver does not have; Song() reads only mSongs/mStars.
    MetaPerformer *mp = (MetaPerformer *)calloc(1, sizeof(MetaPerformer));
    new (&mp->mSongs) std::vector<Symbol>();
    new (&mp->mStars) std::vector<int>();
    mp->mSongs.push_back(Symbol(kSong));
    MetaPerformer::sMetaPerformer = mp;

    GemTrack *track = new GemTrack(fx.guitar);
    fx.guitar->SetTrack(track);
    TrackConfig &tc = track->mTrackConfig;
    tc.SetTrackNum(fx.guitarTrack);
    tc.SetMaxSlots(5);
    GemManager *gm = new GemManager(tc, dir); // the ctor runs SetupGems(0)

    const std::vector<GameGem> &gems = fx.db->GetGems(fx.guitarTrack);
    std::vector<RefGem> ref = RefLaneGems(*fx.smf.Track("PART GUITAR"));
    // Fixture: the parsed list is MiniSmf's list (ticks and lanes).
    int fixBad = 0;
    for (size_t i = 0; i < std::min(ref.size(), gems.size()); i++)
        if (gems[i].mTick != ref[i].tick || gems[i].mSlots != ref[i].slots)
            fixBad++;
    Gate("sg-fixture", gems.size() == ref.size() && fixBad == 0,
         "SongDB guitar gems %d, MiniSmf Expert gems %d, %d differ in tick or lanes",
         (int)gems.size(), (int)ref.size(), fixBad);

    // 1. one Gem per GameGem, in order, bound to it.
    bool bound = gm->mGems.size() == gems.size();
    for (size_t i = 0; bound && i < gems.size(); i++)
        bound = &gm->mGems[i].GetGameGem() == &gems[i];
    Gate("sg-count", bound, "%d Gems for %d GameGems, each bound to its own GameGem",
         (int)gm->mGems.size(), (int)gems.size());
    if (!bound)
        return;

    // 2. times in seconds from MiniSmf's own tempo map. A gem whose duration
    // the parser marks ignorable ends where it starts.
    int timeBad = 0, sustains = 0;
    double worst = 0;
    for (size_t i = 0; i < ref.size() && i < gems.size(); i++) {
        const Gem &g = gm->mGems[i];
        double wantStart = fx.smf.Ms(ref[i].tick) / 1000.0;
        bool ignore = gems[i].mIgnoreDuration && !gems[i].LeftHandSlide();
        double wantEnd = ignore ? wantStart : fx.smf.Ms(ref[i].endTick) / 1000.0;
        if (!ignore)
            sustains++;
        double e = std::max(std::fabs(g.mStart - wantStart), std::fabs(g.mEnd - wantEnd));
        worst = std::max(worst, e);
        // 2 ms: GameGem stores its duration as whole milliseconds.
        if (e > 0.002) {
            if (timeBad < 3)
                printf("  gem %d tick %d: start %.4f end %.4f, want %.4f %.4f\n", (int)i,
                       ref[i].tick, g.mStart, g.mEnd, wantStart, wantEnd);
            timeBad++;
        }
    }
    Gate("sg-times", timeBad == 0,
         "%d gems (%d sustained): start/end seconds vs MiniSmf's tempo map, worst %.2f ms",
         (int)gems.size(), sustains, worst * 1000.0);

    // 3. lanes and HOPO. SetupGems(0) keeps every gem's lanes. HOPO follows
    // RefHopo, except that the track's very first gem is always strummed.
    int thresh = RefHopoThreshold(fx);
    std::vector<bool> refHopo = RefHopo(*fx.smf.Track("PART GUITAR"), ref, thresh);
    int laneBad = 0, hopoBad = 0, hopos = 0, forced = 0;
    for (size_t i = 0; i < ref.size() && i < gems.size(); i++) {
        const Gem &g = gm->mGems[i];
        if (g.mSlots != ref[i].slots)
            laneBad++;
        bool wantHopo = refHopo[i] && i >= 1;
        hopos += wantHopo;
        if (g.mHopo != wantHopo) {
            if (hopoBad < 3)
                printf("  gem %d tick %d slots %x: hopo %d, want %d\n", (int)i, ref[i].tick,
                       ref[i].slots, (int)g.mHopo, (int)wantHopo);
            hopoBad++;
        }
    }
    for (size_t i = 0; i < fx.smf.Track("PART GUITAR")->notes.size(); i++) {
        int p = fx.smf.Track("PART GUITAR")->notes[i].pitch;
        forced += p == 101 || p == 102;
    }
    Gate("sg-lanes", laneBad == 0, "%d of %d gems carry MiniSmf's lane mask", (int)ref.size() - laneBad,
         (int)ref.size());
    Gate("sg-hopo", hopoBad == 0 && hopos > 0,
         "%d HOPO gems by the MIDI rule (threshold %d ticks, %d force markers), %d disagree",
         hopos, thresh, forced, hopoBad);

    // 4. SetupGems(startTick) outside practice/trainer: every gem before the
    // start tick loses its lanes, the rest keep them.
    int cut = ref[ref.size() / 2].tick;
    gm->ClearGems(true);
    gm->SetupGems(cut);
    int before = 0, cutBad = 0;
    for (size_t i = 0; i < ref.size(); i++) {
        unsigned want = ref[i].tick < cut ? 0u : ref[i].slots;
        before += ref[i].tick < cut;
        if (gm->mGems[i].mSlots != want)
            cutBad++;
    }
    Gate("sg-start-tick", gm->mGems.size() == ref.size() && cutBad == 0 && before > 0,
         "SetupGems(%d): %d gems before the cut emptied, %d after kept; %d wrong", cut,
         before, (int)ref.size() - before, cutBad);
    gm->ClearGems(true);
    gm->SetupGems(0);
}

} // namespace

int RunW16TJPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== W16-TJ phase: SetupGems / UpdateScrolling / CustomizePanel::Handle ===\n");
    static Fixture fx;
    if (!BuildFixture(fx))
        return gRan;
    SetupGemsChecks(fx);
    return gRan;
}
