#include "synth/MoggClip.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/File.h"
#include "synth/Stream.h"
#include "synth/Synth.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"

MoggClip::PanInfo::PanInfo(int c, float p) : channel(c), panning(p) {}

MoggClip::MoggClip()
    : mControllerVolume(0), mLoop(false), mVolume(0), mStream(nullptr), unk50(0),
      mData(nullptr), mDataSize(0), mLoader(nullptr), mFader(Hmx::Object::New<Fader>()),
      mUnloadWhenFinished(false), mPlaying(false), mLoopStartSample(0), mLoopEndSample(-1) {
    mFaders.push_back(mFader);
    StartPolling();
}

void MoggClip::SetSend(FxSend *send) { mFxSend = send; }

MoggClip::~MoggClip() {
    RELEASE(mLoader);
    RELEASE(mFader);
    KillStream();
    UnloadData();
}

BEGIN_HANDLERS(MoggClip)
    HANDLE_ACTION(play, Play(0))
    HANDLE_ACTION(stop, Stop())
    HANDLE_ACTION(set_pan, SetPan(_msg->Int(2), _msg->Float(3)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

// Retail's property list is 5 long, not 2 -- the missing loop / loop_start_sample
// / loop_end_sample arms are the bulk of the 112 target-only instructions this
// function used to carry.  Order and member offsets read off the target
// (fn_8270DFB8): file 0x34, volume 0x40, loop 0x44 (SetLoop(_val.Int() != 0)),
// loop_start_sample 0x80 and loop_end_sample 0x84 (both stores inlined at the
// call site).  Matches the rb3-Wii oracle's MoggClip::SyncProperty exactly.
BEGIN_PROPSYNCS(MoggClip)
    SYNC_PROP_SET(file, mMoggFile, SetFile(_val.Str()))
    SYNC_PROP_SET(volume, mControllerVolume, SetControllerVolume(_val.Float()))
    SYNC_PROP_SET(loop, mLoop, SetLoop(_val.Int() != 0))
    SYNC_PROP_SET(loop_start_sample, mLoopStartSample, SetLoopStart(_val.Int()))
    SYNC_PROP_SET(loop_end_sample, mLoopEndSample, SetLoopEnd(_val.Int()))
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(MoggClip)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mMoggFile << mControllerVolume << mLoop;
    bs << mLoopStartSample << mLoopEndSample;
    if (bs.Cached()) {
        FileLoader::SaveData(bs, mData, mDataSize);
    }
END_SAVES

BEGIN_COPYS(MoggClip)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(MoggClip)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMoggFile)
        COPY_MEMBER(mControllerVolume)
        COPY_MEMBER(mLoop)
        COPY_MEMBER(mLoopStartSample)
        COPY_MEMBER(mLoopEndSample)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(MoggClip)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

INIT_REVS(2, 0)

void MoggClip::PreLoad(BinStream &bs) {
    int rev;
    bs >> rev;
    if (rev > 2)
        MILO_WARN("Can't load new MoggClip");
    else {
        Hmx::Object::Load(bs);
        bs >> mMoggFile >> mControllerVolume >> mLoop;
        if (rev > 1)
            bs >> mLoopStartSample >> mLoopEndSample;
        LoadFile(rev > 0 ? &bs : 0);
    }
}

void MoggClip::PostLoad(BinStream &bs) {
    EnsureLoaded();
    LoadNumChannels();
}

const char *MoggClip::GetSoundDisplayName() {
    return !mPlaying ? gNullStr
                     : MakeString("MoggClip: %s", FileGetName(mMoggFile.c_str()));
}

void MoggClip::SynthPoll() {
    if (mPlaying && mStream) {
        mStream->PollStream();
        if (!mStream->IsPlaying() && mStream->IsReady()) {
            if (mPanInfos.empty()) {
                int chans = mStream->GetNumChannels();
                if (chans == 1) {
                    mStream->SetPan(0, 0);
                } else if (chans == 2) {
                    mStream->SetPan(0, -1);
                    mStream->SetPan(1, 1);
                }
            }
            mStream->Play();
        } else {
            if (mStream->IsFinished() || mFader->mVal == -96.0f) {
                MoggClip::Stop();
            }
        }
    }
}

// Retail 0x8270DE60, 100% (all 63 instructions equal).  EnsureLoaded ->
// KillStream -> NewBufStream("mogg", 0.0f, false, true) -> mFader->SetVal(0) ->
// SetLoop(mLoop) -> SetControllerVolume(mControllerVolume) -> UpdateFaders ->
// UpdatePanInfo -> mPlaying = true.
//
// The volume step is an INLINED SetControllerVolume(mControllerVolume), not an
// ad-hoc `if (mStream) SetVolume(mVolume + mControllerVolume)`.  Both forms
// compute the same value, but only the inlined-call form reproduces retail's
// scheduling: the argument (mControllerVolume, 0x40) is loaded EARLY, in the
// load-use stall slot between `lwz r3,0x4c` and `cmplwi`, and lands in f0 --
// which is what fixes the f0<->f13 pairing.  Confirmed by shape: retail's
// out-of-line SetControllerVolume (fn_8270D720) emits `fadds f1, f0(other), f1(arg)`,
// and the Play tail emits `fadds f1, f13(0x48=other), f0(0x40=arg)` -- the same
// (other, arg) order.  Play(float) below already uses this same idiom.
void MoggClip::Play() {
    if (EnsureLoaded()) {
        KillStream();
        mStream = dynamic_cast<StandardStream *>(
            TheSynth->NewBufStream(mData, mDataSize, "mogg", 0, false, true)
        );
        mFader->SetVal(0);
        SetLoop(mLoop);
        SetControllerVolume(mControllerVolume);
        UpdateFaders();
        UpdatePanInfo();
        mPlaying = true;
    }
}

void MoggClip::Play(float f1) {
    if (EnsureLoaded()) {
        KillStream();
        Stream *stream = TheSynth->NewBufStream(mData, mDataSize, "mogg", 0, false, true);
        mStream = dynamic_cast<StandardStream *>(stream);
        if (!mStream) {
            delete stream;
        } else {
            mFader->SetVal(0);
            SetVolume(f1);
            SetControllerVolume(mControllerVolume);
            UpdateFaders();
            UpdatePanInfo();
            ApplyLoop(mLoop, mLoopStartSample, mLoopEndSample);
            mPlaying = true;
        }
    } else
        MILO_WARN("Mogg file not loaded: '%s'", mMoggFile.c_str());
}

void MoggClip::Stop() {
    KillStream();
    if (mUnloadWhenFinished) {
        UnloadData();
    }
}

void MoggClip::Pause(bool pause) {
    mPlaying = !pause;
    if (mStream && !mPlaying) {
        mStream->Stop();
    }
}

bool MoggClip::DonePlaying() { return !mStream; }

void MoggClip::SetVolume(float vol) {
    mVolume = vol;
    if (mStream) {
        mStream->Stream::SetVolume(mControllerVolume + mVolume);
    }
}

void MoggClip::SetControllerVolume(float vol) {
    mControllerVolume = vol;
    if (mStream) {
        mStream->Stream::SetVolume(mControllerVolume + mVolume);
    }
}

void MoggClip::SetPan(float f1) {
    if (mNumChannels == 1) {
        SetPan(0, f1);
    }
}

void MoggClip::EndLoop() { SetLoop(false, mLoopStartSample, mLoopEndSample); }

bool MoggClip::IsStreaming() const { return mStream && mStream->IsPlaying(); }

void MoggClip::ApplyLoop(bool b1, int i2, int i3) {
    if (mStream) {
        mStream->ClearJump();
        if (b1) {
            mStream->SetJumpSamples(i3, i2, 0);
        }
    }
}

// AT_LIMIT -- 75%, and NOT source-fixable.  Our 20-byte body (4 insns + a tail
// `b Fader::DoFade`) is CORRECT.  Retail's fn_8270D7E8 is only 16 bytes because it
// FALLS THROUGH into fn_8270D7F8, a separate 8-byte .pdata entry holding just
// `b fn_8270C4B0` (+ a padding word).  The missing 5th instruction is a target
// EXTENT artifact, not a codegen difference -- no C++ source can emit a function
// that falls off its own end.  => the MoggClip unit can never reach 100% (ceiling
// 55/56); do not fund a unit-completion attempt here.
void MoggClip::FadeOut(float f1) { mFader->DoFade(-96.0f, f1); }

void MoggClip::UnloadWhenFinishedPlaying(bool unload) { mUnloadWhenFinished = unload; }

bool MoggClip::IsReadyToPlay() const {
    if (mLoader)
        return mLoader->IsLoaded();
    else
        return mData && mDataSize > 0;
}

void MoggClip::KillStream() {
    mPlaying = false;
    RELEASE(mStream);
}

void MoggClip::UnloadData() {
    if (mData) {
        MemFree(mData);
        mData = nullptr;
        mDataSize = 0;
    }
}

void MoggClip::SetLoop(bool b) {
    mLoop = b;
    if (mStream) {
        mStream->ClearJump();
        if (mLoop) {
            mStream->SetJumpSamples(mLoopEndSample, mLoopStartSample, 0);
        }
    }
}

void MoggClip::SetLoop(bool b1, int i2, int i3) {
    mLoop = b1;
    mLoopStartSample = i2;
    mLoopEndSample = i3;
    ApplyLoop(mLoop, mLoopStartSample, mLoopEndSample);
}

bool MoggClip::EnsureLoaded() {
    if (mLoader) {
        if (!mLoader->IsLoaded()) {
            MILO_WARN("MoggClip blocked while loading '%s'", mMoggFile.c_str());
            TheLoadMgr.PollUntilLoaded(mLoader, nullptr);
        }
        mData = mLoader->GetBuffer(&mDataSize);
        RELEASE(mLoader);
    }
    return mData && mDataSize > 0;
}

void MoggClip::UpdateFaders() {
    if (mStream) {
        for (std::vector<Fader *>::iterator it = mFaders.begin(); it != mFaders.end();
             ++it) {
            mStream->Faders()->Add(*it);
        }
    }
}

void MoggClip::UpdatePanInfo() {
    if (mStream) {
        for (std::vector<PanInfo>::iterator it = mPanInfos.begin(); it != mPanInfos.end();
             ++it) {
            mStream->SetPan(it->channel, it->panning);
        }
    }
}

void MoggClip::LoadNumChannels() {
    if (mMoggFile.empty()) {
        mNumChannels = -1;
        return;
    }
    if (mLoader && !mLoader->IsLoaded()) {
        TheLoadMgr.PollUntilLoaded(mLoader, nullptr);
    }
    SynthPoll();
    if (!mStream) {
        mNumChannels = -1;
        return;
    }
    int retries = 0;
    int numChannels = 0;
    while (retries < 200) {
        Timer::Sleep(1);
        TheSynth->Poll();
        numChannels = mStream->GetNumChannels();
        if (numChannels >= 1) {
            break;
        }
        retries++;
    }
    mNumChannels = numChannels;
    Stop();
    if (mNumChannels < 0) {
        mNumChannels = -1;
    }
}

void MoggClip::LoadFile(BinStream *bs) {
    RELEASE(mLoader);
    KillStream();
    UnloadData();
    if (!mMoggFile.empty()) {
        BinStream *toUse = (bs && bs->Cached()) ? bs : 0;
        mLoader = new FileLoader(
            mMoggFile,
            FileLocalize(mMoggFile.c_str(), nullptr),
            kLoadFront,
            0,
            false,
            true,
            toUse
        );
    }
}

void MoggClip::SetFile(const char *file) {
    MILO_ASSERT(file != NULL, 0x14C);
    mMoggFile.Set(FilePath::Root().c_str(), file);
    LoadFile(nullptr);
}

void MoggClip::RemoveFader(Fader *fader) {
    if (fader) {
        for (std::vector<Fader *>::iterator it = mFaders.begin(); it != mFaders.end();
             ++it) {
            if (*it == fader) {
                mFaders.erase(it);
                break;
            }
        }
        if (mStream) {
            mStream->Faders()->Remove(fader);
        }
    }
}

void MoggClip::AddFader(Fader *fader) {
    if (fader) {
        bool found = false;
        for (std::vector<Fader *>::iterator it = mFaders.begin(); it != mFaders.end();
             ++it) {
            if (*it == fader) {
                found = true;
                break;
            }
        }
        if (!found) {
            mFaders.push_back(fader);
        }
        if (mStream) {
            mStream->Faders()->Add(fader);
        }
    }
}

void MoggClip::SetPan(int i1, float f2) {
    PanInfo info(i1, f2);
    bool found = false;
    for (std::vector<PanInfo>::iterator it = mPanInfos.begin(); it != mPanInfos.end();
         ++it) {
        if (it->channel == i1) {
            found = true;
            *it = info;
            break;
        }
    }
    if (!found) {
        mPanInfos.push_back(info);
    }
    if (mStream) {
        mStream->SetPan(info.channel, info.panning);
    }
}

// Retail fn @ SetupPanInfo loads TWO DISTINCT constants: -0.5f (lbl_820392FC) for
// channel 0 and +0.5f (lbl_82075090) for channel 1 -- read straight out of
// band.exe .rdata, not inferred.  Our port had the two channels SWAPPED (and used
// a `2.0` double literal on the second), i.e. left/right pan reversed.
//
// objdiff scored the swapped version HIGHER (92.9% vs 78.7%) and marked the
// constant loads `equal`, because the constant is a RELOCATION ARGUMENT and those
// are masked -- the documented "a wrong constant reads 100%" hazard, live.  Do not
// trust the score here; adjudicate on the retail bytes.
//
// 78.7% is a CEILING, not a defect: retail recomputes with a second constant, but
// MSVC /fp:fast canonicalises `x * -0.5f` to `-(x * 0.5f)` and CSEs the multiply
// into callee-saved f30 across the intervening SetPan call.  THREE distinct source
// forms were tried -- `-f2/2.0f`, `f2/-2.0f`, and explicit `f2 * -0.5f` -- and all
// three produced BYTE-IDENTICAL output.  Codegen wall, permuter-class; do not
// re-dig from source.  Costs nothing: the row pays 0 bytes at both 92.9% and 78.7%
// (matched_code is all-or-nothing per row), so correctness is free here.
//
// lane EA-1: a FOURTH form refuted -- both channels written as explicit multiplies
// SIMULTANEOUSLY (`f2 * -0.5f + f1` AND `f2 * 0.5f + f1`).  This was worth trying
// because DX-3's third form changed only channel 0 while channel 1 stayed a
// division, which keeps the shared `f2*0.5` alive no matter what channel 0 says --
// so the CSE had never actually been denied both of its operands at once.  It
// still emits byte-identical code: 78.7%, the same 7 mismatches.  The two retail
// constants were re-verified straight from .rdata rather than inherited:
// lbl_820392FC = 0xBF000000 = -0.5, lbl_82075090 = 0x3F000000 = +0.5, so the
// channels in this file are the CORRECT way round and the higher-scoring variant
// is the wrong one.  MoggClip is 54/55 and this is the residual row; it is a
// codegen wall, NOT "one source fix from complete".
void MoggClip::SetupPanInfo(float f1, float f2, bool stereo) {
    if (stereo) {
        SetPan(0, -f2 / 2.0f + f1);
        SetPan(1, f2 / 2.0f + f1);
    } else {
        SetPan(0, f1);
    }
}

// sw2 scatter-include (default/system/synth/MoggClip <- rndobj/CubeTex.cpp)
#define gRev gRev_CubeTex
#define gAltRev gAltRev_CubeTex
#include "rndobj/CubeTex.cpp"
#undef gRev
#undef gAltRev
