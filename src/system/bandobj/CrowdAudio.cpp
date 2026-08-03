#include "bandobj/CrowdAudio.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Task.h"
#include "synth/MoggClip.h"
#include "synth/Sequence.h"
#include "utl/Messages.h"
#include "utl/Symbols.h"

// RB3-360 retail rev storage. Retail folds both rev words onto ONE base register
// at +0/+4, which only happens for internal-linkage align(4) file-scope storage
// laid out as a single aggregate (altRev at +0, rev at +4). Two separate statics
// do NOT fold: MSVC interleaves the TU's other .bss objects between them.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs;
#define gAltRev gRevs.altRev
#define gRev gRevs.rev

// RB3-360 retail OPEN-CODES the inlined ObjRefConcrete<MoggClip,ObjectDir>::
// SetObjConcrete(0) at every `mMogg = 0` site in this TU, rather than emitting
// the `bl`.  Verified two ways on ?Poll@CrowdAudio@@UAAXXZ: the target asm at
// idx 68-74 is `lwz r3,0x1c(r30) / addi r29,r30,0x14 / cmplwi / beq / mr r4,r29
// / bl <Hmx::Object::Release> / stw r28,8(r29)`, and retail's Ghidra decomp
// reads `if (*(this+0x1c)) { Function_8275B378(*(this+0x1c), this+0x14);
// *(this+0x1c) = 0; }`.  That is exactly SetObjConcrete's retail body with the
// `obj != mObject` guard folded against the null argument and the trailing
// AddRef arm folded away.
//
// This is the per-T1 inline-policy split documented at length in
// obj/ObjPtr_p.h (see ObjRefVirtualBaseObject): MSVC /O1 /Ob2 inlines
// SetObjConcrete(0) for a T1 that reaches Hmx::Object WITHOUT a virtual base --
// MoggClip qualifies -- and declines for the virtual-base instantiations, which
// would first need the vbase displacement lookup.  Our compiler declines in
// BOTH cases, so the non-virtual arm has to be written out at the call site,
// the same way ObjRefConcrete::Load's no-dir path already does it.
//
// mObject is protected in ObjRefConcrete and that class lives in the
// codegen-sacred obj/Object.h, so access is taken through a layout-identical
// derived type: it declares no members and is never instantiated, so no vtable,
// RTTI or dtor COMDAT is emitted for it (confirmed against the unit's symbol
// list after the change).
//
// HX_NATIVE keeps the plain assignment: native's SetObjConcrete additionally
// handles InDeleteObjects()/sRingsDirty via SafeReleaseFromRing, which
// open-coding would silently skip.
namespace {
    struct MoggPtrRef : public ObjPtr<MoggClip> {
        static __forceinline void Clear(ObjPtr<MoggClip> &p) {
#ifdef HX_NATIVE
            p = nullptr;
#else
            MoggPtrRef &r = static_cast<MoggPtrRef &>(p);
            if (r.mObject) {
                r.mObject->Release(&r);
                r.mObject = 0;
            }
#endif
        }
    };
}

CrowdAudio *TheCrowdAudio;

void CrowdAudio::Init() { Register(); }

