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
      mLoop(0), mMainPlayback(0), mIsOverrideActive(0), mMainBlendAlpha(1), mOverridePlayback(0), mBones(this), mTestClip(this),
      mTestWeight(1), unkc4(0), mBlendingIn(0), mBlendingOut(0), mOverrideBlendTarget(0), unkd4(0),
      mOverrideClip(this), mOverrideWeight(0), mOverrideOptions(this),
      mApplyOverrideAdditively(0), mOverrideBlendClip(this), mOverrideBlendWeight(0), mOverrideBlendDuration(0), mOverrideBlendStartTime(0),
      mOverrideBlendActive(0), mAlternateDriver(this) {}

CharLipSyncDriver::~CharLipSyncDriver() {
    RELEASE(mMainPlayback);
    RELEASE(mOverridePlayback);
}

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
    SYNC_SUPERCLASS(CharPollable)
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
    if (mIsOverrideActive) {
        MILO_LOG(
            "CharLipSyncDriver::SetLipSync() - previous VO Lipsync was fading out.  Deleting now - Name:%s\n",
            SafeName(mLipSync)
        );
        RELEASE(mMainPlayback);
        mLipSync = nullptr;
        mIsOverrideActive = false;
        mMainBlendAlpha = 1;
    }

    if (sync) {
        if (!streq(sync->Name(), "player1_cam.lipsync")
            && !streq(sync->Name(), "player2_cam.lipsync")
            && !streq(sync->Name(), "dancer_face.lipsync")) {
            RELEASE(mOverridePlayback);
            mOverridePlayback = new CharLipSync::PlayBack();
            mOverridePlayback->Set(sync, mClips);
            mOverridePlayback->Reset();
            return true;
        } else if (sync != mLipSync) {
            mLipSync = sync;
            mLoop = false;
            mSongOffset = 0;
            Sync();
            return true;
        }
    }
    return false;
}

void CharLipSyncDriver::ApplyBlinks() {
    CharFaceServo *servo = dynamic_cast<CharFaceServo *>(mBones.Ptr());
    if (servo) {
        servo->ApplyProceduralWeights();
    }
}

void CharLipSyncDriver::ResetOverrideBlend() {
    mOverrideBlendClip = nullptr;
    mOverrideBlendWeight = 0;
}

void CharLipSyncDriver::BlendInOverrideClip(CharClip *clip, float f1, float f2) {
    mOverrideBlendClip = clip;
    mOverrideBlendWeight = f1;
    mOverrideBlendDuration = f2;
    mOverrideBlendActive = true;
}

void CharLipSyncDriver::Sync() {
    if (mClips) {
        mBlinkClip = mClips->Find<CharClip>("Blink", false);
    } else {
        mBlinkClip = nullptr;
    }
    RELEASE(mMainPlayback);
    if (mOverridePlayback && mClips) {
        mOverridePlayback->SetClips(mClips);
    }
    if (mLipSync && mClips) {
        mMainPlayback = new CharLipSync::PlayBack();
        mMainPlayback->Set(mLipSync, mClips);
        mMainPlayback->Reset();
        mIsOverrideActive = false;
        mMainBlendAlpha = 1;
    }
}

void CharLipSyncDriver::ClearLipSync() {
    RELEASE(mOverridePlayback);
    RELEASE(mMainPlayback);
    mLipSync = nullptr;
    mIsOverrideActive = false;
    mMainBlendAlpha = 1;
}

void CharLipSyncDriver::BlendInOverrides(float f) {
    mOverrideBlendTarget = f;
    mBlendingIn = true;
    mBlendingOut = false;
    mIsBlending = true;
}

