#include "meta/StreamPlayer.h"
#include "obj/Data.h"
#include "obj/Object.h"
// Retail compiled this TU's message Handle WITHOUT the MessageTimer
// instrumentation: the target ?Handle@StreamPlayer@ goes straight from
// `Symbol sym = _msg->Sym(1)` to the set_volume static-symbol compare (no Timer
// construction, 0xD4 not 0x1BC).
//
// This used to be enforced with a TU-local `#undef MILO_DEBUG` here.  That
// rationale is now DEAD: the timer arm of BEGIN_HANDLERS is gated on
// `MILO_DEBUG && HX_NATIVE` in obj/ObjMacros.h and on MILO_MESSAGE_TIMERS in
// obj/Object.h, so the lean retail Handle is what this TU gets either way.
// The #undef's only surviving effect was to flip the inline OBJ_SET_TYPE arm for
// the 5 classes declared after it (Sound, MsgSource, WaitSeq, FxSendPitchShift,
// AudioDuckerTrigger) -- i.e. this TU emitted different inline virtual bodies than
// the rest of the tree, an ODR violation.  Proven harmless *and* pointless by
// compiling this TU both ways (lane CB-10/D): the two .objs differ in 2 bytes, the
// COFF timestamp and the embedded probe filename -- 0 real bytes.  So: removed.
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "synth/Stream.h"
#include "synth/Synth.h"

// Retail's HANDLE_CHECK kept the PathName(this) side-effect call in the _warn
// branch (the target ?Handle@StreamPlayer@ unhandled path is `if (_warn)
// PathName(this)`), but our default MILO_WARN is sizeof-stripped which discards
// the argument evaluation. Locally evaluate-but-discard the args (comma expr) so
// PathName is emitted while the format string / __FILE__ / line literal produce
// no code, matching retail exactly.
#undef MILO_WARN
#define MILO_WARN(...) ((void)(__VA_ARGS__))

StreamPlayer::StreamPlayer()
    : mMasterVol(1.0f), mStreamVol(1.0f), mLoop(0), mStarted(0), mPaused(0), mStream(0) {}

void StreamPlayer::StopPlaying() { Delete(); }

StreamPlayer::~StreamPlayer() { Delete(); }

void StreamPlayer::PlayFile(char const *cc, float f1, float f2, bool b) {
    Delete();
    mStream = TheSynth->NewStream(cc, 0.0f, 0.0f, true);
    MILO_ASSERT(mStream, 0x2c);
    mStarted = false;
    mLoop = b;
    mStreamVol = f1;
}

void StreamPlayer::Poll() {
    if (!mStream || mPaused) {
        return;
    } else {
        if (!mStream->IsPlaying()) {
            if (mStream->IsReady()) {
                if (!mStarted) {
                    Init();
                    mStarted = true;
                }
                mStream->Play();
            } else if (mStarted) {
                mStarted = false;
                Delete();
            }
        }
    }
}

void StreamPlayer::Delete() {
    if (mStream)
        mStream->Stop();
    delete mStream;
    mStream = 0;
}

void StreamPlayer::Init() {
    mStream->SetVolume(mStreamVol * mMasterVol);
    MILO_ASSERT(mStream->GetNumChannels() == 2, 0x68);
    mStream->SetPan(0, -1.0f);
    mStream->SetPan(1, 1.0f);
    if (mLoop) {
        // retail constant is 0xB4000000 = -1.1920928955078125e-07f (-2^-23,
        // i.e. -FLT_EPSILON) here, not -0.25f (0xBE800000, matches dc3/rb3-Wii);
        // jump-start offset for the loop point.
        mStream->SetJump(-1.1920929e-07f, 0, 0);
    }
}

void StreamPlayer::SetVolume(float value) {
    mMasterVol = value;
    if (mStream) {
        mStream->SetVolume(value * mStreamVol);
    }
}

BEGIN_HANDLERS(StreamPlayer)
    HANDLE_ACTION_STATIC(set_volume, SetVolume(_msg->Float(2)))
    HANDLE_CHECK(0xA9)
END_HANDLERS