CrowdAudio::CrowdAudio()
    : mCurrentMogg(this, 0), mOldMogg(this, 0), mFadingMogg(this, 0),
      mMainFader(Hmx::Object::New<Fader>()), mWantDuck(0), mResultsDuck(0),
      mResultsFadeDuration(1000.0f), mResultsFader(Hmx::Object::New<Fader>()),
      mFadeInFromLoadingDuration(1000.0f), mEntryFader(Hmx::Object::New<Fader>()),
      mVenueChangeFadeDuration(1000.0f), mLevel(kExcitementBad), mLoopChangeTime(1e+30f),
      mIntro(0), mVenueIntro(0), mLevels(0), mVenueOutro(0), mState(0), mCrowdVol(0),
      mCamShotVol(0), mEnabled(1), mCrowdReacts(1), mLastClapBeat(0), mClapAllowed(1),
      mBank(this, 0), mCurrentBankFader(0), mOtherBankFader(0),
      mReleaseFader(Hmx::Object::New<Fader>()), mCrossfadeDuration(1000.0f),
      mReleaseTime(5000.0f), mPaused(0), mShouldPlayVenueIntro(0),
      mShouldPlayVenueOutro(0), mWon(0), mRestarting(1),
      mCloseupFader(Hmx::Object::New<Fader>()), mCloseupFadeDuration(1000.0f) {
    mOverrideExcitementLevel = (ExcitementLevel)-1;
    mOverrideExcitementLevelPrev = (ExcitementLevel)-1;
    if (!TheCrowdAudio) {
        TheCrowdAudio = this;
        static DataNode &crowd_audio = DataVariable("crowd_audio");
        crowd_audio = DataNode(this);
    }
    mEntryFader->SetMode(Fader::kInvExp);
}

CrowdAudio::~CrowdAudio() {
    if (TheCrowdAudio == this) {
        TheCrowdAudio = 0;
        static DataNode &crowd_audio = DataVariable("crowd_audio");
        crowd_audio = DataNode(0);
    }
    StopAllMoggs();
    RELEASE(mMainFader);
    RELEASE(mResultsFader);
    RELEASE(mCurrentBankFader);
    RELEASE(mOtherBankFader);
    RELEASE(mReleaseFader);
    RELEASE(mCloseupFader);
    RELEASE(mEntryFader);
}

void CrowdAudio::Enter() {
    RndPollable::Enter();
    mState = 0;
    mWon = false;
    mLoopChangeTime = 1e+30f;
    mLastClapBeat = 0;
    static Message want_outro_duck_msg("want_outro_duck");
    mWantDuck = HandleType(want_outro_duck_msg).Int();
    SetEnabled(true);
    SetPaused(false);
}

void CrowdAudio::Exit() {
    RndPollable::Exit();
    SetEnabled(false);
    StopAllMoggs();
    StopSequence("win_4.cue");
    StopSequence("win_5.cue");
    mRestarting = true;
}

void CrowdAudio::Poll() {
    static DataNode &override_crowd_audio_level =
        DataVariable("override_crowd_audio_level");
    if (override_crowd_audio_level.Int()) {
        mOverrideExcitementLevel = (ExcitementLevel)override_crowd_audio_level.Int();
        if (mOverrideExcitementLevelPrev != mOverrideExcitementLevel)
            SetExcitement(mOverrideExcitementLevel);
    } else
        mOverrideExcitementLevel = (ExcitementLevel)-1;
    mOverrideExcitementLevelPrev = mOverrideExcitementLevel;
    if (mEnabled) {
        float secs = TheTaskMgr.Seconds(TaskMgr::kDelayedTime);
        float othersecs = secs * 1000.0f;
        if (secs > mLoopChangeTime)
            PlayExcitementLoop();
        if (mOldMogg && mReleaseFader->mVal == -96.0f) {
            mOldMogg->MoggClip::Stop();
            MoggPtrRef::Clear(mOldMogg);
        }
        if (mCurrentMogg && mCurrentMogg->IsStreaming() && mOldMogg
            && !mReleaseFader->IsFading()) {
            mReleaseFader->CancelFade();
            mReleaseFader->SetMode(Fader::kInvExp);
            mReleaseFader->DoFade(-96.0f, mReleaseTime);
        }
        if (mFadingMogg && mOtherBankFader->mVal == -96.0f) {
            mFadingMogg->MoggClip::Stop();
            MoggPtrRef::Clear(mFadingMogg);
        }
        MaybeClap(othersecs);
    }
}

