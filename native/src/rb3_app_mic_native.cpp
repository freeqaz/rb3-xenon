// micwalk: an opt-in synthetic microphone for rb3-app that SINGS the chart.
//
// RB3_APP_SUNG_MIC=1 gives rb3-app's Synth (Rb3AppNativeSynth,
// rb3_app_native_glue.cpp) one connected mic, mic 0, whose capture buffers hold
// a synthesised voice singing the active VocalPlayer's part-0 notes: pitch and
// timing from the song's MIDI as the game parsed it (VocalNoteList::PitchAt at
// the singer's compensated song time, the same lookup retail autoplay makes in
// Singer::Poll_). The game then scores vocals through its real mic path:
// MicClientMapper -> GameMicManager::CreateMic -> GameMic::ThreadProcessOneFrame
// -> PitchDetector::AnalyzeBlock (the VMX ShiftedDotProduct fast path) ->
// Singer::Poll_ -> VocalPart scoring. Nothing in that path knows the signal is
// synthetic.
//
//   RB3_APP_SUNG_MIC=1               enable (unset: rb3-app has no mic, as before)
//   RB3_APP_SUNG_MIC_DETUNE=<semis>  sing every note this many semitones off (default 0)
//   RB3_APP_SUNG_MIC_LOG=1           one `rb3-app-mic:` line per analysed window
//                                    (sung pitch vs the detector's answer)
//   RB3_APP_SUNG_MIC_TAMB=1          (micwalk2) tap the mic on every tambourine gem:
//                                    a short loud burst starting at the gem's time,
//                                    which TambourineDetector::CheckForSwing sees as an
//                                    energy rise then drop
//   RB3_APP_SUNG_MIC_FREESTYLE=1     (micwalk2) sing a loud tone through every freestyle
//                                    section of part 0 (VocalNoteList::mFreestyleSections),
//                                    i.e. deploy overdrive by mic energy
//                                    (Singer::AddToFreestyleDeployment) when it is full
//   RB3_APP_SUNG_MIC_COUNT=<1..3>    (micwalk2) connect this many mics; mic k sings vocal
//                                    part k (a harmony walk), silent when the song has
//                                    fewer parts. Tambourine taps and freestyle tones
//                                    come from mic 0 only. Default 1.
//   RB3_APP_SUNG_MIC_UNPITCHED=0|1   (micwalk2) 0: stay silent on unpitched (spoken)
//                                    notes, as a control; default 1 sings them at their
//                                    nominal pitch like any other note
//
// Once per note list, a `rb3-app-mic: song …` line summarises part 0 (notes,
// unpitched notes, phrases, tambourine gems, freestyle sections) and the
// scoring.vocals thresholds the mic paths are judged by.
//
// The device is modelled on retail's Xbox mic (MicXbox, synth_xbox/Mic.cpp):
// 16 kHz (GetSampleRate 16000), GetRecentBuf returns the most recent 0x600
// samples, sensitivity and gain 1.0 (MicXbox's ctor values). The signal is a
// pure function of song time, so RB3_APP_DETERMINISTIC walks stay reproducible.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "beatmatch/VocalNote.h"
#include "game/Game.h"
#include "game/GameMic.h"
#include "game/GameMicManager.h"
#include "game/Player.h"
#include "game/VocalPart.h"
#include "game/VocalPlayer.h"
#include "game/TambourineManager.h"
#include "obj/Data.h"
#include "os/System.h"
#include "utl/TimeConversion.h"
#include "obj/Task.h"
#include "synth/Mic.h"

