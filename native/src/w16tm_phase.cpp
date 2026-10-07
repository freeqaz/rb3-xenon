// rb3-xenon native -- W16-TM: gates for the VIA-DC3 files native first runs.
//
// CAMPAIGN_STATE_2026-10-07b lever 4 linked 25 VIA-DC3 files that no native
// target compiled into rb3-render (plus the in-tree libogg/libvorbis and
// tomcrypt's cipher registry). This phase runs the ones whose behaviour has a
// reference answer that does not come from the code under test:
//
//   crypto-*      tomcrypt aes.c / ctr.c / crypt.c: FIPS-197 C.1, SP 800-38A
//                 F.5.1 block 1, and a 4 KB CTR keystream whose CRC32 was
//                 computed with pyca/cryptography's AES and a little-endian
//                 counter (the direction the v0x0B mogg pages need: the other
//                 one yields a single valid Ogg page).
//   mogg-*        the whole stream path on shipped Xbox moggs from the ark:
//                 StandardStream -> Synth::NewStreamDecoder -> VorbisReader
//                 (header, OggMap, key setup, ByteGrinder/KeyChain for v0x0E
//                 and v0x10, AES-CTR, in-tree libvorbis) -> StreamReceiver.
//                 v0x0B against an independent decrypt + ffmpeg decode
//                 (exact length, channels, rate; RMS and peak per channel).
//                 v0x0E / v0x10 have no independent decryptor, so the
//                 reference is the unencrypted OggMap the encoder wrote: the
//                 decoded length must land past its last entry and within two
//                 map steps of it. libogg checks every page's CRC32, so a wrong
//                 key fails the decode outright.
//   fx-compress-* CompressionEffect: ratio <= 1.01 is a bypass (output ==
//                 input); in steady state the slope above the threshold is
//                 1/ratio of the slope below it; a signal under the gate
//                 threshold is gated to silence.
//   synth-utl-*   CalcSpeedFromTranspose / CalcTransposeFromSpeed against
//                 2^(semitones/12), CalcRateForTempoSync against bpm/60 per
//                 quarter note.
//   flow-if-*     FlowIf::Activate: the six operators on ints, floats and a
//                 mixed pair, against C++'s own comparison, observed through
//                 whether the child node is activated.
//
// Each gate can fail; the sabotage controls are in
// docs/decomp/W16TM_VIA_DC3_NATIVE_LINK_2026-10-07.md.
//
// RB3_W16TM_DUMP=<dir> writes each decoded mogg as interleaved s16le, for the
// lane-time sample-by-sample comparison against ffmpeg.

#include "flow/FlowIf.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "os/File.h"
#include "platform/StreamReceiver_Native.h"
#include "synth/CompressionEffect.h"
#include "synth/Faders.h"
#include "synth/StandardStream.h"
#include "synth/Synth.h"
#include "synth/Utl.h"
#include "synth/tomcrypt/mycrypt.h"
#include "utl/Symbol.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

bool HexEq(const unsigned char *a, const char *hex, int n) {
    for (int i = 0; i < n; i++) {
        unsigned v;
        sscanf(hex + 2 * i, "%2x", &v);
        if (a[i] != (unsigned char)v)
            return false;
    }
    return true;
}