void CrowdAudio::SetPaused(bool b) {
    if (b != mPaused) {
        if (mState < 4) {
            mPaused = b;
            if (mPaused && mOldMogg) {
                mOldMogg->MoggClip::Stop();
                MoggPtrRef::Clear(mOldMogg);
            }
            if (mCurrentMogg)
                mCurrentMogg->MoggClip::Pause(mPaused);
            if (mFadingMogg) {
                mFadingMogg->MoggClip::Pause(mPaused);
                if (mPaused) {
                    mCurrentBankFader->CancelFade();
                    mOtherBankFader->CancelFade();
                    mReleaseFader->CancelFade();
                } else {
                    mCurrentBankFader->DoFade(0.0f, mCrossfadeDuration);
                    mOtherBankFader->DoFade(-96.0f, mCrossfadeDuration);
                    mReleaseFader->DoFade(-96.0f, mCrossfadeDuration);
                }
            }
            if (!mPaused)
                UpdateVolume();
        }
    }
}

void CrowdAudio::SetExcitement(ExcitementLevel level) {
    if (mOverrideExcitementLevel != (ExcitementLevel)-1)
        level = mOverrideExcitementLevel;
    if (level >= kNumExcitements)
        MILO_FAIL("Invalid excitement level: %d", level);
    if (mState != 2)
        mLevel = level;
    else {
        static const char *upSfx[5] = { "crowd_upto_poor",
                                        "crowd_upto_poor",
                                        "crowd_upto_norm",
                                        "crowd_upto_good",
                                        "crowd_upto_peak" };
        static const char *downSfx[5] = { "crowd_dnto_danger",
                                          "crowd_dnto_poor",
                                          "crowd_dnto_norm",
                                          "crowd_dnto_good",
                                          "crowd_dnto_good" };
        PlaySequence(((level > mLevel) ? upSfx : downSfx)[level]);

        mLevel = level;
        mLoopChangeTime = TheTaskMgr.Seconds(TaskMgr::kDelayedTime) + 1.0f;
    }
}

bool CrowdAudio::PlayExcitementLoop() {
    mLoopChangeTime = 1e+30f;
    return PlayLoop(mLevels->Array(mLevel + 1), false);
}

bool CrowdAudio::PlayLoop(const DataArray *loopInfo, bool force) {
    MILO_ASSERT(loopInfo != NULL, 0x163);
    MoggClip *clip = 0;
    const char *clipname = loopInfo->Str(1);
    if (mBank) {
        clip = mBank->Find<MoggClip>(clipname, false);
        if (!clip) {
            MILO_WARN("%s not found in %s_bank.milo", clipname, mBank->Name());
        }
    }
    if (!clip)
        return false;
    else {
        bool b2 = false;
        if (clip != mCurrentMogg) {
            if (clip != mOldMogg) {
                b2 = true;
            } else {
                if (mCurrentMogg)
                    mCurrentMogg->MoggClip::Stop();
                mOldMogg->RemoveFader(mReleaseFader);
                mCurrentMogg = mOldMogg;
                mOldMogg = 0;
            }
        }
        if (b2 || force) {
            if (mOldMogg) {
                if (mCurrentMogg)
                    mCurrentMogg->MoggClip::Stop();
            } else if (clip == mCurrentMogg) {
                MILO_ASSERT(force, 0x1AA);
            } else {
                mOldMogg = mCurrentMogg;
                if (mOldMogg) {
                    mOldMogg->AddFader(mReleaseFader);
                    mReleaseFader->SetVal(0.0f);
                }
            }
            mCurrentMogg = clip;
            DataArray *loopArr = loopInfo->Array(2);
            float pan = (loopArr->Float(1) + loopArr->Float(2)) * 0.5f;
            mCurrentMogg->SetPan(0, pan);
            mCurrentMogg->SetPan(1, pan);
            mCurrentMogg->AddFader(mMainFader);
            mCurrentMogg->AddFader(mEntryFader);
            mCurrentMogg->AddFader(mResultsFader);
            mCurrentMogg->AddFader(mCurrentBankFader);
            mCurrentMogg->RemoveFader(mOtherBankFader);
            mCurrentMogg->RemoveFader(mReleaseFader);
            mCurrentMogg->Play(0.0f);
            SetPaused(mPaused);
        }
        return true;
    }
}