namespace {

const int kRate = 16000;      // MicXbox::GetSampleRate
const int kRecent = 0x600;    // MicXbox::GetRecentBuf's count
const int kContMax = 0x2000;  // GameMic's cap on the continuous buffer
const float kAmp = 4000.0f;   // fundamental amplitude (peak of the sum ~8300)

bool gOn = false, gLog = false;
float gDetune = 0.0f;
bool gTamb = false, gFreestyle = false, gUnpitched = true;
const float kTapMs = 60.0f;        // a tambourine tap's burst length
const float kTapPitch = 72.0f;     // pitch of the tap burst
const float kFreestylePitch = 60.0f;
const VocalNoteList *gSummarised = nullptr;

bool EnvOn(const char *name, bool def) {
    const char *v = getenv(name);
    if (!v || !*v) return def;
    return strcmp(v, "0") != 0;
}

void Summarise(VocalPlayer *vp) {
    const VocalNoteList *nl = vp->mVocalParts[0]->mVocalNoteList;
    if (nl == gSummarised) return;
    gSummarised = nl;
    int unp = 0;
    for (size_t i = 0; i < nl->mNotes.size(); i++)
        if (nl->mNotes[i].mUnpitchedNote) unp++;
    DataArray *cfg = SystemConfig("scoring", "vocals");
    printf("rb3-app-mic: song notes %d unpitched %d phrases %d tamb_gems %d freestyle %d "
           "nonpitch_thr %.3f tamb_rise %.3f tamb_drop %.3f tamb_ms_offset %d tamb_window_ticks %d "
           "scream_thr %.3f\n",
           (int)nl->mNotes.size(), unp, (int)nl->mPhrases.size(), (int)nl->mTambourineGems.size(),
           (int)nl->mFreestyleSections.size(), cfg->FindFloat("nonpitch_energy_threshold"),
           cfg->FindFloat("tambourine_energy_rise_threshold"),
           cfg->FindFloat("tambourine_energy_drop_threshold"), cfg->FindInt("tambourine_ms_offset"),
           cfg->FindInt("tambourine_window_ticks"), cfg->FindFloat("scream_energy_threshold"));
}

// The active local VocalPlayer, or null.
VocalPlayer *FindVocalPlayer() {
    if (!TheGame) return nullptr;
    std::vector<Player *> &players = TheGame->GetActivePlayers();
    for (size_t i = 0; i < players.size(); i++) {
        VocalPlayer *vp = dynamic_cast<VocalPlayer *>(players[i]);
        if (vp && vp->IsLocal() && !vp->mVocalParts.empty() && vp->mVocalParts[0]->mVocalNoteList)
            return vp;
    }
    return nullptr;
}

class Rb3AppSungMic : public Mic {
public:
    explicit Rb3AppSungMic(int part) : mPart(part), mRunning(false), mSensitivity(1.0f), mGain(1.0f), mOutGain(1.0f),
                      mLastContMs(-1.0f), mPhase(0.0), mLastSung(0.0f), mWindows(0) {
        memset(mRecent, 0, sizeof mRecent);
        memset(mCont, 0, sizeof mCont);
    }
    virtual void Start() { mRunning = true; }
    virtual void Stop() { mRunning = false; }
    // Synth::SetMic / Singer::Poll ask IsRunning before Update; a USB mic that is
    // plugged in reads as running (MicXbox: mRunning, set by Start). Nothing in
    // rb3-app's path calls Start on mic 0, so report running while enabled.
    virtual bool IsRunning() const { return true; }
    // Retail MicXbox::GetType (0x82B5... synth_xbox/Mic.cpp): type 2 while connected.
    virtual Type GetType() const { return kMicNull; }
    virtual void SetDMA(bool) {}
    virtual bool GetDMA() const { return false; }
    virtual void SetGain(float g) { mGain = g; }
    virtual float GetGain() const { return mGain; }
    virtual void SetEarpieceVolume(float) {}
    virtual float GetEarpieceVolume() const { return 0.0f; }
    virtual bool GetClipping() const { return false; }
    virtual void SetOutputGain(float g) { mOutGain = g; }
    virtual float GetOutputGain() const { return mOutGain; }
    virtual void SetSensitivity(float s) { mSensitivity = s; }
    virtual float GetSensitivity() const { return mSensitivity; }
    virtual void SetCompressor(bool) {}
    virtual bool GetCompressor() const { return false; }
    virtual void SetCompressorParam(float) {}
    virtual float GetCompressorParam() const { return 0.0f; }
    virtual int GetSampleRate() const { return kRate; }

