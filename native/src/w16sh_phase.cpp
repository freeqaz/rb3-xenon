// rb3-xenon native -- W16-SH: gates for code that first runs on host here.
//
// W16-SH linked 83 of the 96 in-scope band3/bandobj/synth/dsp files that no
// native target compiled (CAMPAIGN_STATE_2026-10-07 lever 5) into rb3-render.
// Linking alone runs only their static initializers (ProfileMgr's is the one
// with logic). This phase calls into the files whose gap rows have an oracle
// that does not come from the code under test:
//
//   dsp-pitch-*    PitchDetector::AnalyzeBlock and its SndAnalysis helpers
//                  (FindCCPeak, ShiftedDotProduct, RefinePeriod2) on a
//                  synthesized sine, against 69 + 12*log2(f/440).
//   fx-eq-*        EQEffect::SetParameter/Process: a disabled EQ is the
//                  identity, and each band's DC / Nyquist gain is what a
//                  first-order shelf or an RBJ biquad must give (1, 10^(dB/20),
//                  0), measured on a constant and on an alternating signal.
//   fx-delay-*     DelayEffect (mono): an impulse comes back after
//                  delay*48000 samples scaled by 10^(dB/20), then squared.
//   fx-distort-*   DistortionEffect: drive 0 is the identity; any drive keeps
//                  +-1 at +-1, is odd, monotone, and adds gain below full scale.
//   fx-flanger-*   FlangerEffect with depth 0 and no feedback is a pure delay
//                  of delayMs*48 samples (mono).
//   pm-*           ProfileMgr, now the real TU, constructed by static init:
//                  its lag table equals GetJoypadExtraLagInits cell by cell,
//                  three cells equal retail's decoded constants, and the sync
//                  offset follows the video latency.
//   tracker-*      TrackerMultiplierMap::InitFromDataArray over a DTA string,
//                  read back by threshold.
//
// Each gate can fail. See docs/decomp/W16SH_NATIVE_LINK_96_2026-10-07.md.

#include "game/TrackerUtils.h"
#include "meta_band/ProfileMgr.h"
#include "dsp/PitchDetector.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "synth/DelayEffect.h"
#include "synth/DistortionEffect.h"
#include "synth/EQEffect.h"
#include "synth/FlangerEffect.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <vector>