void CrowdAudio::PlaySequence(const char *cc) {
    Sequence *seq = 0;
    if (mBank) {
        seq = mBank->Find<Sequence>(cc, false);
        if (!seq)
            MILO_WARN("%s not found in %s_bank.milo", cc, mBank->Name());
    }
    if (seq) {
        seq->Faders().Add(mMainFader);
        seq->Faders().Add(mEntryFader);
        seq->Faders().Add(mResultsFader);
        seq->Faders().Add(mCurrentBankFader);
        seq->Faders().Remove(mOtherBankFader);
        seq->Play(0, 0, 0);
    }
}

void CrowdAudio::StopSequence(const char *cc) {
    Sequence *seq = 0;
    if (mBank)
        seq = mBank->Find<Sequence>(cc, false);
    if (seq)
        seq->Stop(true);
}

void CrowdAudio::PlayCloseupAudio() {
    const char *cue_name = "crowd_closeup.cue";
    Sequence *seq = 0;
    if (mBank)
        seq = mBank->Find<Sequence>(cue_name, false);
    if (seq) {
        seq->Stop(false);
        seq->Faders().Add(mCloseupFader);
    }
    mCloseupFader->SetVal(0.0f);
    PlaySequence(cue_name);
}

void CrowdAudio::StopCloseupAudio() {
    mCloseupFader->DoFade(-96.0f, mCloseupFadeDuration);
}

void CrowdAudio::UpdateVolume() {
    static DataNode &volumeSetting = DataVariable("crowd_audio.volume");
    float f1 = mCrowdVol + mCamShotVol + volumeSetting.Float();
    if (f1 > 3.0f) {
        MILO_WARN("Excessive crowd audio volume!\n");
        f1 = 3.0f;
    }
    mMainFader->SetVal(f1);
}

void CrowdAudio::SetEnabled(bool b) {
    Hmx::Object *src = Dir();
    if (src)
        src->RemoveSink(this);
    mEnabled = b;
    if (mEnabled) {
        if (src)
            src->AddSink(this);
        SetExcitement(mLevel);
    }
}

void CrowdAudio::StopAllMoggs() {
    if (mCurrentMogg) {
        mCurrentMogg->MoggClip::Stop();
        mCurrentMogg = 0;
    }
    if (mOldMogg) {
        mOldMogg->MoggClip::Stop();
        mOldMogg = 0;
    }
    if (mFadingMogg) {
        mFadingMogg->MoggClip::Stop();
        mFadingMogg = 0;
    }
}

// Lane DR-2: retail X360 runs this body UNCONDITIONALLY.  rb3-Wii's DEV build
// wraps it in `if (TypeDef() != arr)` and we inherited that, which cost exactly
// the three leading instructions retail does not have:
//     lwz r10, -168(r11) / cmplw cr6, r10, r4 / beq cr6, <end>
// This is the usual oracle trap running in reverse -- the oracle AGREED with our
// source and both were wrong for retail.  Adjudicated on retail bytes, per the
// standing rule.
void CrowdAudio::SetTypeDef(DataArray *arr) {
    Hmx::Object::SetTypeDef(arr);
    mIntro = 0;
    mLevels = 0;
    mVenueIntro = 0;
    mVenueOutro = 0;
    mClapOffsetMs = 0;
    mCrowdVol = 0;
    mCamShotVol = 0;
    mResultsDuck = 0;
    mResultsFadeDuration = 1000.0f;
    mVenueChangeFadeDuration = 1000.0f;
    mCloseupFadeDuration = 1000.0f;
    mCrossfadeDuration = 1000.0f;
    mReleaseTime = 5000.0f;
    mFadeInFromLoadingDuration = 1000.0f;
    if (arr) {
        arr->FindData("clap_early_amount_ms", mClapOffsetMs, false);
        arr->FindData("crowd_volume", mCrowdVol, false);
        DataArray *streamArr = arr->FindArray("streams", false);
        if (streamArr) {
            mIntro = streamArr->FindArray("intro");
            mLevels = streamArr->FindArray("levels");
            mVenueIntro = streamArr->FindArray("venue_intro");
            mVenueOutro = streamArr->FindArray("venue_outro");
        }
        arr->FindData("results_duck", mResultsDuck, false);
        arr->FindData("results_fade_ms", mResultsFadeDuration, false);
        arr->FindData("venue_change_fade_ms", mVenueChangeFadeDuration, false);
        arr->FindData("closeup_fade_ms", mCloseupFadeDuration, false);
        arr->FindData("crossfade_ms", mCrossfadeDuration, false);
        arr->FindData("release_ms", mReleaseTime, false);
        arr->FindData("fade_in_from_loading_ms", mFadeInFromLoadingDuration, false);
    }
    UpdateVolume();
}

