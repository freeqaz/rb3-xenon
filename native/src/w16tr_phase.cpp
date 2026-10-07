// rb3-xenon native -- W16-TR: callers for the VIA-DC3 files rb3-render linked
// but discarded whole (W16-TM §3: MetaMusic, MidiSynth, StorePanel,
// FlowValueCase, FlowOnStop, HAQManager, DeJitterPanel).
//
// Each gate drives the file the way its retail caller does and checks the
// result against a reference that does not come from the code under test:
//
//   metamusic-*   MetaMusic as MetaPanel::Load/PollForLoading/FinishLoad drive
//                 it, on shipped moggs and the shipped synth config (volume
//                 -20 dB, fade_time 1.0 s, play_from_memory TRUE). The loaded
//                 buffer must equal the file's bytes; the fade must follow the
//                 straight line -96 -> -20 dB over 1000 ms and land exactly; the
//                 receivers must get 10^(dB/20) of the summed faders; a looping
//                 stream must replay the file from sample 0; Stop must fade out
//                 and then release the stream; a start point must start the
//                 decode at that point's sample (against a decode from 0).
//   dejitter-*    DeJitterPanel::Enter/Poll, observed from inside the panel's
//                 own `enter`/`poll` handlers: the TaskMgr seconds timeline is
//                 replaced for the scope with 0 (enter, first frame) or the
//                 dejittered split (later frames, within 16 ms of the raw
//                 clock, delta == difference of successive outputs) and put
//                 back exactly afterwards.
//   storepanel-*  StorePanel's offer pipeline (PopulateOffers, UpdateOffers,
//                 UpdateFromEnumProduct, operator==, load_ok, set_source) over
//                 StoreOffers built from a DataArray, through a probe subclass
//                 that supplies the platform virtuals BandStorePanel would.
//   midisynth-*   MidiSynth's ctor (16 default channels) and the Mic.cpp code
//                 retail placed in MidiSynth.cpp's span and native emits from
//                 that TU: RingBuffer against a byte-deque model, Mic::Set.
//
// FlowValueCase, FlowOnStop and HAQManager have no gate: retail RB3 does not
// contain them (docs/decomp/W16TR_DISCARDED_CALLERS_2026-10-07.md §2).

#include "math/Decibels.h"
#include "meta/DeJitterPanel.h"
#include "meta/StoreOffer.h"
#include "meta/StorePanel.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataFunc.h"
#include "obj/Msg.h"
#include "obj/Task.h"
#include "os/File.h"
#include "os/System.h"
#include "platform/StreamReceiver_Native.h"
#include "synth/Faders.h"
#include "synth/MetaMusic.h"
#include "synth/Mic.h"
#include "synth/MidiSynth.h"
#include "synth/Pollable.h"
#include "synth/StandardStream.h"
#include "synth/Synth.h"
#include "utl/Loader.h"
#include "utl/Symbol.h"

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <list>
#include <thread>
#include <vector>