namespace {

typedef void (*GateFn)(const char *, bool, const char *);
GateFn gGate = nullptr;
char gBuf[512];

void Gate(const char *name, bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Gate(const char *name, bool ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gBuf, sizeof(gBuf), fmt, ap);
    va_end(ap);
    gGate(name, ok, gBuf);
}

double MidiOf(double hz) { return 69.0 + 12.0 * std::log2(hz / 440.0); }

// Feed `blocks` blocks of a sine at `hz` (amplitude `amp`, 16-bit) and return
// the last pitch AnalyzeBlock reports.
float DetectPitch(double hz, double amp, int blocks, float *gateOut) {
    const int kRate = 48000, kBlock = 1024;
    PitchDetector pd(kRate);
    std::vector<short> buf(kBlock);
    float pitch = 0, conf = 0, gate = 0;
    long n = 0;
    for (int b = 0; b < blocks; b++) {
        for (int i = 0; i < kBlock; i++, n++)
            buf[i] = (short)std::lround(amp * std::sin(2.0 * M_PI * hz * n / kRate));
        pd.AnalyzeBlock("w16sh", buf.data(), kBlock, 1.0f, 1.0f, pitch, conf, gate);
    }
    if (gateOut)
        *gateOut = gate;
    return pitch;
}

void PitchGates() {
    const double hzs[3] = { 110.0, 220.0, 440.0 };
    const char *names[3] = { "dsp-pitch-110hz", "dsp-pitch-220hz", "dsp-pitch-440hz" };
    for (int k = 0; k < 3; k++) {
        float g = 0;
        float p = DetectPitch(hzs[k], 12000.0, 40, &g);
        double want = MidiOf(hzs[k]);
        Gate(names[k], std::fabs(p - want) <= 0.25, "pitch %.3f, want %.3f (+-0.25 semitone), level %.2f",
             p, want, g);
    }
    float g = 0;
    float p = DetectPitch(220.0, 0.0, 40, &g);
    Gate("dsp-pitch-silence", p == 0.0f && g == 0.0f, "silence: pitch %.3f level %.3f (want 0, 0)", p,
         g);
}

// Steady-state gain of `fx` on a constant (dc=true) or an alternating +-1
// (Nyquist) mono signal, measured after the filter settles.
template <class Fx>
double SteadyGain(Fx &fx, bool dc) {
    const int kN = 4096;
    std::vector<float> s(kN);
    for (int i = 0; i < kN; i++)
        s[i] = dc ? 0.5f : ((i & 1) ? -0.5f : 0.5f);
    fx.Process(s.data(), kN, 1);
    double sum = 0;
    for (int i = kN - 256; i < kN; i++)
        sum += std::fabs(s[i]);
    return sum / 256 / 0.5;
}

bool Close(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

void EQGates() {
    {
        EQEffect eq(nullptr);
        std::vector<float> s(512), orig(512);
        for (int i = 0; i < 512; i++)
            orig[i] = s[i] = (float)std::sin(i * 0.37) * 0.7f;
        eq.Process(s.data(), 512, 1);
        bool same = true;
        for (int i = 0; i < 512; i++)
            same = same && s[i] == orig[i];
        Gate("fx-eq-identity", same, "all bands disabled: output %s input", same ? "==" : "!=");
    }
    {
        // params 0/1: the first-order shelf whose boost sits at the TOP of the
        // band (its default corner is 12 kHz). +6 dB: DC 1, Nyquist 10^(6/20).
        EQEffect eq(nullptr);
        eq.SetParameter(0, 2000.0f);
        eq.SetParameter(1, 6.0f);
        double dc = SteadyGain(eq, true);
        eq.Reset();
        double ny = SteadyGain(eq, false);
        double want = std::pow(10.0, 6.0 / 20.0);
        Gate("fx-eq-treble-shelf", Close(dc, 1.0, 0.01) && Close(ny, want, 0.02),
             "+6 dB at 2 kHz: DC gain %.4f (want 1), Nyquist gain %.4f (want %.4f)", dc, ny, want);
    }
    {
        // params 5/6: the shelf whose boost sits at the BOTTOM. -6 dB.
        EQEffect eq(nullptr);
        eq.SetParameter(5, 200.0f);
        eq.SetParameter(6, -6.0f);
        double dc = SteadyGain(eq, true);
        eq.Reset();
        double ny = SteadyGain(eq, false);
        double want = std::pow(10.0, -6.0 / 20.0);
        Gate("fx-eq-bass-shelf", Close(dc, want, 0.02) && Close(ny, 1.0, 0.01),
             "-6 dB at 200 Hz: DC gain %.4f (want %.4f), Nyquist gain %.4f (want 1)", dc, want, ny);
    }
    {
        // param 7: low-pass corner (enabled below 19999 Hz); an RBJ low-pass
        // passes DC at unity and has its double zero at Nyquist.
        EQEffect eq(nullptr);
        eq.SetParameter(8, 0.0f);
        eq.SetParameter(7, 1000.0f);
        double dc = SteadyGain(eq, true);
        eq.Reset();
        double ny = SteadyGain(eq, false);
        Gate("fx-eq-lowpass", Close(dc, 1.0, 0.01) && ny < 0.01,
             "1 kHz low-pass: DC gain %.4f (want 1), Nyquist gain %.5f (want 0)", dc, ny);
    }
    {
        // param 9: high-pass corner (enabled above 21 Hz): the mirror image.
        EQEffect eq(nullptr);
        eq.SetParameter(10, 0.0f);
        eq.SetParameter(9, 1000.0f);
        double dc = SteadyGain(eq, true);
        eq.Reset();
        double ny = SteadyGain(eq, false);
        Gate("fx-eq-highpass", dc < 0.01 && Close(ny, 1.0, 0.01),
             "1 kHz high-pass: DC gain %.5f (want 0), Nyquist gain %.4f (want 1)", dc, ny);
    }
}

void DelayGates() {
    DelayEffect d(nullptr);
    d.Reset();
    d.SetParameter(0, 0.01f); // 10 ms
    d.SetParameter(1, -6.0f);
    const int D = 480, kN = 3 * D + 16;
    std::vector<float> s(kN, 0.0f);
    s[0] = 1.0f;
    d.Process(s.data(), kN, 1);
    double r = std::pow(10.0, -6.0 / 20.0);
    bool quiet = true;
    for (int i = 0; i < kN; i++)
        if (i != D && i != 2 * D && i != 3 * D && s[i] != 0.0f)
            quiet = false;
    Gate("fx-delay-echo", Close(s[D], r, 1e-4) && Close(s[2 * D], r * r, 1e-4) && quiet,
         "impulse, 10 ms, -6 dB: out[%d] %.5f (want %.5f), out[%d] %.5f (want %.5f), elsewhere %s", D,
         s[D], r, 2 * D, s[2 * D], r * r, quiet ? "0" : "NONZERO");
}

void DistortionGates() {
    float xs[9] = { -1.0f, -0.75f, -0.5f, -0.25f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    {
        DistortionEffect fx(nullptr);
        float s[9];
        for (int i = 0; i < 9; i++)
            s[i] = xs[i];
        fx.Process(s, 9, 1);
        bool same = true;
        for (int i = 0; i < 9; i++)
            same = same && s[i] == xs[i];
        Gate("fx-distort-identity", same, "drive 0: output %s input", same ? "==" : "!=");
    }
    DistortionEffect::Params p;
    p.unk4 = 60.0f;
    DistortionEffect fx(nullptr);
    fx.SetParameters(p);
    float s[9];
    for (int i = 0; i < 9; i++)
        s[i] = xs[i];
    fx.Process(s, 9, 1);
    bool ends = Close(s[0], -1.0, 1e-6) && Close(s[8], 1.0, 1e-6) && s[4] == 0.0f;
    bool odd = true, mono = true, gain = true;
    for (int i = 0; i < 9; i++) {
        odd = odd && Close(s[i], -s[8 - i], 1e-6);
        if (i)
            mono = mono && s[i] > s[i - 1];
        if (xs[i] != 0 && std::fabs(xs[i]) < 1)
            gain = gain && std::fabs(s[i]) > std::fabs(xs[i]);
    }
    Gate("fx-distort-curve", ends && odd && mono && gain,
         "drive 60: f(-1,0,1) = %.4f,%.4f,%.4f; odd %d monotone %d gain-below-full-scale %d (f(0.5)=%.4f)",
         s[0], s[4], s[8], odd, mono, gain, s[6]);
}

void FlangerGates() {
    FlangerEffect fx(nullptr);
    FlangerEffect::Params p;
    p.mDelayMs = 1.0f;
    p.mRate = 0.5f;
    p.mDepth = 0.0f;
    p.mFeedback = 0;
    p.mWet = 50;
    fx.SetParameters(p);
    fx.Reset();
    const int D = 48, kN = 256;
    std::vector<float> s(kN, 0.0f);
    s[0] = 1.0f;
    fx.Process(s.data(), kN, 1);
    bool quiet = true;
    for (int i = 0; i < kN; i++)
        if (i != D && std::fabs(s[i]) > 1e-6f)
            quiet = false;
    Gate("fx-flanger-delay", Close(s[D], 1.0, 1e-5) && quiet,
         "depth 0, no feedback, 1 ms: out[%d] %.5f (want 1), elsewhere %s", D, s[D],
         quiet ? "0" : "NONZERO");
}

void ProfileMgrGates() {
    ProfileMgr &pm = TheProfileMgr;
    int bad = 0, cells = 0;
    for (int t = 0; t < 47; t++)
        for (int c = 0; c < kNumLagContexts; c++, cells++)
            if (pm.mJoypadExtraLagOffsets[t][c]
                != pm.GetJoypadExtraLagInits((JoypadType)t, (LagContext)c))
                bad++;
    Gate("pm-lag-table", bad == 0 && cells == 47 * 7, "static-init lag table: %d of %d cells differ from GetJoypadExtraLagInits", bad, cells);
    // Retail 0x82545AC8's table: Xbox button guitar 43/19/45, Wii real 22-fret
    // VCal 74.0 (.rdata 0x82091FC0 = 0x42940000), Xbox drums game 36.
    float a = pm.mJoypadExtraLagOffsets[kJoypadXboxButtonGuitar][kVCal];
    float b = pm.mJoypadExtraLagOffsets[kJoypadXboxButtonGuitar][kGame];
    float c = pm.mJoypadExtraLagOffsets[kJoypadWiiRealGuitar22Fret][kVCal];
    float d = pm.mJoypadExtraLagOffsets[kJoypadXboxDrums][kGame];
    Gate("pm-lag-retail", a == 43.0f && b == 45.0f && c == 74.0f && d == 36.0f,
         "xbox guitar vcal %.1f game %.1f (want 43, 45); wii real-22 vcal %.1f (want 74); xbox drums game %.1f (want 36)",
         a, b, c, d);
    Gate("pm-latency", pm.mSyncOffset == -pm.mPlatformVideoLatency
             && pm.GetSongToTaskMgrMsRaw() == pm.mPlatformVideoLatency - pm.mPlatformAudioLatency
             && pm.mMicVolumes.size() == 3,
         "sync offset %.1f (want -video %.1f), song-to-taskmgr %.1f (want video-audio %.1f), %d mic volumes (want 3)",
         pm.mSyncOffset, -pm.mPlatformVideoLatency, pm.GetSongToTaskMgrMsRaw(),
         pm.mPlatformVideoLatency - pm.mPlatformAudioLatency, (int)pm.mMicVolumes.size());
}

void TrackerGates() {
    DataArray *a = DataReadString("(multipliers (0.25 2) (0.5 3) (0.75 4))");
    DataArray *m = a->Array(0);
    TrackerMultiplierMap map;
    map.InitFromDataArray(m);
    struct {
        float f, mult;
        int idx;
    } cases[] = { { 0.1f, 1, 0 }, { 0.3f, 2, 1 }, { 0.6f, 3, 2 }, { 0.9f, 4, 3 } };
    int bad = 0;
    for (auto &k : cases)
        if (map.GetMultiplier(k.f) != k.mult || map.GetMultiplierIndex(k.f) != k.idx)
            bad++;
    float pct = map.GetPercentOfMaxMultiplier(0.375f);
    Gate("tracker-multiplier-map", bad == 0 && pct == 0.5f,
         "(0.25 2)(0.5 3)(0.75 4): %d of 4 lookups wrong; percent-of-max(0.375) %.3f (want 0.5)", bad,
         pct);
    a->Release();
}

} // namespace

int RunW16SHPhase(void (*gate)(const char *, bool, const char *)) {
    gGate = gate;
    printf("=== W16-SH phase: newly linked dsp / synth / meta code ===\n");
    PitchGates();
    EQGates();
    DelayGates();
    DistortionGates();
    FlangerGates();
    ProfileMgrGates();
    TrackerGates();
    return 0;
}