void CrowdAudio::SetBank(ObjectDir *dir) {
    if (mBank != dir) {
        mBank = dir;
        if (!mCurrentBankFader || !mOtherBankFader) {
            Fader *newf = Hmx::Object::New<Fader>();
            if (!mCurrentBankFader) {
                mCurrentBankFader = newf;
                mCurrentBankFader->SetVal(0.0f);
            } else if (!mOtherBankFader) {
                mOtherBankFader = mCurrentBankFader;
                mCurrentBankFader = newf;
                mCurrentBankFader->SetVal(-96.0f);
            }
        } else {
            Fader *tmp = mCurrentBankFader;
            mCurrentBankFader = mOtherBankFader;
            mOtherBankFader = tmp;
        }
        if (mCurrentBankFader) {
            mCurrentBankFader->CancelFade();
            mCurrentBankFader->SetMode(Fader::kInvExp);
            mCurrentBankFader->DoFade(0.0f, mCrossfadeDuration);
        }
        if (mOtherBankFader) {
            mOtherBankFader->CancelFade();
            mOtherBankFader->SetMode(Fader::kExp);
            mOtherBankFader->DoFade(-96.0f, mCrossfadeDuration);
        }
        if (mState == 2) {
            if (mFadingMogg) {
                mFadingMogg->MoggClip::Stop();
                MoggPtrRef::Clear(mFadingMogg);
            }
            if (PlayExcitementLoop()) {
                mFadingMogg = mOldMogg;
            } else {
                mFadingMogg = mCurrentMogg;
                if (mOldMogg) {
                    mFadingMogg = mOldMogg;
                    if (mCurrentMogg) {
                        mCurrentMogg->MoggClip::Stop();
                    }
                }
                MoggPtrRef::Clear(mCurrentMogg);
            }
            MoggPtrRef::Clear(mOldMogg);
        }
    }
}

void CrowdAudio::Save(BinStream &) { MILO_ASSERT(0, 0x33F); }

BEGIN_LOADS(CrowdAudio)
    // RB3-360 retail uses the rb3-Wii rev dialect: the packed rev int is split
    // into two MUTABLE file-scope aligned(4) shorts (gRev/gAltRev) and the body
    // reads `bs` directly. No BinStreamRev shim, no ASSERT_REVS block.
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    RndPollable::Load(bs);
    if (gRev != 5) {
        if (gRev == 4) {
            int i;
            bs >> i >> i >> i;
        } else if (gRev == 3) {
            int j;
            bs >> j;
        } else if (gRev == 2) {
            String str;
            bs >> str;
        }
    }
END_LOADS
#undef gRev
#undef gAltRev

BEGIN_COPYS(CrowdAudio)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndPollable)
END_COPYS

void CrowdAudio::MaybeClap(float f) {
    if (mState < 3) {
        int beat = MsToLastClapBeat(f + mClapOffsetMs);
        if (beat != mLastClapBeat) {
            if (mCrowdReacts && mClapAllowed && mLevel == kExcitementPeak
                && beat > mLastClapBeat) {
                PlaySequence("claps");
            }
            mLastClapBeat = beat;
        }
    }
}

