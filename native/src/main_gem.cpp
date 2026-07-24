// rb3-xenon native — M3a beatmatch gem-pipeline driver.
//
// Turns on the gem half of the beatmatch pipeline that M2 left dark (M2 passed
// gems=null so MidiParser's gem branch was skipped). Real engine code paths
// throughout — the only new code here is thin glue that implements two engine
// interfaces (SongInfo, InternalSongParserSink/GemListInterface).
//
//   Stage 1: SongParser (the REAL gem parser, a MidiReceiver) reads a real .mid.
//            MidiReader builds the TempoMap/MeasureMap and drives SongParser
//            across its three passes (beat / non-parts / parts). On the parts
//            pass SongParser::OnMidiMessageGem pairs note-on/off on PART <inst>,
//            computes slots + difficulty (PitchToSlot), and emits a MultiGemInfo
//            to the sink. The sink stores each gem into a real GameGemDB ->
//            GameGemList (the concrete GemListInterface containers, newly ported
//            from the rb3-Wii oracle: GameGem.cpp / GameGemList.cpp / GameGemDB.cpp).
//
//   Stage 2: The same sink is handed to MidiParserMgr as its GemListInterface.
//            MidiParserMgr::OnEndOfTrack calls mGems->SetTrack(name) then
//            MidiParser::ParseAll(mGems, ...), which fires the previously-skipped
//            gem branch (mGems->GetGem) and re-emits each gem as a DataEvent.
//
// Output: the queryable gem list — total gems and the first ~10 with
// (tick, ms, slots bitmask, duration). Stress-check on a large PART BASS .mid.

#include "beatmatch/SongParser.h"
#include "beatmatch/GameGemDB.h"
#include "beatmatch/GameGemList.h"
#include "beatmatch/GameGem.h"
#include "beatmatch/GemListInterface.h"
#include "beatmatch/InternalSongParserSink.h"
#include "beatmatch/TrackType.h"
#include "utl/SongInfoCopy.h"
#include "utl/SongInfoAudioType.h"
#include "utl/TempoMap.h"
#include "utl/MeasureMap.h"
#include "midi/Midi.h"
#include "midi/MidiParserMgr.h"
#include "midi/MidiParser.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "utl/FileStream.h"
#include "utl/Symbol.h"

#include <cstdio>
#include <map>
#include <string>
#include <vector>

extern void InitMakeString();
extern DataArray *gSystemConfig; // src/system/os/System.cpp
void DataInit();                 // src/system/obj/Data.cpp

// The number of note difficulties RB3 authors per instrument track.
static const int kNumDifficulties = 4; // Easy / Medium / Hard / Expert
static const int kExpert = 3;

// ---------------------------------------------------------------- config ----
// SystemConfig()->FindArray("beatmatcher") is read by the SongParser ctor:
//   parser.track_mapping  : <TRACK> -> (audio_type track_type part_sym)
//   parser.player_slot / low_vocal_pitch / high_vocal_pitch (optional)
//   parser.keyboard_range_shift_duration_ms (required float)
//   audio (required array; submixes optional)
//   watcher (optional)
// track_mapping keys are the PART-stripped names ("PART BASS" -> "BASS").
static const char *kBeatmatcherDta =
    "(beatmatcher"
    "   (parser"
    "      (player_slot 9)"
    "      (low_vocal_pitch 36)"
    "      (high_vocal_pitch 84)"
    "      (keyboard_range_shift_duration_ms 100.0)"
    "      (track_mapping"
    // Node(0)=stripped key, Int(1)=SongInfoAudioType, Int(2)=TrackType,
    // Sym(3)=full track name (matched by SongParser::ContainsTrackName). These
    // int values are the kAudio*/kTrack* enum numbers the retail beatmatch.dta
    // #defines expand to (kAudioBass=3/kTrackBass=2, etc.).
    "         (DRUMS  0 0 'PART DRUMS')"
    "         (BASS   3 2 'PART BASS')"
    "         (GUITAR 4 1 'PART GUITAR')"
    "         (VOCALS 6 3 'PART VOCALS')"
    "         (KEYS   9 4 'PART KEYS')"
    "      )"
    "   )"
    "   (audio (submixes))"
    ")";

