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
//
// The device is modelled on retail's Xbox mic (MicXbox, synth_xbox/Mic.cpp):
// 16 kHz (GetSampleRate 16000), GetRecentBuf returns the most recent 0x600
// samples, sensitivity and gain 1.0 (MicXbox's ctor values). The signal is a
// pure function of song time, so RB3_APP_DETERMINISTIC walks stay reproducible.
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
#include "obj/Task.h"
#include "synth/Mic.h"

namespace {

const int kRate = 16000;      // MicXbox::GetSampleRate
const int kRecent = 0x600;    // MicXbox::GetRecentBuf's count
const int kContMax = 0x2000;  // GameMic's cap on the continuous buffer
const float kAmp = 4000.0f;   // fundamental amplitude (peak of the sum ~8300)

bool gOn = false, gLog = false;
float gDetune = 0.0f;

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
    Rb3AppSungMic() : mRunning(false), mSensitivity(1.0f), mGain(1.0f), mOutGain(1.0f),
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
    static float SungPitch(VocalPlayer *vp, float ms) {
        if (!vp) return 0.0f;
        float p = vp->mVocalParts[0]->mVocalNoteList->PitchAt(vp->GetCompensatedTime(ms));
        return p != 0.0f ? p + gDetune : 0.0f;
    }

    // n samples starting at song time startMs: a harmonic tone (1, 1/2, 1/3, 1/4)
    // at the sung pitch, silence where no note is sung. The recent window restarts
    // its phase each call (it overlaps the previous one); the continuous stream
    // carries its phase across calls.
    void Render(short *out, int n, float startMs, bool recent) {
        VocalPlayer *vp = FindVocalPlayer();
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
        GameMic *gm = TheGameMicManager->GetMic(MicClientID(0, -1));
        if (gm && (mLastSung != 0.0f || gm->mPitch > 0.0f)) {
            printf("rb3-app-mic: ms %.1f sung %.3f det %.3f energy %.4f\n", nowMs, mLastSung,
                   gm->mPitch, gm->mEnergy);
        }
        mWindows++;
    }

    bool mRunning;
    float mSensitivity, mGain, mOutGain, mLastContMs;
    double mPhase;
    float mLastSung;
    long mWindows;
    short mRecent[kRecent];
    short mCont[kContMax];
};

Rb3AppSungMic *gMic = nullptr;
bool gCaptured = false, gChanged = false;

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
    const char *l = getenv("RB3_APP_SUNG_MIC_LOG");
    gLog = l && *l && strcmp(l, "0") != 0;
    gMic = new Rb3AppSungMic();
    gChanged = true; // the first Synth::Poll runs MicClientMapper::HandleMicsChanged
    printf("rb3-app-mic: sung mic on (mic 0, %d Hz, detune %.2f semitones)\n", kRate, gDetune);
    return true;
}

// Rb3AppNativeSynth's mic virtuals delegate here while the mic is on.
Mic *rb3_app_sung_mic() { return gMic; } // ring: native-real
int rb3_app_sung_mic_next_available() { return gMic && !gCaptured ? 0 : -1; } // ring: native-real
void rb3_app_sung_mic_capture(int id, bool on) { // ring: native-real
    if (id == 0) gCaptured = on;
}
bool rb3_app_sung_mic_changed() { return gChanged; } // ring: native-real
void rb3_app_sung_mic_reset_changed() { gChanged = false; } // ring: native-real