unsigned Crc32(const unsigned char *p, int n) { // zlib's CRC-32 (reflected 0xEDB88320)
    unsigned c = 0xFFFFFFFFu;
    for (int i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

// ---------------------------------------------------------------------------
// crypto
// ---------------------------------------------------------------------------
const unsigned char kRB1Key[16] = { 0x37, 0xB2, 0xE2, 0xB9, 0x1C, 0x74, 0xFA, 0x9E,
                                    0x38, 0x81, 0x08, 0xEA, 0x36, 0x23, 0xDB, 0xE4 };

void CryptoGates() {
    unsigned char key[16], pt[16], ct[16];
    for (int i = 0; i < 16; i++) {
        key[i] = i;
        pt[i] = (unsigned char)(i * 0x11);
    }
    symmetric_key sk;
    int rc = rijndael_setup(key, 16, 0, &sk);
    rijndael_ecb_encrypt(pt, ct, &sk);
    Gate("crypto-aes-fips197", rc == 0 && HexEq(ct, "69c4e0d86a7b0430d8cdb78070b4c55a", 16),
         "rc %d, ct[0..3] %02x%02x%02x%02x (want 69c4e0d8)", rc, ct[0], ct[1], ct[2], ct[3]);

    int cipher = register_cipher(&rijndael_desc);
    unsigned char k2[16], ctr0[16], out[16];
    static const char *k2h = "2b7e151628aed2a6abf7158809cf4f3c";
    static const char *c0h = "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
    static const char *p1h = "6bc1bee22e409f96e93d7e117393172a";
    for (int i = 0; i < 16; i++) {
        unsigned a, b, c;
        sscanf(k2h + 2 * i, "%2x", &a);
        sscanf(c0h + 2 * i, "%2x", &b);
        sscanf(p1h + 2 * i, "%2x", &c);
        k2[i] = a;
        ctr0[i] = b;
        pt[i] = c;
    }
    symmetric_CTR ctr;
    int rc1 = ctr_start(cipher, ctr0, k2, 16, 0, &ctr);
    int rc2 = ctr_encrypt(pt, out, 16, &ctr);
    Gate("crypto-ctr-sp800-38a", cipher >= 0 && rc1 == 0 && rc2 == 0
             && HexEq(out, "874d6191b620e3261bef6864990db6ce", 16),
         "cipher %d rc %d/%d, ct[0..3] %02x%02x%02x%02x (want 874d6191)", cipher, rc1, rc2,
         out[0], out[1], out[2], out[3]);

    // 4 KB keystream: the first 16 bytes go through ctr_encrypt's byte loop
    // (pad not yet consumed), the rest through ctr_encrypt_fast (aligned,
    // multiple of 16, pad consumed). A second pass does it one byte at a time.
    unsigned char nonce[16];
    for (int i = 0; i < 16; i++)
        nonce[i] = i;
    std::vector<unsigned char> zero(4096, 0), ks(4096), ks2(4096);
    ctr_start(cipher, nonce, kRB1Key, 16, 0, &ctr);
    ctr_encrypt(zero.data(), ks.data(), 16, &ctr);
    ctr_encrypt(zero.data() + 16, ks.data() + 16, 4096 - 16, &ctr);
    ctr_start(cipher, nonce, kRB1Key, 16, 0, &ctr);
    for (int i = 0; i < 4096; i++)
        ctr_encrypt(zero.data() + i, ks2.data() + i, 1, &ctr);
    unsigned c1 = Crc32(ks.data(), 4096), c2 = Crc32(ks2.data(), 4096);
    Gate("crypto-ctr-keystream-4k", c1 == 0xba1dfccau && c2 == 0xba1dfccau,
         "crc32 fast path %08x, byte path %08x (reference ba1dfcca)", c1, c2);
}

// ---------------------------------------------------------------------------
// mogg decode
// ---------------------------------------------------------------------------
// StandardStream's native ConsumeData static_casts each channel to
// StreamReceiverNative (flow control reads its ring space), so the capture
// receiver must BE one. StartSendImpl keeps the samples and never advances the
// ring's write cursor: the space check always sees an empty ring, and
// IsOutputDrained() (play >= write) is true, which lets the stream finish.
struct CaptureRcvr : public StreamReceiverNative {
    CaptureRcvr(int n, bool slip) : StreamReceiverNative(n, slip) {}
    virtual void StartSendImpl(unsigned char *data, int size, int) override {
        const short *s = (const short *)data;
        pcm.insert(pcm.end(), s, s + size / 2);
        mSending = true;
        mWantToSend = false;
    }
    std::vector<short> pcm;
};
std::vector<CaptureRcvr *> gRcvrs;
StreamReceiver *CaptureCreate(int n, int rate, bool slip, int) {
    CaptureRcvr *r = new CaptureRcvr(n, slip);
    gRcvrs.push_back(r);
    return r;
}

struct Decoded {
    bool opened = false, failed = false, finished = false;
    int channels = 0, polls = 0;
    std::vector<std::vector<short> > pcm;
};

Decoded DecodeMogg(const char *path) {
    Decoded d;
    File *f = NewFile(path, 2); // 2 = kRead, as Synth::NewStreamFile opens moggs
    if (!f)
        return d;
    d.opened = true;
    gRcvrs.clear();
    StreamReceiverFactoryFunc *old = StreamReceiver::sFactory;
    StreamReceiver::sFactory = CaptureCreate;
    // Ext "mogg" takes Synth::NewStreamDecoder's VorbisReader arm. 2 s of
    // buffer (stream_buf_size is not in the preinit config); polling off: the
    // gate drives PollStream itself.
    StandardStream *s = new StandardStream(f, 0.0f, 2.0f, Symbol("mogg"), false, false);
    s->Play(); // pumps headers, pre-fills, then kPlaying
    for (int i = 0; i < 200000 && !s->IsFinished() && !s->Fail(); i++, d.polls++)
        s->PollStream();
    d.failed = s->Fail();
    d.finished = s->IsFinished();
    d.channels = (int)gRcvrs.size();
    for (CaptureRcvr *r : gRcvrs)
        d.pcm.push_back(r->pcm);
    delete s; // releases the file and the receivers
    gRcvrs.clear();
    StreamReceiver::sFactory = old;

    if (const char *dir = getenv("RB3_W16TM_DUMP")) {
        const char *base = strrchr(path, '/');
        char out[512];
        snprintf(out, sizeof(out), "%s/%s.s16", dir, base ? base + 1 : path);
        if (FILE *o = fopen(out, "wb")) {
            size_t n = d.pcm.empty() ? 0 : d.pcm[0].size();
            for (size_t i = 0; i < n; i++)
                for (auto &ch : d.pcm)
                    fwrite(i < ch.size() ? &ch[i] : &ch[0], 2, 1, o);
            fclose(o);
        }
    }
    return d;
}

void Stats(const std::vector<short> &s, double &rms, int &peak) {
    double acc = 0;
    peak = 0;
    for (short v : s) {
        acc += (double)v * v;
        if (std::abs((int)v) > peak)
            peak = std::abs((int)v);
    }
    rms = s.empty() ? 0 : std::sqrt(acc / s.size());
}

// Reference from the lane script ~/tmp/w16tm/mogg_ref.py: AES-128-CTR
// with pyca/cryptography, Python CRC check of every Ogg page, ffmpeg decode.
struct V0BRef {
    const char *path;
    int channels;
    unsigned samples;
    double rms[2];
    int peak[2];
};
const V0BRef kV0B[] = {
    // 9/9 Ogg pages CRC-valid after decrypt; mono 48 kHz
    { "sfx/streams/sync_beep.mogg", 1, 220500, { 1186.126, 0 }, { 15685, 0 } },
    // 4/4 pages; stereo 44.1 kHz; ffmpeg clips to 32768, the engine to 32767
    { "sfx/streams/sync_clap.mogg", 2, 53363, { 2839.074, 2765.360 }, { 32768, 32768 } },
};

// The base Synth with the three faders Synth::Init makes (Stream's ctor adds
// TheSynth->MasterFader() to every stream's group). The rest of Synth::Init
// (synth_hud overlay, mics, security) is not needed to decode.
struct GateSynth : Synth {
    GateSynth() {
        if (!Hmx::Object::RegisteredFactory(Fader::StaticClassName()))
            REGISTER_OBJ_FACTORY(Fader)
        mMasterFader = Hmx::Object::New<Fader>();
        mSfxFader = Hmx::Object::New<Fader>();
        mMidiInstrumentFader = Hmx::Object::New<Fader>();
    }
};

void MoggGates() {
    // Synth::NewStreamDecoder and VorbisReader::CheckHmxHeader go through
    // TheSynth (the stream decoder factory, the ByteGrinder). rb3-render has
    // none; the base Synth is the null synth SynthPreInit falls back to.
    bool own = false;
    if (!TheSynth) {
        TheSynth = new GateSynth();
        own = true;
    }
    for (const V0BRef &r : kV0B) {
        Decoded d = DecodeMogg(r.path);
        bool shape = d.opened && !d.failed && d.finished && d.channels == r.channels;
        bool ok = shape;
        char detail[256] = "";
        int off = 0;
        for (int c = 0; shape && c < r.channels; c++) {
            double rms;
            int peak;
            Stats(d.pcm[c], rms, peak);
            bool len = d.pcm[c].size() == r.samples;
            bool rmsOk = std::fabs(rms - r.rms[c]) <= 0.005 * r.rms[c];
            bool peakOk = std::abs(peak - r.peak[c]) <= r.peak[c] / 100 + 1;
            ok = ok && len && rmsOk && peakOk;
            off += snprintf(detail + off, sizeof(detail) - off,
                            " ch%d %zu samples (ref %u) rms %.3f (ref %.3f) peak %d (ref %d);",
                            c, d.pcm[c].size(), r.samples, rms, r.rms[c], peak, r.peak[c]);
        }
        const char *name = strstr(r.path, "beep") ? "mogg-v0b-sync-beep" : "mogg-v0b-sync-clap";
        Gate(name, ok, "opened %d failed %d finished %d channels %d (ref %d), %d polls;%s",
             d.opened, d.failed, d.finished, d.channels, r.channels, d.polls, detail);
    }

    // v0x0E (HMXA pages, magic-hash XOR) and v0x10 (keymask HvDecrypt +
    // ByteGrinder): bounded by the plaintext OggMap.
    struct MapRef {
        const char *name, *path;
        unsigned lastSample, maxStep;
    };
    // lastSample = the map's last entry; maxStep = its largest sample step
    // (both read from the header by mogg_ref.py).
    static const MapRef kMap[] = {
        { "mogg-v0e-shellmusic", "sfx/streams/shellmusic_multi_simpleton.mogg", 1393088, 38656 },
        { "mogg-v10-trainer",
          "ui/trainers/songs/drum_trainer_medium/intermediate_techniques/"
          "intermediate_techniques.mogg",
          1091584, 117568 },
    };
    for (const MapRef &m : kMap) {
        Decoded d = DecodeMogg(m.path);
        size_t n = d.pcm.empty() ? 0 : d.pcm[0].size();
        bool same = true;
        double rms0 = 0;
        int peak0 = 0;
        for (auto &ch : d.pcm)
            same = same && ch.size() == n;
        if (!d.pcm.empty())
            Stats(d.pcm[0], rms0, peak0);
        unsigned step = m.maxStep;
        bool ok = d.opened && !d.failed && d.finished && d.channels > 0 && same
            && n >= m.lastSample && n <= (size_t)m.lastSample + 2 * step && rms0 > 1.0;
        Gate(m.name, ok,
             "opened %d failed %d finished %d, %d channels, %zu samples each=%d "
             "(map last %u, bound +%u), ch0 rms %.1f peak %d, %d polls",
             d.opened, d.failed, d.finished, d.channels, n, same, m.lastSample, 2 * step,
             rms0, peak0, d.polls);
    }
    // Synth's destructor is protected (SynthTerminate owns it). The null synth
    // stays installed; this phase runs last in the default mode.
    (void)own;
}

// ---------------------------------------------------------------------------
// CompressionEffect
// ---------------------------------------------------------------------------
// Steady-state stereo output for a constant input of amplitude `a`.
float CompOut(CompressionEffect &c, float a) {
    std::vector<float> buf(2 * 4800);
    float out = 0;
    for (int blk = 0; blk < 20; blk++) { // 2 s at 48 kHz: the 0.2 s release has settled
        for (size_t i = 0; i < buf.size(); i++)
            buf[i] = a;
        c.Process(buf.data(), 4800, 2);
        out = buf.back();
    }
    return out;
}

void CompressGates() {
    CompressionEffect c(nullptr);
    CompressionEffect::Params p;
    // ratio 1 (the shipped default): bypass, output is input bit for bit
    p.mRatio = 1.0f;
    c.SetParameters(p);
    float x[8] = { 0.9f, -0.5f, 0.25f, -0.125f, 0.3f, 0.7f, -0.9f, 0.01f };
    float y[8];
    memcpy(y, x, sizeof(x));
    c.Process(y, 4, 2);
    Gate("fx-compress-bypass", memcmp(x, y, sizeof(x)) == 0, "ratio 1.0: out[0] %g (in %g)",
         y[0], x[0]);

    // ratio 4, threshold -6 dB (0.501): two levels below, two above. In a
    // compressor the output-vs-input slope above the threshold is 1/ratio of
    // the slope below it (this effect works on linear amplitude).
    p.mRatio = 4.0f;
    p.mThresholdDb = -6.0f;
    p.mOutputGainDb = 0.0f;
    float lo1 = 0.20f, lo2 = 0.30f, hi1 = 0.70f, hi2 = 0.90f;
    float o[4];
    float in[4] = { lo1, lo2, hi1, hi2 };
    for (int i = 0; i < 4; i++) {
        CompressionEffect ci(nullptr);
        ci.SetParameters(p);
        o[i] = CompOut(ci, in[i]);
    }
    double below = (o[1] - o[0]) / (lo2 - lo1), above = (o[3] - o[2]) / (hi2 - hi1);
    double r = above / below;
    Gate("fx-compress-ratio", std::fabs(r - 0.25) < 0.01 && below > 0,
         "slope below %.5f, above %.5f, ratio %.4f (want 1/4)", below, above, r);

    // A -60 dB signal is under the -40 dB gate, so every frame takes the gate
    // branch: target gain 0, and the envelope (starting at 1) falls with the
    // gate's own release, mPeakReleaseTime 1.01 s. After CompOut's 96,000
    // frames the output is 0.001 * exp(-96000 / (1.01 * 48000)). Ungated, the
    // same input would come out at 0.001 times the 4.5 dB makeup gain (1.679).
    CompressionEffect cg(nullptr);
    cg.SetParameters(p);
    float g = CompOut(cg, 0.001f);
    double gateModel = 0.001 * std::exp(-96000.0 / (1.01 * 48000.0));
    Gate("fx-compress-gate", std::fabs(g / gateModel - 1) < 0.02,
         "-60 dB in, out %.6g after 2 s (gate-release model %.6g, ungated %.6g)", g,
         gateModel, 0.001 * below);
}

// ---------------------------------------------------------------------------
// synth/Utl
// ---------------------------------------------------------------------------
void UtlGates() {
    double worst = 0;
    for (int st = -24; st <= 24; st += 7) {
        double want = std::pow(2.0, st / 12.0);
        double got = CalcSpeedFromTranspose((float)st);
        double back = CalcTransposeFromSpeed((float)want);
        worst = std::fmax(worst, std::fabs(got / want - 1));
        worst = std::fmax(worst, std::fabs(back - st) / 12.0);
    }
    Gate("synth-utl-transpose", worst < 1e-5, "worst relative error %.2e over -24..+24", worst);

    // 120 bpm: a quarter note is 2 Hz; the measure table is in quarter notes
    static const struct {
        const char *sym;
        double quarters;
    } kM[] = { { "sixteenth", 0.25 }, { "eighth", 0.5 }, { "dotted_eighth", 0.75 },
               { "quarter", 1.0 },    { "dotted_quarter", 1.5 }, { "half", 2.0 },
               { "whole", 4.0 } };
    bool ok = true;
    for (auto &m : kM) {
        double got = CalcRateForTempoSync(Symbol(m.sym), 120.0f);
        ok = ok && std::fabs(got - 2.0 / m.quarters) < 1e-5;
    }
    double unk = CalcRateForTempoSync(Symbol("w16tm_not_a_measure"), 90.0f);
    Gate("synth-utl-tempo-sync", ok && std::fabs(unk - 1.5) < 1e-5,
         "7 measures at 120 bpm, unknown symbol at 90 bpm -> %g (want 1.5)", unk);
}

// ---------------------------------------------------------------------------
// FlowIf
// ---------------------------------------------------------------------------
struct ProbeNode : public FlowNode {
    int hits = 0;
    virtual bool Activate() override {
        hits++;
        return false; // ran in full
    }
};
struct ProbeIf : public FlowIf {
    void Set(const DataNode &a, const DataNode &b, int op) {
        mValue1 = a;
        mValue2 = b;
        mOperator = (OperatorType)op;
    }
    void Add(FlowNode *n) { mChildNodes.push_back(n); }
};

void FlowGates() {
    ProbeIf *f = new ProbeIf();
    ProbeNode *child = new ProbeNode();
    f->Add(child);
    struct Case {
        DataNode a, b;
        double x, y;
    };
    Case cases[] = { { DataNode(3), DataNode(5), 3, 5 },
                     { DataNode(5), DataNode(5), 5, 5 },
                     { DataNode(7), DataNode(5), 7, 5 },
                     { DataNode(2.5f), DataNode(-1.0f), 2.5, -1 },
                     { DataNode(4), DataNode(4.0f), 4, 4 },
                     { DataNode(4), DataNode(4.5f), 4, 4.5 } };
    int bad = 0, runs = 0;
    char first[160] = "";
    for (auto &c : cases) {
        // DataNode equality converts an int to float against a float (retail's
        // operator== and the native Equal agree), so every operator is the
        // plain numeric comparison.
        bool want[6] = { c.x == c.y, c.x != c.y, c.x > c.y, c.x >= c.y, c.x < c.y, c.x <= c.y };
        for (int op = 0; op < 6; op++) {
            f->Set(c.a, c.b, op);
            int before = child->hits;
            f->Activate();
            bool got = child->hits != before;
            runs++;
            if (got != want[op]) {
                if (!bad)
                    snprintf(first, sizeof(first), "first miss: %g op%d %g -> %d", c.x, op,
                             c.y, got);
                bad++;
            }
        }
    }
    // a non-numeric operand never satisfies an ordering operator
    f->Set(DataNode(Symbol("w16tm")), DataNode(1), 2);
    int before = child->hits;
    f->Activate();
    bool symOk = child->hits == before;
    Gate("flow-if-operators", bad == 0 && symOk, "%d/%d cases wrong, symbol > 1 %s; %s", bad,
         runs, symOk ? "false" : "TRUE", first);
}

} // namespace

int RunW16TMPhase(void (*gate)(const char *, bool, const char *)) {
    gGate = gate;
    printf("\n=== W16-TM: VIA-DC3 files first run natively ===\n");
    CryptoGates();
    MoggGates();
    CompressGates();
    UtlGates();
    FlowGates();
    return 0;
}