namespace {

typedef void (*GateFn)(const char *, bool, const char *);
GateFn gGate = nullptr;
char gBuf[768];

void Gate(const char *name, bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Gate(const char *name, bool ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gBuf, sizeof(gBuf), fmt, ap);
    va_end(ap);
    gGate(name, ok, gBuf);
}

typedef std::chrono::steady_clock Clock;
double MsSince(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

// ---------------------------------------------------------------------------
// stream capture (the W16-TM receiver, plus the volume/pan each channel gets)
// ---------------------------------------------------------------------------
struct CapRcvr : public StreamReceiverNative {
    CapRcvr(int n, bool slip) : StreamReceiverNative(n, slip) {}
    virtual void StartSendImpl(unsigned char *data, int size, int) override {
        const short *s = (const short *)data;
        pcm.insert(pcm.end(), s, s + size / 2);
        mSending = true;
        mWantToSend = false;
    }
    virtual void SetVolume(float v) override {
        vol = v;
        StreamReceiverNative::SetVolume(v);
    }
    virtual void SetPan(float p) override {
        pan = p;
        StreamReceiverNative::SetPan(p);
    }
    std::vector<short> pcm;
    float vol = -1.0f, pan = -99.0f;
};
std::vector<CapRcvr *> gRcvrs;
StreamReceiver *CapCreate(int n, int, bool slip, int) {
    CapRcvr *r = new CapRcvr(n, slip);
    gRcvrs.push_back(r);
    return r;
}
struct CaptureScope {
    StreamReceiverFactoryFunc *old;
    CaptureScope() : old(StreamReceiver::sFactory) {
        gRcvrs.clear();
        StreamReceiver::sFactory = CapCreate;
    }
    ~CaptureScope() {
        StreamReceiver::sFactory = old;
        gRcvrs.clear();
    }
};

// Synth with the faders Synth::Init makes (the W16-TM GateSynth): installed
// only when no earlier phase left one.
struct TrSynth : Synth {
    TrSynth() {
        if (!Hmx::Object::RegisteredFactory(Fader::StaticClassName()))
            REGISTER_OBJ_FACTORY(Fader)
        mMasterFader = Hmx::Object::New<Fader>();
        mSfxFader = Hmx::Object::New<Fader>();
        mMidiInstrumentFader = Hmx::Object::New<Fader>();
    }
};

// A decode from sample 0 through the file path (the W16-TM mogg path, which
// its gates check against ffmpeg and the OggMap).
std::vector<std::vector<short> > DecodeFromZero(const char *path) {
    std::vector<std::vector<short> > out;
    File *f = NewFile(path, 2);
    if (!f)
        return out;
    CaptureScope cap;
    StandardStream *s = new StandardStream(f, 0.0f, 2.0f, Symbol("mogg"), false, false);
    s->Play();
    for (int i = 0; i < 20000 && !s->IsFinished() && !s->Fail(); i++)
        s->PollStream();
    for (CapRcvr *r : gRcvrs)
        out.push_back(r->pcm);
    delete s;
    return out;
}

std::vector<unsigned char> ReadWhole(const char *path) {
    std::vector<unsigned char> b;
    File *f = NewFile(path, 2);
    if (!f)
        return b;
    b.resize(f->Size());
    int got = f->Read(b.data(), b.size());
    if (got != (int)b.size())
        b.clear();
    delete f;
    return b;
}

// The metamusic section of the loaded system config; the reference values
// are the shipped config/synth.dta: (volume -20.0) (fade_time 1.0)
// (play_from_memory TRUE), no start_points_ms.
const float kShipVolume = -20.0f, kShipFadeSec = 1.0f;

// MetaPanel::Load + PollForLoading until IsLoaded (MetaMusic::Loaded).
bool LoadAndPoll(MetaMusic *mm, const char *name, float vol, bool b1, bool loop, int &polls) {
    mm->Load(name, vol, b1, loop);
    polls = 0;
    for (; polls < 5000 && !mm->Loaded(); polls++) {
        TheLoadMgr.Poll();
        mm->Poll();
    }
    return mm->Loaded();
}

// Synth::Poll's order: the pollables (streams), then the fader tasks.
void SynthTick() {
    SynthPollable::PollAll();
    FaderTask::PollAll();
}

void MetaMusicGates() {
    if (!TheSynth)
        TheSynth = new TrSynth();
    // NewBufStream asks for a 0 s buffer, which StandardStream::Init replaces
    // with synth stream_buf_size (1.2 in system/run/config/default.dta, which
    // band_preinit_keep #merges). Measured present in rb3-render's config.
    float bufSecs = 0;
    bool haveBuf = SystemConfig("synth")->FindData("stream_buf_size", bufSecs, false);
    printf("  (synth stream_buf_size %s %g)\n", haveBuf ? "present:" : "ABSENT", bufSecs);

    // ---------------------------------------------------------------- load
    const char *kShell = "sfx/streams/shellmusic_multi_simpleton";
    std::vector<unsigned char> fileBytes = ReadWhole("sfx/streams/shellmusic_multi_simpleton.mogg");
    CaptureScope cap;
    MetaMusic *mm = new MetaMusic(nullptr);
    int loadPolls = 0;
    bool loaded = LoadAndPoll(mm, kShell, 0.0f, true, true, loadPolls);
    bool same = loaded && mm->mBuf && !fileBytes.empty() && mm->mBufSize == (int)fileBytes.size()
        && memcmp(mm->mBuf, fileBytes.data(), fileBytes.size()) == 0;
    Gate("metamusic-load",
         loaded && same && mm->mFadeTime == kShipFadeSec && mm->mVolume == kShipVolume
             && mm->mPlayFromBuffer && !mm->mLoader && !mm->mFile && !mm->IsPlaying(),
         "loaded %d after %d polls, buffer %d B == file %zu B: %d, fade_time %g (ship %g), "
         "volume %g (ship %g), from memory %d, loader/file released %d/%d",
         loaded, loadPolls, mm->mBufSize, fileBytes.size(), same, mm->mFadeTime, kShipFadeSec,
         mm->mVolume, kShipVolume, mm->mPlayFromBuffer, !mm->mLoader, !mm->mFile);

    // ---------------------------------------------------- start, fade in
    // MetaPanel::FinishLoad adds background_music_level.fade; this gate's
    // stand-in sits at -3 dB so the receivers' sum is distinguishable.
    Fader *bg = Hmx::Object::New<Fader>();
    bg->SetVal(-3.0f);
    mm->AddFader(bg);
    mm->Start();
    Stream *st = mm->mStream;
    bool started = st && mm->IsPlaying() && !st->IsPlaying();
    int readyPolls = 0;
    for (; st && readyPolls < 2000 && !st->IsReady(); readyPolls++)
        SynthTick();
    Clock::time_point t0 = Clock::now();
    mm->Poll(); // ready, not playing: SetVal(-96), DoFade(volume, fade_time), Play
    double pollMs = MsSince(t0);
    bool playing = st && st->IsPlaying();
    float v0 = mm->mFader->mVal;
    bool fading0 = mm->IsFading();
    float target = mm->mFader->GetTargetDb();
    int nch = st ? st->GetNumChannels() : 0;
    // mid-fade: the fader's own timer started inside Poll, so its elapsed time
    // is within [t - pollMs, t] of this clock.
    double midT = -1;
    float midV = 0;
    while (MsSince(t0) < 1300.0) {
        SynthTick();
        mm->Poll();
        double t = MsSince(t0);
        if (midT < 0 && t > 450.0) {
            midT = t;
            midV = mm->mFader->mVal;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    auto line = [](double ms) { return -96.0 + 76.0 * std::min(1.0, std::max(0.0, ms / 1000.0)); };
    bool midOk = midT > 0 && midV >= line(midT - pollMs) - 0.5 && midV <= line(midT) + 0.5;
    float vEnd = mm->mFader->mVal;
    SynthTick(); // UpdateVolumes pushes the settled sum to the receivers
    double wantRatio = std::pow(10.0, (kShipVolume - 3.0) / 20.0);
    int volOk = 0;
    for (CapRcvr *r : gRcvrs)
        volOk += std::fabs(r->vol - wantRatio) < 1e-4;
    Gate("metamusic-fade-in",
         started && playing && nch == 6 && v0 == -96.0f && fading0 && target == kShipVolume
             && midOk && vEnd == kShipVolume && !mm->IsFading() && volOk == nch,
         "stream %d (%d ch) ready after %d polls, playing %d; fader %g -> target %g fading %d; "
         "at %.0f ms %g (line %.2f..%.2f); end %g fading %d; receivers %d/%d at %.5f (= "
         "10^((-20-3)/20))",
         started, nch, readyPolls, playing, v0, target, fading0, midT, midV,
         line(midT - pollMs), line(midT), vEnd, mm->IsFading(), volOk, nch, wantRatio);

    // ------------------------------------------------------- mute / unmute
    Message muteMsg("mute");
    mm->Handle(muteMsg, true);
    float muteT = mm->mFaderMute->GetTargetDb();
    bool muteF = mm->mFaderMute->IsFading();
    mm->UnMute();
    float unmuteT = mm->mFaderMute->GetTargetDb();
    Gate("metamusic-mute", muteT == -96.0f && muteF && unmuteT == 0.0f && mm->mFader->mVal == kShipVolume,
         "{mute} -> mute fader target %g fading %d; UnMute -> target %g; music fader stays %g",
         muteT, muteF, unmuteT, mm->mFader->mVal);
    mm->mFaderMute->SetVal(0.0f);

    // --------------------------------------------------------------- stop
    int pollablesPlaying = (int)SynthPollable::Pollables().size();
    Clock::time_point s0 = Clock::now();
    mm->Stop();
    float stopT = mm->mFader->GetTargetDb();
    bool stillThere = mm->IsPlaying() && mm->IsFading();
    double releasedAt = -1;
    while (MsSince(s0) < 1500.0) {
        SynthTick();
        mm->Poll();
        if (!mm->IsPlaying()) {
            releasedAt = MsSince(s0);
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
    int pollablesAfter = (int)SynthPollable::Pollables().size();
    Gate("metamusic-stop",
         stopT == -96.0f && stillThere && releasedAt >= 1000.0 * kShipFadeSec && releasedAt < 1300.0
             && !mm->mStream && pollablesAfter == pollablesPlaying - 1,
         "Stop -> target %g, stream kept while fading %d; released at %.0f ms (fade_time %g s); "
         "pollables %d -> %d",
         stopT, stillThere, releasedAt, kShipFadeSec, pollablesPlaying, pollablesAfter);
    delete mm;
    delete bg;

    // ------------------------------------------- stereo: pan and the loop
    // UpdateMix with no fx dir: a 2-channel stream is panned hard (+/-2) when
    // Load's third argument is set, +/-1 otherwise.
    const char *kClap = "sfx/streams/sync_clap";
    std::vector<std::vector<short> > ref = DecodeFromZero("sfx/streams/sync_clap.mogg");
    size_t refN = ref.size() == 2 ? ref[0].size() : 0;
    for (int wide = 1; wide >= 0; wide--) {
        CaptureScope c2;
        MetaMusic *m2 = new MetaMusic(nullptr);
        int lp;
        bool ok = LoadAndPoll(m2, kClap, 0.0f, wide, true, lp);
        m2->Start();
        Stream *s2 = m2->mStream;
        for (int i = 0; s2 && i < 2000 && !s2->IsReady(); i++)
            SynthTick();
        m2->Poll(); // Play
        m2->Poll(); // playing: UpdateMix
        float want = wide ? 2.0f : 1.0f;
        bool pans = s2 && s2->GetNumChannels() == 2 && s2->GetPan(0) == -want && s2->GetPan(1) == want
            && gRcvrs.size() == 2 && gRcvrs[0]->pan == -want && gRcvrs[1]->pan == want;
        if (wide) {
            // looped (MetaPanel passes loop=true): decode two passes' worth
            for (int i = 0; i < 4000 && gRcvrs.size() == 2 && gRcvrs[0]->pcm.size() < 2 * refN + 4096; i++)
                SynthTick();
            bool notDone = s2 && !s2->IsFinished();
            size_t got = gRcvrs.size() == 2 ? gRcvrs[0]->pcm.size() : 0;
            // the second pass is the file again from sample 0
            int diff = -1, firstDiff = -1;
            if (refN && got >= refN + 4096) {
                diff = 0;
                for (int c = 0; c < 2; c++)
                    for (size_t k = 0; k < 4096; k++)
                        if (gRcvrs[c]->pcm[refN + k] != ref[c][k]) {
                            if (firstDiff < 0)
                                firstDiff = (int)k;
                            diff++;
                        }
            }
            int pre = -1;
            if (refN && got >= refN) {
                pre = 0;
                for (int c = 0; c < 2; c++)
                    for (size_t k = 0; k < refN; k++)
                        pre += gRcvrs[c]->pcm[k] != ref[c][k];
            }
            Gate("metamusic-loop", ok && notDone && pre == 0 && diff == 0,
                 "%zu samples/ch decoded, one pass %zu; not finished %d; pass 1 differs from "
                 "the decode-from-0 in %d samples, pass 2's first 4096/ch in %d (first at %d)",
                 got, refN, notDone, pre, diff, firstDiff);
        }
        Gate(wide ? "metamusic-pan-wide" : "metamusic-pan-narrow", ok && pans,
             "loaded %d, %d ch, stream pans %g/%g, receivers %g/%g (want -%g/+%g)", ok,
             s2 ? s2->GetNumChannels() : 0, s2 ? s2->GetPan(0) : 0, s2 ? s2->GetPan(1) : 0,
             gRcvrs.size() == 2 ? gRcvrs[0]->pan : 0, gRcvrs.size() == 2 ? gRcvrs[1]->pan : 0, want,
             want);
        delete m2; // still playing: the dtor releases the stream
    }

    // ------------------------------------------------------- start point
    // MetaMusic::Load reads (start_points_ms ...) and Start passes the choice
    // to NewBufStream as the stream's start (retail Synth360::NewBufStream
    // builds StandardStream(BufFile, startMs, 0.0f, ...)). The shipped config
    // has none, so inject one point: the decode must begin at its sample.
    {
        DataArray *mcfg = SystemConfig("synth", "metamusic");
        DataArray *sp = DataReadString("(start_points_ms 500)");
        int at = mcfg->Size();
        mcfg->Insert(at, DataNode(sp->Array(0), kDataArray));
        sp->Release();
        // the shipped fade_time (1.0) equals MetaMusic's ctor default, so the
        // load gate cannot see whether Load reads it: this load uses 0.25
        DataArray *ft = mcfg->FindArray("fade_time", true);
        float shipFade = ft->Float(1);
        ft->Node(1) = DataNode(0.25f);
        CaptureScope c3;
        MetaMusic *m3 = new MetaMusic(nullptr);
        int lp;
        bool ok = LoadAndPoll(m3, kClap, 0.0f, true, false, lp);
        int choice = m3->ChooseStartMs();
        m3->Start();
        Stream *s3 = m3->mStream;
        for (int i = 0; s3 && i < 2000 && !s3->IsReady(); i++)
            SynthTick();
        m3->Poll();
        float t3 = s3 ? s3->GetTime() : -1;
        for (int i = 0; s3 && i < 2000 && !s3->IsFinished(); i++)
            SynthTick();
        const size_t kSkip = 500 * 44100 / 1000; // sync_clap is 44.1 kHz
        size_t got = gRcvrs.size() == 2 ? gRcvrs[0]->pcm.size() : 0;
        int diff = -1;
        if (refN > kSkip && got == refN - kSkip) {
            diff = 0;
            for (int c = 0; c < 2; c++)
                for (size_t k = 0; k < got; k++)
                    diff += gRcvrs[c]->pcm[k] != ref[c][kSkip + k];
        }
        float fade3 = m3->mFadeTime;
        Gate("metamusic-start-point",
             ok && choice == 500 && t3 >= 500.0f && t3 < 600.0f && diff == 0 && fade3 == 0.25f,
             "ChooseStartMs %d (want 500); stream time %g ms after Play; %zu samples/ch "
             "(want %zu - %zu = %zu), %d differ from the decode-from-0 at +%zu; config "
             "fade_time 0.25 -> %g",
             choice, t3, got, refN, kSkip, refN - kSkip, diff, kSkip, fade3);
        delete m3;
        ft->Node(1) = DataNode(shipFade);
        mcfg->Remove(at);
    }
}

// ---------------------------------------------------------------------------
// DeJitterPanel
// ---------------------------------------------------------------------------
struct DjSample {
    float secs, delta;
};
std::vector<DjSample> gDj;
DataNode DjProbe(DataArray *) {
    gDj.push_back({ TheTaskMgr.Seconds(TaskMgr::kRealTime), TheTaskMgr.DeltaSeconds() });
    return 0;
}

struct ProbeDJ : public DeJitterPanel {
    void Arm() {
        mState = kDown;
        mLoadRefs = 1;
    }
    void Disarm() {
        mState = kUnloaded;
        mLoadRefs = 0;
    }
    bool FirstFrame() const { return mFirstFrame; }
};

void DeJitterGates() {
    DataRegisterFunc("w16tr_dj_probe", DjProbe);
    ProbeDJ *p = new ProbeDJ();
    DataArray *def = DataReadString("(enter {w16tr_dj_probe}) (poll {w16tr_dj_probe})");
    p->SetTypeDef(def);
    def->Release();
    p->Arm();

    // a known outside time, which every scope must hand back
    TheTaskMgr.SetTimeAndDelta(kTaskSeconds, 123.5f, 0.25f);
    gDj.clear();
    p->Enter();
    float afterS = TheTaskMgr.Seconds(TaskMgr::kRealTime), afterD = TheTaskMgr.DeltaSeconds();
    bool enterOk = gDj.size() == 1 && gDj[0].secs == 0.0f && gDj[0].delta == 0.0f;
    Gate("dejitter-panel-enter",
         enterOk && afterS == 123.5f && afterD == 0.25f && p->FirstFrame() && p->GetState() == UIPanel::kUp,
         "inside enter: %zu call(s), seconds %g delta %g (want 0/0); after: %g/%g (want "
         "123.5/0.25); first frame %d, up %d",
         gDj.size(), gDj.empty() ? -1 : gDj[0].secs, gDj.empty() ? -1 : gDj[0].delta, afterS,
         afterD, p->FirstFrame(), p->GetState() == UIPanel::kUp);

    // frames ~5 ms apart; the panel's Timer starts on the first Poll
    gDj.clear();
    Clock::time_point f0;
    std::vector<double> wall;
    int restored = 0;
    const int kFrames = 40;
    for (int i = 0; i < kFrames; i++) {
        if (i == 0)
            f0 = Clock::now();
        double w = MsSince(f0);
        TheTaskMgr.SetTimeAndDelta(kTaskSeconds, 50.0f + i, 0.5f);
        p->Poll();
        double w2 = MsSince(f0);
        wall.push_back(0.5 * (w + w2));
        restored += TheTaskMgr.Seconds(TaskMgr::kRealTime) == 50.0f + i && TheTaskMgr.DeltaSeconds() == 0.5f;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    bool first = gDj.size() == (size_t)kFrames && gDj[0].secs == 0.0f && gDj[0].delta == 0.0f;
    int monotone = 0, deltaOk = 0, near = 0;
    double worst = 0;
    for (size_t i = 1; i < gDj.size(); i++) {
        monotone += gDj[i].secs >= gDj[i - 1].secs;
        // every frame after the first reports delta = this output - last output
        // (the second frame's "last output" is the primer's 0)
        float prev = i == 1 ? 0.0f : gDj[i - 1].secs;
        deltaOk += std::fabs(gDj[i].delta - (gDj[i].secs - prev)) < 2e-5f;
        // the dejittered time stays within 16 ms of the raw split (+ the span
        // of this frame's own wall-clock bracket)
        double err = std::fabs(gDj[i].secs * 1000.0 - wall[i]);
        worst = std::max(worst, err);
        near += err < 16.0 + 3.0;
    }
    int n1 = kFrames - 1;
    Gate("dejitter-panel-poll",
         first && monotone == n1 && deltaOk == n1 && near == n1 && restored == kFrames && !p->FirstFrame(),
         "%zu poll handler calls; frame 0 seconds/delta %g/%g (want 0/0); frames 1..%d: "
         "non-decreasing %d, delta == step %d, within 16 ms of the raw clock %d (worst %.2f ms, "
         "final %.4f s); outside time restored %d/%d",
         gDj.size(), gDj.empty() ? -1 : gDj[0].secs, gDj.empty() ? -1 : gDj[0].delta, n1,
         monotone, deltaOk, near, worst, gDj.empty() ? 0 : gDj.back().secs, restored, kFrames);
    p->Disarm();
    delete p;
}

// ---------------------------------------------------------------------------
// StorePanel
// ---------------------------------------------------------------------------
struct ProbeStore : public StorePanel {
    virtual bool IsSongInLibrary(int const &) const override { return false; }
    virtual void ExitStore(StoreError) const override {}
    virtual LocalUser *StoreUser() const override { return nullptr; }
    virtual StoreOffer *MakeNewOffer(DataArray *a) override { return new StoreOffer(a, nullptr); }
    virtual StoreOffer *FindOffer(Symbol) const override { return nullptr; }
    virtual void GetOfferIDsToEnumerate(std::vector<u64> &, bool) const override {}
    virtual void StoreUserProfileSwappedToUser(LocalUser *) override {}
    void Populate(DataArray *a, bool pending) { PopulateOffers(a, pending); }
    int Update(const std::list<EnumProduct> &l, bool pending) { return UpdateOffers(l, pending); }
};

EnumProduct Prod(u64 id, int purchased, int price) {
    EnumProduct p;
    p.mName = "w16tr";
    p.mOfferID = id;
    p.mPurchased = purchased;
    p.mPrice = price;
    return p;
}

void StorePanelGates() {
    ProbeStore *sp = new ProbeStore();
    DataArray *offersFile = DataReadString(
        "(offers"
        " (song_a (type song) (id \"0123456789ABCDEF\") (song_ids 11)"
        "   (album_id \"00000000000A0001\") (pack_id \"00000000000B0001\"))"
        " (song_b (type song) (id \"0000000000000102\") (song_ids 12))"
        " (test_c (type song) (id \"0000000000000103\") (song_ids 13) (test 1)))");
    DataArray *offers = offersFile->Array(0);

    // load_ok gates PopulateOffers (StorePanel::Load sets it from the cache)
    sp->Populate(offers, false);
    size_t before = sp->mOffers.size();
    sp->SetProperty("load_ok", DataNode(1));
    bool prop = sp->Property("load_ok", true)->Int() == 1 && sp->mLoadOk;
    sp->Populate(offers, false);
    bool order = sp->mOffers.size() == 2 && sp->mOffers[0]->songID == 0x0123456789ABCDEFull
        && sp->mOffers[0]->mAlbum.songID == 0xA0001ull && sp->mOffers[0]->mPack.songID == 0xB0001ull
        && sp->mOffers[1]->songID == 0x102ull && sp->mPendingOffers.empty();
    sp->Populate(offers, true); // pending: the live list is kept
    bool pend = sp->mPendingOffers.size() == 2 && sp->mOffers.size() == 2;
    sp->mShowTestOffers = true;
    sp->Populate(offers, false);
    bool withTest = sp->mOffers.size() == 3 && sp->mOffers[2]->songID == 0x103ull && sp->mPendingOffers.empty();
    Gate("storepanel-populate", before == 0 && prop && order && pend && withTest,
         "load_ok 0 -> %zu offers; load_ok set via property %d; 2 live, test offer hidden, "
         "ids/album/pack parsed %d; pending fill keeps the live list %d; test offers shown -> %zu",
         before, prop, order, pend, sp->mOffers.size());

    // UpdateOffers: a live enumeration (Marketplace product list)
    std::list<EnumProduct> en;
    en.push_back(Prod(0x0123456789ABCDEFull, 1, 160));
    en.push_back(Prod(0xA0001ull, 0, 1200));
    en.push_back(Prod(0x999ull, 1, 80)); // an offer we do not show
    StoreOffer *a = sp->mOffers[0], *b = sp->mOffers[1], *c = sp->mOffers[2];
    b->isAvailable = false;
    b->cost = 7;
    // test offers hidden, so a kStoreErrorSuccess here can only come from a match
    sp->mShowTestOffers = false;
    int r1 = sp->Update(en, false);
    bool aOk = a->isAvailable && a->isPurchased && a->cost == 160;
    bool albOk = a->mAlbum.isAvailable && !a->mAlbum.isPurchased && a->mAlbum.cost == 1200;
    bool packOk = !a->mPack.isAvailable && a->mPack.cost == 0;
    bool bOk = !b->isAvailable && b->cost == 7; // not enumerated, not a test: untouched
    bool cOk = !c->isAvailable && !c->isPurchased && c->cost == 9999; // unenumerated test offer
    bool eq = en.front() == *(StorePurchaseable *)a && !(en.back() == *(StorePurchaseable *)a);
    std::list<EnumProduct> none;
    sp->mShowTestOffers = false;
    int r2 = sp->Update(none, false);
    int r3 = sp->Update(none, true); // no pending offers
    sp->mShowTestOffers = true;
    int r4 = sp->Update(none, false);
    Gate("storepanel-update-offers",
         r1 == kStoreErrorSuccess && aOk && albOk && packOk && bOk && cOk && eq
             && r2 == kStoreErrorNoContent && r3 == kStoreErrorSignedOut && r4 == kStoreErrorSuccess,
         "match -> %d; offer %d, album %d, pack untouched %d, unlisted %d, test 9999 %d, "
         "operator== %d; nothing matched -> %d (want %d), empty list -> %d (want %d), test "
         "offers shown -> %d (want 0)",
         r1, aOk, albOk, packOk, bOk, cOk, eq, r2, kStoreErrorNoContent, r3,
         kStoreErrorSignedOut, r4);

    // set_source through Handle, then set_source_to_backup
    Message m1("set_source", Symbol("w16tr_main"), 1);
    sp->Handle(m1, true);
    Message m2("set_source", Symbol("w16tr_other"), 0);
    sp->Handle(m2, true);
    Symbol cur = sp->mPurchaseSource, bak = sp->mBackupPurchaseSource;
    Message m3("set_source_to_backup");
    sp->Handle(m3, true);
    Gate("storepanel-source",
         cur == Symbol("w16tr_other") && bak == Symbol("w16tr_main") && sp->mPurchaseSource == Symbol("w16tr_main"),
         "after (main,1),(other,0): source %s backup %s; to_backup -> %s", cur.Str(), bak.Str(),
         sp->mPurchaseSource.Str());
    delete sp;
    offersFile->Release();
}

// ---------------------------------------------------------------------------
// MidiSynth (and the Mic.cpp code native emits from its TU)
// ---------------------------------------------------------------------------
struct ProbeMidiSynth : public MidiSynth {
    const std::vector<MidiChannel> &Channels() const { return mChannels; }
};

struct ProbeMic : public Mic {
    float gain = -1, cparam = -1;
    int dma = -1, comp = -1;
    virtual void Start() override {}
    virtual void Stop() override {}
    virtual bool IsRunning() const override { return false; }
    virtual Type GetType() const override { return kMicNull; }
    virtual void SetDMA(bool b) override { dma = b; }
    virtual bool GetDMA() const override { return dma == 1; }
    virtual void SetGain(float g) override { gain = g; }
    virtual float GetGain() const override { return gain; }
    virtual void SetEarpieceVolume(float) override {}
    virtual float GetEarpieceVolume() const override { return 0; }
    virtual bool GetClipping() const override { return false; }
    virtual void SetOutputGain(float) override {}
    virtual float GetOutputGain() const override { return 0; }
    virtual void SetSensitivity(float) override {}
    virtual float GetSensitivity() const override { return 0; }
    virtual void SetCompressor(bool b) override { comp = b; }
    virtual bool GetCompressor() const override { return comp == 1; }
    virtual void SetCompressorParam(float f) override { cparam = f; }
    virtual float GetCompressorParam() const override { return cparam; }
    virtual short *GetRecentBuf(int &) override { return nullptr; }
    virtual short *GetContinuousBuf(int &) override { return nullptr; }
    virtual int GetSampleRate() const override { return 0; }
};

void MidiSynthGates() {
    ProbeMidiSynth ms;
    MidiChannel def;
    int same = 0;
    for (const MidiChannel &c : ms.Channels())
        same += memcmp(&c, &def, 5 * sizeof(int)) == 0;
    float vol;
    memcpy(&vol, (const char *)&def + 2 * sizeof(int), sizeof(float));
    Gate("midisynth-ctor", ms.Channels().size() == 16 && same == 16 && vol == 1.0f,
         "%zu channels (want 16), %d equal to a default MidiChannel (inst/bank/volume/pan/"
         "transpose), default volume %g",
         ms.Channels().size(), same, vol);

    // RingBuffer against a model: a FIFO of at most `size` bytes that drops
    // the oldest on overflow (Write returns the bytes dropped, negative while
    // there is room), and a write history Peek reads the newest `len` of.
    const int kSize = 1000;
    RingBuffer rb;
    rb.Init(kSize);
    std::deque<unsigned char> fifo;
    std::vector<unsigned char> hist(kSize, 0); // Init zeroes the buffer
    unsigned seed = 0x5eed1234u;
    auto rnd = [&seed](int n) {
        seed = seed * 1103515245u + 12345u;
        return (int)((seed >> 8) % (unsigned)n);
    };
    unsigned char in[2 * kSize], out[2 * kSize], want[2 * kSize];
    unsigned char next = 1;
    int ops = 0, bad = 0;
    char first[200] = "";
    for (int i = 0; i < 3000; i++, ops++) {
        int op = rnd(3);
        int len = op == 0 ? 1 + rnd(i % 50 == 0 ? 2 * kSize : 300) : 1 + rnd(op == 2 ? kSize : 400);
        if (op == 0) {
            for (int k = 0; k < len; k++)
                in[k] = next++;
            int kept = std::min(len, kSize);
            int wantRet = (int)fifo.size() + kept - kSize;
            for (int k = len - kept; k < len; k++) {
                fifo.push_back(in[k]);
                hist.push_back(in[k]);
            }
            while ((int)fifo.size() > kSize)
                fifo.pop_front();
            int got = rb.Write(in, len);
            if (got != wantRet && !bad++)
                snprintf(first, sizeof(first), "op %d Write(%d) -> %d, want %d", i, len, got, wantRet);
        } else if (op == 1) {
            int n = std::min(len, (int)fifo.size());
            for (int k = 0; k < n; k++) {
                want[k] = fifo.front();
                fifo.pop_front();
            }
            int got = rb.Read(out, len);
            if ((got != n || memcmp(out, want, n) != 0) && !bad++)
                snprintf(first, sizeof(first), "op %d Read(%d) -> %d, want %d", i, len, got, n);
        } else {
            int got = rb.Peek(out, len);
            if ((got != len || memcmp(out, hist.data() + hist.size() - len, len) != 0) && !bad++)
                snprintf(first, sizeof(first), "op %d Peek(%d) -> %d bytes differ", i, len, got);
        }
        if (hist.size() > 8 * kSize)
            hist.erase(hist.begin(), hist.end() - 2 * kSize);
    }
    Gate("midisynth-ringbuffer", bad == 0, "%d random Write/Read/Peek ops on a %d B ring, %d differ from the FIFO model; %s",
         ops, kSize, bad, first);

    ProbeMic mic;
    DataArray *cfgFile = DataReadString("(mic (gain 0.75) (dma 1) (compressor 1 0.3))");
    mic.Set(cfgFile->Array(0));
    cfgFile->Release();
    Gate("midisynth-mic-set",
         mic.gain == 0.75f && mic.dma == 1 && mic.comp == 1 && std::fabs(mic.cparam - 0.3f) < 1e-7f,
         "(gain 0.75) (dma 1) (compressor 1 0.3) -> gain %g, dma %d, compressor %d param %g",
         mic.gain, mic.dma, mic.comp, mic.cparam);
}

} // namespace

int RunW16TRPhase(void (*gate)(const char *, bool, const char *)) {
    gGate = gate;
    printf("\n=== W16-TR: callers for files rb3-render discarded ===\n");
    MidiSynthGates();
    StorePanelGates();
    DeJitterGates();
    MetaMusicGates();
    return 0;
}