// -------------------------------------------------------------- SongInfo ----
// Minimal concrete SongInfo. SongParser only queries NumChannelsOfTrack (via
// AudioTrackUsed) and TrackIndex (via AnalyzeTrackList). Report every standard
// instrument as present so whatever PART tracks the .mid contains get parsed;
// the audio track number is cosmetic on the gem path (the sink ignores it).
class NativeSongInfo : public SongInfo {
public:
    Symbol GetName() const { return Symbol("native_test"); }
    const char *GetBaseFileName() const { return "native_test"; }
    const char *GetPackageName() const { return ""; }
    const std::vector<TrackChannels> &GetTracks() const { return mTracks; }
    bool IsPlayTrackChannel(int) const { return true; }
    const TrackChannels *FindTrackChannel(SongInfoAudioType) const { return nullptr; }
    int NumChannelsOfTrack(SongInfoAudioType t) const { return t <= kAudioTypeKeys3 ? 2 : 0; }
    int TrackIndex(SongInfoAudioType t) const { return (int)t; }
    int GetNumVocalParts() const { return 1; }
    int GetHopoThreshold() const { return 170; }
    const std::vector<float> &GetPans() const { return mPans; }
    const std::vector<float> &GetVols() const { return mVols; }
    const std::vector<int> &GetCores() const { return mCores; }
    const std::vector<int> &GetCrowdChannels() const { return mCores; }
    const std::vector<Symbol> &GetDrumSoloSamples() const { return mSyms; }
    const std::vector<Symbol> &GetDrumFreestyleSamples() const { return mSyms; }
    float GetMuteVolume() const { return -96.0f; }
    float GetVocalMuteVolume() const { return -96.0f; }
    bool UnkTU5Virtual_0x4c() const { return false; }
    int NumExtraMidiFiles() const { return 0; }
    const char *GetExtraMidiFile(int) const { return nullptr; }

    std::vector<TrackChannels> mTracks;
    std::vector<float> mPans, mVols;
    std::vector<int> mCores;
    std::vector<Symbol> mSyms;
};

// ---------------------------------------------------------------- sink -------
// Concrete InternalSongParserSink + GemListInterface. The sink half receives
// parsed gems (AddMultiGem) and stores them into a real GameGemDB per track
// (one GameGemList per difficulty). The GemListInterface half (SetTrack/GetGem)
// is what MidiParserMgr later queries to re-emit gems as DataEvents — identical
// shape to SongData::SetTrack / SongData::GetGem, minus the fake-track path.
class NativeGemSink : public InternalSongParserSink, public GemListInterface {
public:
    struct TrackRec {
        Symbol name;
        TrackType type;
        GameGemDB *db;
    };
    std::vector<TrackRec> mTracks;      // index == song-data track number
    GameGemList *mCurGems = nullptr;    // GemListInterface cursor (SetTrack)

    // --- InternalSongParserSink (gem-relevant methods) ---
    void AddTrack(int trk, AudioTrackNum, Symbol name, SongInfoAudioType,
                  TrackType type, bool) {
        if ((int)mTracks.size() <= trk)
            mTracks.resize(trk + 1);
        mTracks[trk].name = name;
        mTracks[trk].type = type;
        mTracks[trk].db = new GameGemDB(kNumDifficulties, 170);
    }
    void ClearTrack(int trk) {
        if (trk >= 0 && trk < (int)mTracks.size() && mTracks[trk].db)
            mTracks[trk].db->Clear();
    }
    void OnEndOfTrack(int trk, bool merge) {
        if (merge && trk >= 0 && trk < (int)mTracks.size() && mTracks[trk].db) {
            mTracks[trk].db->MergeChordGems();
            mTracks[trk].db->Finalize();
        }
    }
    void AddMultiGem(int diff, const MultiGemInfo &info) {
        int trk = info.track;
        if (trk >= 0 && trk < (int)mTracks.size() && mTracks[trk].db) {
            mTracks[trk].db->AddMultiGem(diff, info);
            mGemCount++;
        }
    }
    void AddRGGem(int diff, const RGGemInfo &info) {
        int trk = info.track;
        if (trk >= 0 && trk < (int)mTracks.size() && mTracks[trk].db)
            mTracks[trk].db->AddRGGem(diff, info);
    }
    // Off the gem path — no-ops (vocals / phrases / fills / mixes / beats).
    void AddVocalNote(const VocalNote &) {}
    void AddPitchOffset(int, float) {}
    void AddLyricShift(int) {}
    void OnTambourineGem(int) {}
    void StartVocalPlayerPhrase(int, int) {}
    void EndVocalPlayerPhrase(int, int) {}
    void AddPhrase(BeatmatchPhraseType, int, int, float, int, float, int) {}
    void AddDrumFill(int, int, int, int, bool) {}
    void AddRoll(int, int, unsigned int, int, int) {}
    void AddTrill(int, int, int, int, int, int) {}
    void AddRGRoll(int, int, const RGRollChord &, int, int) {}
    void AddRGTrill(int, int, const RGTrill &, int, int) {}
    void AddMix(int, int, int, const char *) {}
    void AddLyricEvent(int, int, const char *) {}
    void DrumMapLane(int, int, int, bool) {}
    void AddBeat(int, int) {}
    void SetDetailedGrid(bool) {}
    void AddRangeShift(int, float) {}
    void AddKeyboardRangeShift(int, int, float, int, int) {}

