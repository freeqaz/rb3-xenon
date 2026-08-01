#include "char/CharLipSyncDriver.h"
#include "char/Char.h"
#include "char/CharFaceServo.h"
#include "char/CharLipSync.h"
#include "char/CharWeightable.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/Utl.h"
#include "math/Utl.h"
#include "os/Timer.h"
#include "rndobj/Poll.h"
#include "rndobj/Rnd.h"
#include "utl/Loader.h"
#include "world/CameraManager.h"
#include "world/Dir.h"
#include <cstring>

float Mod(float a, float b) {
    if (b == 0.0f)
        return 0.0f;
    float result = fmod(a, b);
    if (result < 0.0f)
        result += b;
    return result;
}

CharLipSyncDriver::CharLipSyncDriver()
    : mLipSync(this), mClips(this), mBlinkClip(this), mSongOwner(this), mSongOffset(0),
      mLoop(0), mMainPlayback(0), mBones(this), mTestClip(this), mTestWeight(1),
      mOverrideClip(this), mOverrideWeight(0), mOverrideOptions(this),
      mApplyOverrideAdditively(0), mAlternateDriver(this) {}

CharLipSyncDriver::~CharLipSyncDriver() { RELEASE(mMainPlayback); }

BEGIN_HANDLERS(CharLipSyncDriver)
    HANDLE_ACTION(resync, Sync())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharLipSyncDriver)
    SYNC_PROP(bones, mBones)
    SYNC_PROP_SET(clips, mClips.Ptr(), SetClips(_val.Obj<ObjectDir>()))
    SYNC_PROP_SET(lipsync, mLipSync.Ptr(), SetLipSync(_val.Obj<CharLipSync>()))
    SYNC_PROP(song_owner, mSongOwner)
    SYNC_PROP(loop, mLoop)
    SYNC_PROP(song_offset, mSongOffset)
    SYNC_PROP(test_clip, mTestClip)
    SYNC_PROP(test_weight, mTestWeight)
    SYNC_PROP(override_clip, mOverrideClip)
    SYNC_PROP(override_weight, mOverrideWeight)
    SYNC_PROP(override_options, mOverrideOptions)
    SYNC_PROP(apply_override_additively, mApplyOverrideAdditively)
    SYNC_PROP(alternate_driver, mAlternateDriver)
    SYNC_SUPERCLASS(CharWeightable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain does not include this superclass;
    // DC3's newer engine added it. Native-only.
    SYNC_SUPERCLASS(CharPollable)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharLipSyncDriver)
    SAVE_REVS(7, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(CharWeightable)
    bs << mBones;
    bs << mClips;
    bs << mLipSync;
    bs << mTestClip;
    bs << mTestWeight;
    bs << mOverrideClip;
    bs << mOverrideOptions;
    bs << mApplyOverrideAdditively;
    bs << mOverrideWeight;
    bs << mAlternateDriver;
END_SAVES

INIT_REVS(7, 0)

BEGIN_LOADS(CharLipSyncDriver) // register error
    LOAD_REVS(bs)
    ASSERT_REVS(7, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    d >> mBones;
    d >> mClips;
    if (d.rev < 1) {
        FilePath fp;
        d >> fp;
        MILO_NOTIFY("%s old version, won't load %s", PathName(this), (String &)fp);
        String str;
        d >> str;
    } else
        d >> mLipSync;
    if (d.rev > 1) {
        mTestClip.Load(bs, true, mClips);
        d >> mTestWeight;
    }
    if (d.rev > 2) {
        mOverrideClip.Load(bs, true, mClips);
        if (d.rev < 5) {
            int x;
            d >> x;
        }
        d >> mOverrideOptions;
    }
    if (d.rev > 3)
        d >> mApplyOverrideAdditively;
    if (d.rev > 5)
        d >> mOverrideWeight;
    if (d.rev > 6)
        d >> mAlternateDriver;
    Sync();
END_LOADS

BEGIN_COPYS(CharLipSyncDriver)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharLipSyncDriver)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mBones)
        COPY_MEMBER(mClips)
        COPY_MEMBER(mLipSync)
        COPY_MEMBER(mBlinkClip)
        COPY_MEMBER(mSongOffset)
        COPY_MEMBER(mLoop)
        COPY_MEMBER(mSongOwner)
        COPY_MEMBER(mTestClip)
        COPY_MEMBER(mTestWeight)
        COPY_MEMBER(mOverrideWeight)
        COPY_MEMBER(mOverrideClip)
        COPY_MEMBER(mOverrideOptions)
        COPY_MEMBER(mApplyOverrideAdditively)
        COPY_MEMBER(mAlternateDriver)
    END_COPYING_MEMBERS
END_COPYS

void CharLipSyncDriver::Enter() {
    RndPollable::Enter();
    mOverrideWeight = 0;
    if (mLipSync)
        Sync();
}

void CharLipSyncDriver::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mBones);
}

void CharLipSyncDriver::SetClips(ObjectDir *dir) {
    mClips = dir;
    Sync();
}

bool CharLipSyncDriver::SetLipSync(CharLipSync *sync) {
    // rb3-Wii oracle body (returns void there); bool kept for hamobj compat.
    if (sync != mLipSync) {
        mLipSync = sync;
        mLoop = false;
        mSongOffset = 0;
        Sync();
        return true;
    }
    return false;
}

// dc3-only override-blend API: retail RB3 has no blend state fields, so these
// are compat shims (instant apply) for the hamobj (DC3 game layer) callers.
void CharLipSyncDriver::ResetOverrideBlend() {
    mOverrideClip = nullptr;
    mOverrideWeight = 0;
}

void CharLipSyncDriver::BlendInOverrideClip(CharClip *clip, float weight, float) {
    mOverrideClip = clip;
    mOverrideWeight = weight;
}

