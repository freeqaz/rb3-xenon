// rb3-xenon native M10 — support layer for the full vocal-gameplay orchestration.
//
// Brings up the pieces the REAL VocalPlayer::Poll -> Singer::Poll -> VocalPart::
// ScoreSinger per-frame pipeline needs that are otherwise supplied by heavy
// singleton graphs (Synth/Fonix mic stack, BandUser, VocalTrack render). Every
// definition here is native-only harness scaffolding; none is compiled for X360
// (rb3-vocal2 is a native target). The contract mirrors M8/M9: only the *mic input*
// is synthetic; all pitch matching / scoring / rating is REAL engine code.
//
//   * synthetic GameMic + GameMicManager — the injection point. Singer::Poll reads
//     GameMic::unk2c (pitch) / unk28 (energy); the driver writes those each frame.
//     GameMic.cpp / GameMicManager.cpp are NOT compiled (they pull TheSynth /
//     MicClientMapper / FxSend). We define exactly the methods the vocal Poll path
//     calls, backed by driver globals (layout-safe on a calloc'd manager, the m8
//     calloc'd-Game pattern).
//   * NativeMic — a concrete Mic whose IsRunning() is true so Singer::Poll takes the
//     real-mic branch (mic->unk2c/unk28) instead of the silent branch.
#include "game/GameMic.h"
#include "game/GameMicManager.h"
#include "game/SongDB.h"
#include "game/GameConfig.h"
#include "beatmatch/SongData.h"
#include "beatmatch/VocalNote.h"
#include "net/NetSession.h"
#include "synth/Mic.h"
#include "synth/MicManagerInterface.h"

#include <vector>
#include <cstdlib>
#include <cstring>

// ==================================== Poll-path decomp-mangled shims ==========
// VocalPart.cpp calls VocalNoteList::NoteAt / ::PitchAt through their MWCC X360
// symbol names (so the retail obj links against the real methods). Native (Itanium
// ABI) has no such symbols; forward the extern "C" name to the real C++ method.
// VocalPlayer::kInvalidPitch (a static sentinel for "no pitch difference") equals
// 1000.0f — the same value VocalPlayer::Poll tests fDev against (`1000.0f != fDev`).
extern "C" VocalNote *NoteAt__13VocalNoteListCFf(const VocalNoteList *self, float ms) {
    return const_cast<VocalNote *>(self->NoteAt(ms));
}
extern "C" float PitchAt__13VocalNoteListCFf(const VocalNoteList *self, float ms) {
    return self->PitchAt(ms);
}
extern "C" float kInvalidPitch__11VocalPlayer = 1000.0f;

// ------------------------------------------------------------- driver state --
// The synthetic mic bank, keyed by MicClientID.mClientID (== singer index). The
// driver sets gNativeMicPitch/Energy[i] each frame before VocalPlayer::Poll.
std::vector<GameMic *> gNativeMics;
int gNativeMicCount = 0;

// =============================================================== NativeMic ====
// Concrete Mic with IsRunning()==true. All other virtuals are trivial (no real
// capture device); the DSP that consumes samples is the real TalkyMatcher, fed
// GameMic::mSamplesContinuous by the driver.
class NativeMic : public Mic {
public:
    NativeMic() {}
    void Start() {}
    void Stop() {}
    bool IsRunning() const { return true; }
    Type GetType() const { return (Type)0; }
    void SetDMA(bool) {}
    bool GetDMA() const { return false; }
    void SetGain(float) {}
    float GetGain() const { return 1.0f; }
    void SetEarpieceVolume(float) {}
    float GetEarpieceVolume() const { return 0.0f; }
    bool GetClipping() const { return false; }
    void SetOutputGain(float) {}
    float GetOutputGain() const { return 1.0f; }
    void SetSensitivity(float) {}
    float GetSensitivity() const { return 1.0f; }
    void SetCompressor(bool) {}
    bool GetCompressor() const { return false; }
    void SetCompressorParam(float) {}
    float GetCompressorParam() const { return 0.0f; }
    short *GetRecentBuf(int &n) { n = 0; return 0; }
    short *GetContinuousBuf(int &n) { n = 0; return 0; }
    int GetSampleRate() const { return 16000; }
};

// =============================================================== GameMic ======
// GameMic.cpp does not exist in-tree; provide native definitions of the members
// the vocal path touches. The ctor zeroes the struct and installs a NativeMic.
GameMic::GameMic(int micID) {
    std::memset(this, 0, sizeof(GameMic));
    mMicID = micID;
    mNullMic = new NativeMic();
}
GameMic::~GameMic() {}

Mic *GameMic::GetMyMic() { return mNullMic; }
void GameMic::Update() {} // driver writes unk2c/unk28 directly each frame
void GameMic::SetEnablePitchDetection(bool) {}
void GameMic::SetInputFile(const char *) {}
int GameMic::GetDataSampleRate() { return 16000; }