void CrowdAudio::OnIntro() {
    if (mRestarting || mShouldPlayVenueIntro) {
        DataArray *arr = mIntro;
        if (mShouldPlayVenueIntro)
            arr = mVenueIntro;
        PlayLoop(arr, mShouldPlayVenueIntro);
        mShouldPlayVenueIntro = false;
    }
    mState = 1;
    mRestarting = false;
    mResultsFader->DoFade(0, 1000.0f);
}

void CrowdAudio::OnLose() {
    mState = 4;
    mWantDuck = true;
}

void CrowdAudio::OnOutro() {
    if (mWantDuck) {
        mResultsFader->DoFade(mResultsDuck, mResultsFadeDuration);
    }
}

void CrowdAudio::OnMusicStart() {
    if (mState != 4) {
        ExcitementLevel old = mLevel;
        mState = 2;
        mLevel = (ExcitementLevel)-1;
        SetExcitement(old);
    }
}

void CrowdAudio::OnWin() {
    bool b1 = false;
    if (mShouldPlayVenueOutro) {
        b1 = PlayLoop(mVenueOutro, false);
        mShouldPlayVenueOutro = false;
    }
    if (!b1 && mLevel < kExcitementOkay) {
        SetExcitement(kExcitementOkay);
    }
    mState = 3;
    mWon = true;
}

void CrowdAudio::OnEnd() { mState = 4; }

BEGIN_HANDLERS(CrowdAudio)
    HANDLE_ACTION(play_intro, OnIntro())
    HANDLE_ACTION(play_sequence, PlaySequence(_msg->Str(2)))
    HANDLE_ACTION(stop_sequence, StopSequence(_msg->Str(2)))
    HANDLE_ACTION(play_crowd_closeup, PlayCloseupAudio())
    HANDLE_ACTION(stop_crowd_closeup, StopCloseupAudio())
    HANDLE_ACTION(excitement, SetExcitement((ExcitementLevel)_msg->Int(2)))
    HANDLE_ACTION(set_paused, SetPaused(_msg->Int(2)))
    HANDLE_ACTION(music_start, OnMusicStart())
    HANDLE_ACTION(on_win, OnWin())
    HANDLE_ACTION(on_lose, OnLose())
    HANDLE_ACTION(on_end, OnEnd())
    HANDLE_ACTION(game_outro, OnOutro())
    HANDLE_ACTION(crowd_noclap, mClapAllowed = false)
    HANDLE_ACTION(crowd_clap, mClapAllowed = true)
    HANDLE_ACTION(world_pause, SetPaused(true))
    HANDLE_ACTION(world_unpause, SetPaused(false))
    HANDLE_EXPR(done, mState == 4 || mState == 3)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CrowdAudio)
    SYNC_PROP_SET(enabled, mEnabled, SetEnabled(_val.Int()))
    SYNC_PROP(crowd_reacts, mCrowdReacts)
    SYNC_PROP(should_play_venue_intro, mShouldPlayVenueIntro)
    SYNC_PROP(should_play_venue_outro, mShouldPlayVenueOutro)
END_PROPSYNCS

// RB3 retail linker interleaved InlineHelp.cpp COMDATs into this TU's .text span
// (InlineHelp::Enter + ActionElement ctors/setters). Compile here so objdiff
// pairs them (sw scatter-scan). gRev/gAltRev collide with this TU's INIT_REVS.
#define gRev gRev_InlineHelp
#define gAltRev gAltRev_InlineHelp
#include "ui/InlineHelp.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/CrowdAudio <- hamobj/HamMove.cpp)
#define gRev gRev_HamMove
#define gAltRev gAltRev_HamMove
#include "hamobj/HamMove.cpp"
#undef gRev
#undef gAltRev