    // --- GemListInterface (query half, mirrors SongData) ---
    void SetTrack(Symbol name) {
        mCurGems = nullptr;
        GameGemDB *db = FindDB(name);
        if (db)
            mCurGems = db->GetDiffGemList(kNumDifficulties - 1); // Expert
    }
    bool GetGem(int gemId, int &startTick, int &endTick, int &slots) {
        if (mCurGems && gemId < mCurGems->NumGems()) {
            GameGem &g = mCurGems->GetGem(gemId);
            startTick = g.GetTick();
            endTick = startTick + g.GetDurationTicks();
            slots = g.GetSlots();
            return true;
        }
        return false;
    }

    GameGemDB *FindDB(Symbol name) {
        for (size_t i = 0; i < mTracks.size(); i++)
            if (mTracks[i].name == name && mTracks[i].db)
                return mTracks[i].db;
        return nullptr;
    }
    GameGemList *ExpertList(Symbol name) {
        GameGemDB *db = FindDB(name);
        return db ? db->GetDiffGemList(kNumDifficulties - 1) : nullptr;
    }

    int mGemCount = 0;
};

// ---------------------------------------------------------------- main -------
int main(int argc, char **argv) {
    const char *defMid =
        "/home/free/code/milohax/onyx/songs-cort/hurt/pills/notes.mid";
    const char *midPath = (argc >= 2) ? argv[1] : defMid;

    InitMakeString();
    Symbol::Init();
    DataInit();
    ObjectDir::PreInit(256, 4096);

    DataArray *cfg = DataReadString(kBeatmatcherDta);
    gSystemConfig = cfg;

    printf("=== rb3-xenon native M3a: beatmatch gem pipeline ===\n");
    printf("mid: %s\n\n", midPath);

    NativeSongInfo songInfo;
    NativeGemSink sink;
    TempoMap *tempoMap = nullptr;   // filled by MidiReader::OnAcceptMaps
    MeasureMap *measureMap = nullptr;

    // --- Stage 1: SongParser reads the .mid and produces real gems ----------
    printf("--- Stage 1: SongParser -> GameGemDB gem parse ---\n");
    {
        FileStream fs(midPath, FileStream::kRead, false); // big-endian for MIDI
        SongParser parser(sink, kNumDifficulties, tempoMap, measureMap, 2);
        parser.ReadMidiFile(fs, midPath, &songInfo);
        int pumps = 0;
        // Poll drains one MidiReader pass at a time (beat / non-parts / parts);
        // after the parts pass the parser drops mFile and NoMidiReader() goes true.
        while (!parser.NoMidiReader() && pumps < 2000000) {
            parser.Poll();
            pumps++;
        }
        printf("  drove SongParser to completion in %d Poll() pump(s).\n", pumps);
    }
    // Finalize each track's gem lists (merge chord gems, shrink) — the game does
    // this in SongData::OnEndOfTrack; drive it directly for every parsed track.
    for (size_t i = 0; i < sink.mTracks.size(); i++)
        if (sink.mTracks[i].db) {
            sink.mTracks[i].db->MergeChordGems();
            sink.mTracks[i].db->Finalize();
        }

    printf("  tempoMap=%p measureMap=%p\n", (void *)tempoMap, (void *)measureMap);
    printf("  parsed %d track(s), %d total gem(s) across all difficulties:\n",
           (int)sink.mTracks.size(), sink.mGemCount);
    for (size_t i = 0; i < sink.mTracks.size(); i++) {
        const NativeGemSink::TrackRec &t = sink.mTracks[i];
        if (!t.db)
            continue;
        printf("    track %zu '%-8s' (type %d): ", i,
               t.name.Str() ? t.name.Str() : "?", (int)t.type);
        for (int d = 0; d < kNumDifficulties; d++)
            printf("diff%d=%d ", d, t.db->GetDiffGemList(d)->NumGems());
        printf("\n");
    }

    // --- Pick a track to detail: prefer PART BASS, else the first ----------
    Symbol bassSym("PART BASS");
    GameGemList *detail = sink.ExpertList(bassSym);
    Symbol detailName = bassSym;
    if (!detail || detail->NumGems() == 0) {
        for (size_t i = 0; i < sink.mTracks.size(); i++) {
            if (sink.mTracks[i].db) {
                GameGemList *gl =
                    sink.mTracks[i].db->GetDiffGemList(kNumDifficulties - 1);
                if (gl && gl->NumGems() > 0) {
                    detail = gl;
                    detailName = sink.mTracks[i].name;
                    break;
                }
            }
        }
    }

    printf("\n--- Expert gem list for part '%s' ---\n",
           detailName.Str() ? detailName.Str() : "?");
    if (detail) {
        int n = detail->NumGems();
        printf("  total gems: %d\n", n);
        int show = n < 10 ? n : 10;
        printf("  %-4s %8s %10s %8s %8s %8s\n", "#", "tick", "ms", "slots", "dur_ms",
               "dur_tk");
        for (int i = 0; i < show; i++) {
            GameGem &g = detail->GetGem(i);
            printf("  %-4d %8d %10.2f  0x%04x %8d %8d\n", i, g.GetTick(), g.GetMs(),
                   g.GetSlots(), (int)g.DurationMs(), g.GetDurationTicks());
        }
    } else {
        printf("  (no gems parsed)\n");
    }

    // --- Stage 2: fire MidiParser's previously-skipped gem branch -----------
    // Hand the populated sink to MidiParserMgr as its GemListInterface. On each
    // track's OnEndOfTrack the mgr calls mGems->SetTrack(name) + ParseAll(mGems),
    // and MidiParser's gem branch (mGems->GetGem) re-emits gems as DataEvents.
    printf("\n--- Stage 2: MidiParser gem branch via GemListInterface ---\n");
    {
        // A gem-typedef MidiParser for PART BASS. The "(gem ...)" array sets
        // mGemParser, which is what gates the ParseAll gem branch.
        MidiParser *gemParser = new MidiParser();
        gemParser->SetName("gem_parser", ObjectDir::Main());
        DataArray *td =
            DataReadString("(track_name 'PART BASS') (gem {$this add_message $mp.data})");
        gemParser->SetTypeDef(td);
        td->Release();

        MidiParserMgr mgr(&sink, Symbol("nativetest"));
        {
            FileStream fs(midPath, FileStream::kRead, false);
            MidiReader reader(fs, mgr, midPath);
            reader.ReadAllTracks();
        }
        mgr.FinishLoad();

        int emitted = gemParser->Events()->Size();
        printf("  MidiParser gem branch fired: %d gem DataEvent(s) re-emitted from\n",
               emitted);
        printf("  the GemListInterface for 'PART BASS'.\n");
        int show = emitted < 8 ? emitted : 8;
        for (int i = 0; i < show; i++) {
            const DataEvent &e = gemParser->Events()->Event(i);
            String msg;
            if (e.Msg())
                e.Msg()->Print(msg, kDataArray, false);
            printf("    gem_event[%d] start=%-9.3f end=%-9.3f  %s\n", i, e.Start(),
                   e.End(), e.Msg() ? msg.c_str() : "(null)");
        }
    }

    printf("\nDone.\n");
    return 0;
}