    virtual short *GetRecentBuf(int &n) {
        float nowMs = SongMs();
        LogPrev(nowMs);
        // The 0x600 samples ending at nowMs.
        Render(mRecent, kRecent, nowMs - kRecent * 1000.0f / kRate, true);
        n = kRecent;
        return mRecent;
    }

    virtual short *GetContinuousBuf(int &n) {
        float nowMs = SongMs();
        int count = 0;
        if (mLastContMs >= 0.0f && nowMs > mLastContMs)
            count = (int)((nowMs - mLastContMs) * kRate / 1000.0f);
        if (count > kContMax) count = kContMax;
        if (count > 0) Render(mCont, count, nowMs - count * 1000.0f / kRate, false);
        mLastContMs = nowMs;
        n = count;
        return mCont;
    }

private:
    static float SongMs() { return TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f; }

    // The pitch a singer on time would be singing at song time ms (0 = silent).
    float SungPitch(VocalPlayer *vp, float ms) const {
        if (!vp || mPart >= (int)vp->mVocalParts.size()) return 0.0f;
        const VocalNoteList *nl = vp->mVocalParts[mPart]->mVocalNoteList;
        if (!nl) return 0.0f;
        float t = vp->GetCompensatedTime(ms);
        float p = nl->PitchAt(t);
        if (p != 0.0f && !gUnpitched) {
            const VocalNote *n = nl->NoteAt(t);
            if (n && n->mUnpitchedNote) p = 0.0f;
        }
        if (p != 0.0f) return p + gDetune;
        if (gTamb && mPart == 0) {
            // A tap: kTapMs of tone from each gem's time (gems are sorted ticks).
            const std::vector<int> &g = nl->mTambourineGems;
            std::vector<int>::const_iterator it =
                std::upper_bound(g.begin(), g.end(), MsToTickInt(t));
            if (it != g.begin()) {
                float gms = TickToMs(*(it - 1));
                if (t >= gms && t < gms + kTapMs) return kTapPitch;
            }
        }
        if (gFreestyle && mPart == 0) {
            for (size_t i = 0; i < nl->mFreestyleSections.size(); i++) {
                const std::pair<float, float> &fs = nl->mFreestyleSections[i];
                if (t >= fs.first && t < fs.second) return kFreestylePitch;
            }
        }
        return 0.0f;
    }

    // n samples starting at song time startMs: a harmonic tone (1, 1/2, 1/3, 1/4)
    // at the sung pitch, silence where no note is sung. The recent window restarts
    // its phase each call (it overlaps the previous one); the continuous stream
    // carries its phase across calls.
    void Render(short *out, int n, float startMs, bool recent) {
        VocalPlayer *vp = FindVocalPlayer();
        if (vp && mPart == 0) Summarise(vp);
        double phase = recent ? 0.0 : mPhase;
        for (int i = 0; i < n; i++) {
            float ms = startMs + i * 1000.0f / kRate;
            float p = SungPitch(vp, ms);
            if (p == 0.0f) {
                out[i] = 0;
                continue;
            }
            double f = 440.0 * pow(2.0, (p - 69.0) / 12.0);
            phase += 2.0 * M_PI * f / kRate;
            if (phase > 2.0 * M_PI * 64) phase = fmod(phase, 2.0 * M_PI);
            double s = sin(phase) + sin(2 * phase) / 2 + sin(3 * phase) / 3 + sin(4 * phase) / 4;
            out[i] = (short)lrint(kAmp * s);
        }
        if (!recent) mPhase = phase;
        else mLastSung = SungPitch(vp, startMs + n * 1000.0f / kRate);
    }