// ProcessTalkyData feeds these samples to the REAL TalkyMatcher. The driver leaves
// mSamplesContinuous zeroed (silence) unless it injects a waveform; unk8034 is the
// continuous sample count.
void GameMic::AccessContinuousSamples(const short *&samples, int &count) const {
    samples = mSamplesContinuous;
    count = unk8034;
}

// ========================================================== GameMicManager ====
// TheGameMicManager is a calloc'd manager; the methods the Poll path calls read
// driver globals only (never manager members), so the zeroed object is safe. The
// heavy Synth/FxSend methods are never reached.
GameMicManager *TheGameMicManager = 0;

GameMic *GameMicManager::GetMic(const MicClientID &id) {
    if (id.mClientID < 0 || (unsigned)id.mClientID >= gNativeMics.size())
        return 0;
    return gNativeMics[id.mClientID];
}
bool GameMicManager::HasMic(const MicClientID &id) const {
    return id.mClientID >= 0 && (unsigned)id.mClientID < gNativeMics.size()
        && gNativeMics[id.mClientID];
}
int GameMicManager::GetMicCount() const { return gNativeMicCount; }
void GameMicManager::SetOverdriveEffectEnable(bool) {}
void GameMicManager::Poll(float) {}
void GameMicManager::SetPitchCorrectionTarget(bool, bool, float, float, float, float, float) {}
void GameMicManager::SetPlayback(bool) {}
void GameMicManager::HandleMicsChanged() {}
float GameMicManager::GetEnergyForMic(const MicClientID &id) {
    GameMic *m = GetMic(id);
    return m ? m->unk28 : 0.0f;
}

// Build a calloc'd manager + N synthetic mics keyed by singer index. Returns the
// manager to assign to TheGameMicManager.
GameMicManager *NativeMakeGameMicManager(int nSingers) {
    GameMicManager *mgr = (GameMicManager *)std::calloc(1, sizeof(GameMicManager));
    gNativeMics.clear();
    gNativeMicCount = nSingers;
    for (int i = 0; i < nSingers; i++)
        gNativeMics.push_back(new GameMic(-1));
    return mgr;
}

// Per-frame injection: set the synthetic pitch (MIDI note, 0 = silence) and energy
// for singer i.
void NativeSetMicFrame(int i, float pitch, float energy) {
    if (i >= 0 && (unsigned)i < gNativeMics.size()) {
        gNativeMics[i]->unk2c = pitch;
        gNativeMics[i]->unk28 = energy;
    }
}

// M11: per-frame CONTINUOUS-waveform injection for the REAL TalkyMatcher path.
// Singer::ProcessTalkyData -> GameMic::AccessContinuousSamples -> TalkyMatcher::
// Analyze -> VoiceBeat DSP reads mSamplesContinuous[0..unk8034). The driver writes
// a synthetic voiced burst here so the real syllable-onset detector scores the
// chart's unpitched (talky) notes. count is clamped to the 8192-short buffer.
void NativeSetMicSamples(int i, const short *buf, int count) {
    if (i < 0 || (unsigned)i >= gNativeMics.size()) return;
    GameMic *m = gNativeMics[i];
    if (count < 0) count = 0;
    if (count > 8192) count = 8192;
    if (buf && count > 0)
        std::memcpy(m->mSamplesContinuous, buf, count * sizeof(short));
    m->unk8034 = count;
}

// ============================================= SongDB vocal query overrides ===
// The vocal Poll path resolves note lists / pitch offsets through TheSongDB. These
// delegate to the real parsed SongData (the base m8_support.cpp provides the SongDB
// ctor + mSongData wiring). NOTE: m8_link_stubs.cpp's GetVocalNoteListCount()->0
// stub is intentionally NOT linked into rb3-vocal2 (see m10_link_stubs.cpp).
int SongDB::GetVocalNoteListCount() const {
    return mSongData ? mSongData->GetVocalNoteListCount() : 0;
}
VocalNoteList *SongDB::GetVocalNoteList(int i) const {
    return mSongData ? mSongData->GetVocalNoteList(i) : 0;
}
float SongDB::GetPitchOffsetForTick(int) const { return 0.0f; }
void SongDB::OverrideBasePoints(int, TrackType, const UserGuid &, int, int, int) {}

// ==================================================== other singletons =========
// TheNetSession / TheGameConfig globals live in m6_symbols.cpp (both null). The
// driver points TheNetSession at a calloc'd NetSession; NetSession::IsLocal() is
// stubbed to return true (offline single-player) in m10_link_stubs.cpp so
// VocalPlayer::Poll's `!TheNetSession->IsLocal()` chat gate short-circuits before
// dereferencing the (absent) BandUser. TheGameConfig stays null (only the
// HX_NATIVE-compiled-out spotlight path would read it).
NetSession *NativeMakeNetSession() {
    return (NetSession *)std::calloc(1, sizeof(NetSession));
}