void CharLipSyncDriver::BlendInOverrides(float) {}

void CharLipSyncDriver::BlendOutOverrides(float) { mOverrideWeight = 0; }

void CharLipSyncDriver::Sync() {
    if (mClips) {
        mBlinkClip = mClips->Find<CharClip>("Blink", false);
    } else {
        mBlinkClip = nullptr;
    }
    RELEASE(mMainPlayback);
    if (mLipSync && mClips) {
        mMainPlayback = new CharLipSync::PlayBack();
        mMainPlayback->Set(mLipSync, mClips);
        mMainPlayback->Reset();
    }
}

void CharLipSyncDriver::ClearLipSync() {
    RELEASE(mMainPlayback);
    mLipSync = nullptr;
}

void CharLipSyncDriver::Highlight() {
    if (gCharHighlightY == -1.0f) {
        CharDeferHighlight(this);
    } else {
        Hmx::Color white(1, 1, 1);
        Vector2 v2(5.0f, gCharHighlightY);
        float y = TheRnd.DrawString(MakeString("%s:", PathName(this)), v2, white, true).y;
        v2.y += y;
        if (mMainPlayback) {
            int frame = mMainPlayback->mFrame;
            TheRnd.DrawString(MakeString("frame %d", frame), v2, white, true);
            v2.y += y;
            std::vector<CharLipSync::PlayBack::Weight> &weights = mMainPlayback->mWeights;
            for (int i = 0; i < weights.size(); i++) {
                CharLipSync::PlayBack::Weight &curWeight = weights[i];
                float f14 = curWeight.mCurWeight;
                CharClip *clip = curWeight.mClip;
                if (f14 != 0 && clip) {
                    TheRnd.DrawString(
                        MakeString("%s %.4f", clip->Name(), f14), v2, white, true
                    );
                    v2.y += y;
                }
            }
        }
        gCharHighlightY = v2.y + y;
    }
}

void CharLipSyncDriver::Poll() {
    // rb3-Wii oracle body (retail RB3 Poll; dc3's override-blend machinery removed).
    START_AUTO_TIMER("lipsyncdriver");
    if (!mClips || !mBones)
        return;
    if (mTestClip && LOADMGR_EDITMODE) {
        if (!mTestClip->Relative() || mTestWeight < 0.0f)
            return;
        mBones.Ptr()->ScaleAdd(mTestClip, mTestWeight, mTestClip->StartBeat(), 0.0f);
        return;
    }
    if (mLipSync) {
        float weight = Weight();
        if (mOverrideClip && !mApplyOverrideAdditively) {
            if (mOverrideWeight > 0.0f) {
                weight *= 1.0f - mOverrideWeight;
            }
        }
        if (mMainPlayback && mOverrideClip && mOverrideWeight > 0.0f) {
            ScaleAddViseme(mOverrideClip, mOverrideWeight);
        }
        if (weight == 0.0)
            return;
        if (mMainPlayback) {
            float songTime = TheTaskMgr.Seconds(TaskMgr::kRealTime) + mSongOffset;
            if (mLoop) {
                songTime = Mod(songTime, mMainPlayback->mLipSync->Duration() - 0.001f);
            }
            if (mAlternateDriver)
                songTime = mAlternateDriver->TopClipFrame();
            mMainPlayback->Poll(songTime);
            CharLipSync::PlayBack *pb = mMainPlayback;
            for (unsigned int i = 0; i < pb->mWeights.size(); i++) {
                float curWeight = pb->mWeights[i].mCurWeight;
                if (curWeight != 0.0f) {
                    CharClip *clip = pb->mWeights[i].mClip;
                    if (clip != mBlinkClip) {
                        if (mSongOwner)
                            curWeight = 0.0f;
                        else
                            curWeight *= weight;
                    }
                    if (clip && curWeight != 0.0f) {
                        ScaleAddViseme(clip, curWeight);
                    }
                }
            }
        }
        if (mSongOwner && mSongOwner->mMainPlayback) {
            float songTime =
                TheTaskMgr.Seconds(TaskMgr::kRealTime) + mSongOwner->mSongOffset;
            if (mLoop) {
                songTime =
                    Mod(songTime, mSongOwner->mMainPlayback->mLipSync->Duration() - 0.001f);
            }
            mSongOwner->mMainPlayback->Poll(songTime);
            CharLipSync::PlayBack *pb = mSongOwner->mMainPlayback;
            for (unsigned int i = 0; i < pb->mWeights.size(); i++) {
                float curWeight = weight * pb->mWeights[i].mCurWeight;
                CharClip *clip = pb->mWeights[i].mClip;
                if (curWeight != 0.0f && clip && clip != mSongOwner->mBlinkClip) {
                    CharClip *remapped = mClips->Find<CharClip>(clip->Name(), true);
                    ScaleAddViseme(remapped, curWeight);
                }
            }
        }
    }
    {
        CharFaceServo *servo = dynamic_cast<CharFaceServo *>(mBones.Ptr());
        if (servo)
            servo->ApplyProceduralWeights();
    }
}

void CharLipSyncDriver::ScaleAddViseme(CharClip *clip, float f1) {
    float dVar2 = 0.0f;
    float length = 0.0f;
    if (clip->LengthSeconds() != 0.0) {
        float temp = clip->LengthSeconds();
        length = TheTaskMgr.Seconds(TaskMgr::kRealTime);
        dVar2 = fmod(length, temp);
    } else {
        dVar2 = 0.0f;
    }
    length = clip->FrameToBeat(clip->FramesPerSec() * dVar2);
    mBones.Ptr()->ScaleAdd(clip, 0.0, length, f1);
}