    // GetRecentBuf runs at the top of GameMic::ThreadProcessOneFrame, so the
    // GameMic still holds the detector's answer for the previous window.
    void LogPrev(float nowMs) {
        if (!gLog || !TheGameMicManager) return;
        GameMic *gm = TheGameMicManager->GetMic(MicClientID(mPart, -1));
        if (gm && (mLastSung != 0.0f || gm->mPitch > 0.0f)) {
            if (mPart == 0)
                printf("rb3-app-mic: ms %.1f sung %.3f det %.3f energy %.4f\n", nowMs, mLastSung,
                       gm->mPitch, gm->mEnergy);
            else
                printf("rb3-app-mic%d: ms %.1f sung %.3f det %.3f energy %.4f\n", mPart, nowMs,
                       mLastSung, gm->mPitch, gm->mEnergy);
        }
        mWindows++;
    }

    int mPart;
    bool mRunning;
    float mSensitivity, mGain, mOutGain, mLastContMs;
    double mPhase;
    float mLastSung;
    long mWindows;
    short mRecent[kRecent];
    short mCont[kContMax];
};

const int kMaxMics = 3;
int gCount = 1;
Rb3AppSungMic *gMics[kMaxMics] = {};
bool gCaptured[kMaxMics] = {};
bool gChanged = false;

} // namespace

// Read the knobs once (Rb3AppNativeSynth's ctor). Returns whether the mic is on.
bool rb3_app_sung_mic_init() { // ring: native-real
    static bool done = false;
    if (done) return gOn;
    done = true;
    const char *v = getenv("RB3_APP_SUNG_MIC");
    gOn = v && *v && strcmp(v, "0") != 0;
    if (!gOn) return false;
    if (const char *d = getenv("RB3_APP_SUNG_MIC_DETUNE")) gDetune = (float)atof(d);
    gLog = EnvOn("RB3_APP_SUNG_MIC_LOG", false);
    gTamb = EnvOn("RB3_APP_SUNG_MIC_TAMB", false);
    gFreestyle = EnvOn("RB3_APP_SUNG_MIC_FREESTYLE", false);
    gUnpitched = EnvOn("RB3_APP_SUNG_MIC_UNPITCHED", true);
    if (const char *c = getenv("RB3_APP_SUNG_MIC_COUNT")) gCount = atoi(c);
    if (gCount < 1) gCount = 1;
    if (gCount > kMaxMics) gCount = kMaxMics;
    for (int i = 0; i < gCount; i++) gMics[i] = new Rb3AppSungMic(i);
    gChanged = true; // the first Synth::Poll runs MicClientMapper::HandleMicsChanged
    printf("rb3-app-mic: sung mic on (%d mic(s), %d Hz, detune %.2f semitones, tamb %d freestyle %d "
           "unpitched %d)\n", gCount, kRate, gDetune, gTamb, gFreestyle, gUnpitched);
    return true;
}

// Rb3AppNativeSynth's mic virtuals delegate here while the mic is on.
// Mic ids are 0..count-1; MicClientMapper::RefreshMics captures them in that
// order, so client k (Singer k's MicClientID(k, -1)) gets mic k.
Mic *rb3_app_sung_mic(int id) { // ring: native-real
    return id >= 0 && id < gCount ? gMics[id] : nullptr;
}
int rb3_app_sung_mic_count() { return gOn ? gCount : 0; } // ring: native-real
int rb3_app_sung_mic_next_available() { // ring: native-real
    for (int i = 0; i < gCount; i++)
        if (gMics[i] && !gCaptured[i]) return i;
    return -1;
}
void rb3_app_sung_mic_capture(int id, bool on) { // ring: native-real
    if (id >= 0 && id < gCount) gCaptured[id] = on;
}
bool rb3_app_sung_mic_changed() { return gChanged; } // ring: native-real
void rb3_app_sung_mic_reset_changed() { gChanged = false; } // ring: native-real