void CharLipSyncDriver::BlendOutOverrides(float f) {
    mOverrideBlendTarget = f;
    mBlendingOut = true;
    mBlendingIn = false;
    mIsBlending = true;
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

void CharLipSyncDriver::UpdatePlayback(CharLipSync::PlayBack *pb, float weight, float songTime) {
    if (!pb)
        return;
    songTime += TheTaskMgr.Seconds(TaskMgr::kRealTime);
    if (mLoop) {
        songTime = Mod(songTime, pb->mLipSync->Duration() - 0.001f);
    }
    if (mAlternateDriver) {
        songTime = mAlternateDriver->TopClipFrame();
    }
    pb->Poll(songTime);
    for (unsigned int i = 0; i < pb->mWeights.size(); i++) {
        CharLipSync::PlayBack::Weight &w = pb->mWeights[i];
        float curWeight = w.mCurWeight;
        if (curWeight != 0.0f) {
            CharClip *clip = w.mClip;
            if (clip != mBlinkClip) {
                if (mSongOwner) {
                    curWeight = 0.0f;
                } else {
                    curWeight = curWeight * weight;
                }
            }
            if (clip && curWeight != 0.0f) {
                if (curWeight < 0.0f) {
                    MILO_FAIL("weight = %f", curWeight);
                }
                curWeight = Max(curWeight, 0.0f);
                ScaleAddViseme(clip, curWeight);
            }
        }
    }
}

void CharLipSyncDriver::Poll() {
    START_AUTO_TIMER("lipsyncdriver");

    if (mClips) {
        if (mBones) {
            if (mTestClip) {
                if (TheLoadMgr.EditMode()) {
                    CharClip *relative = mTestClip->Relative();
                    if (relative && mTestWeight >= 0.0f) {
                        mBones.Ptr()->ScaleAdd(
                            mTestClip, mTestWeight, mTestClip->StartBeat(), 0.0f
                        );
                    }
                    return;
                }
            }
        } else
            return;
    } else
        return;

    float timeMs = TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f;

    if (mIsBlending) {
        unkd4 = timeMs;
        mIsBlending = false;
    }

    if (mBlendingIn) {
        if (timeMs > mOverrideBlendTarget + unkd4) {
            unkc4 = 1.0f;
            mBlendingIn = false;
        } else {
            float pct = Clamp(0.0f, 1.0f, (timeMs - unkd4) / mOverrideBlendTarget);
            if (pct > unkc4) unkc4 = pct;
        }
    } else if (mBlendingOut) {
        if (timeMs > mOverrideBlendTarget + unkd4) {
            unkc4 = 0.0f;
            mBlendingOut = false;
        } else {
            float pct = Clamp(0.0f, 1.0f, (timeMs - unkd4) / mOverrideBlendTarget);
            float invPct = 1.0f - pct;
            if (invPct < unkc4) unkc4 = invPct;
        }
    }

    if (mOverrideBlendWeight > 0.0f) {
        if (mOverrideBlendActive) {
            mOverrideBlendStartTime = timeMs;
            mOverrideBlendActive = false;
        }
        if (timeMs > mOverrideBlendDuration + mOverrideBlendStartTime) {
            mOverrideClip.CopyRef(mOverrideBlendClip);
            mOverrideWeight = mOverrideBlendWeight;
            mOverrideBlendWeight = 0.0f;
        } else {
            if (mMainPlayback) {
                float pct = Clamp(0.0f, 1.0f, (timeMs - mOverrideBlendStartTime) / mOverrideBlendDuration);

                if (mOverrideClip) {
                    if (mOverrideWeight > 0.0f) {
                        float w = (1.0f - pct) * mOverrideWeight * unkc4;
                        if (w < 0.0f) {
                            if (unkc4 < 0.0f)
                                MILO_FAIL("mOverallOverrideWeight = %f", unkc4);
                            if (mOverrideWeight < 0.0f)
                                MILO_FAIL("mOverrideWeight = %f", mOverrideWeight);
                            if (pct > 1.0f)
                                MILO_FAIL("pct = %f", pct);
                            w = 0.0f;
                        }
                        ScaleAddViseme(mOverrideClip, w);
                    }
                }

                if (mOverrideBlendClip && 0.0f < mOverrideBlendWeight) {
                    float w = mOverrideBlendWeight * unkc4 * pct;
                    if (w < 0.0f) {
                        if (unkc4 < 0.0f)
                            MILO_FAIL("mOverallOverrideWeight = %f", unkc4);
                        if (mOverrideBlendWeight < 0.0f)
                            MILO_FAIL("mOverrideWeight = %f", mOverrideBlendWeight);
                        if (pct < 0.0f)
                            MILO_FAIL("pct = %f", pct);
                        w = 0.0f;
                    }
                    ScaleAddViseme(mOverrideBlendClip, w);
                }

                if (1.0f <= unkc4) {
                    ApplyBlinks();
                    return;
                }
            }
        }
    }

    if (mOverrideBlendWeight <= 0.0f) {
        if (mOverrideClip) {
            float w = mOverrideWeight * unkc4;
            if (0.0f < w) {
                ScaleAddViseme(mOverrideClip, w);
                if (1.0f <= unkc4) {
                    ApplyBlinks();
                    return;
                }
            }
        }
    }

    float remainingWeight = 1.0f - unkc4;
    if (0.0f >= remainingWeight)
        return;

    UpdatePlayback(mMainPlayback, mMainBlendAlpha * remainingWeight, mSongOffset);

    if (!mIsOverrideActive) {
        if (mMainPlayback) {
            if (mMainPlayback->mLipSync) {
                float duration = mMainPlayback->mLipSync->Duration();
                float songTime = TheTaskMgr.Seconds(TaskMgr::kRealTime) + mSongOffset;
                if (songTime >= duration) {
                    char *lsName = mMainPlayback->mLipSync
                        ? (char *)mMainPlayback->mLipSync->Name()
                        : (char *)"";
                    MILO_LOG(
                        "CharLipSyncDriver::Poll() - Triggering VO Lip Sync FadeOut - Name:%s\n",
                        lsName
                    );
                    mIsOverrideActive = true;
                }
            }
        }
    }

    if (mIsOverrideActive) {
        if (mMainBlendAlpha < 0.001f) {
            char *lsName2 = mMainPlayback->mLipSync
                ? (char *)mMainPlayback->mLipSync->Name()
                : (char *)"";
            MILO_LOG(
                "CharLipSyncDriver::Poll() - Deleting VO Lip Sync track because it finished and faded out - Name:%s\n",
                lsName2
            );
            RELEASE(mMainPlayback);
            mLipSync = nullptr;
            mMainBlendAlpha = 1.0f;
            mIsOverrideActive = false;
        } else {
            mMainBlendAlpha *= 0.99f;
        }
    }

    CamShot *cam = nullptr;
    if (TheWorld) {
        CameraManager *camMgr = TheWorld->GetCameraManager();
        if (camMgr) {
            cam = camMgr->CurrentShot();
            if (!cam) {
                cam = camMgr->MiloCamera();
            }
        }
    }

    unsigned char skipOverride;
    if (!mMainPlayback || !mMainPlayback->mLipSync || !cam) {
        skipOverride = 0;
    } else {
        const char *name = cam->Name();
        if (name && strncmp(name, "battle_", 7) == 0) {
            skipOverride = 1;
        } else {
            skipOverride = 0;
        }
    }
    if (!skipOverride) {
        UpdatePlayback(mOverridePlayback, remainingWeight, 0.0f);
    }

    if (mSongOwner) {
        if (mSongOwner->mMainPlayback) {
            float songTime = TheTaskMgr.Seconds(TaskMgr::kRealTime) + mSongOwner->mSongOffset;
            if (mLoop) {
                songTime = Mod(songTime, mSongOwner->mMainPlayback->mLipSync->Duration() - 0.001f);
            }
            mSongOwner->mMainPlayback->Poll(songTime);
            CharLipSync::PlayBack *pb = mSongOwner->mMainPlayback;
            for (unsigned int i = 0; i < pb->mWeights.size(); i++) {
                CharClip *clip = pb->mWeights[i].mClip;
                float curWeight = pb->mWeights[i].mCurWeight * remainingWeight;
                if (curWeight != 0.0f && clip && clip != mSongOwner->mBlinkClip) {
                    CharClip *remapped = mClips->Find<CharClip>(clip->Name(), true);
                    ScaleAddViseme(remapped, curWeight);
                }
            }
        }
    }

    ApplyBlinks();
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
