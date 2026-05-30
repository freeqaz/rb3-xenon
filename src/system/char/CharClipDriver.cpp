#include "char/CharClipDriver.h"
#include "char/CharBones.h"
#include "char/CharClip.h"
#include "macros.h"
#include "math/Easing.h"
#include "math/Rand.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Anim.h"
#include "utl/Symbol.h"
#include <cmath>

CharClipDriver::CharClipDriver(
    Hmx::Object *owner,
    CharClip *clip,
    int mask,
    float blendwidth,
    CharClipDriver *next,
    float f2,
    float f3,
    bool multclips
)
    : mPlayFlags(clip->PlayFlags()), mBlendWidth(blendwidth), mTimeScale(1.0f), mDBeat(0),
      mAdvanceBeat(0), mClip(owner, clip), mNext(next), mNextEvent(-1),
      mPlayMultipleClips(multclips) {
    if (mask & 0xF0U)
        CharClip::SetDefaultLoopFlag(mPlayFlags, mask & 0xF0U);
    if (mask & 0xFU)
        CharClip::SetDefaultBlendFlag(mPlayFlags, mask & 0xFU);
    if (mask & 0xF600U)
        CharClip::SetDefaultBeatAlignModeFlag(mPlayFlags, mask & 0xF600U);
    while (mNext && mNext->mBlendFrac == 0) {
        mNext = mNext->Exit(false);
    }
    if (f2 != kHugeFloat) {
        mBeat = f2;
        mRampIn = f3;
        mBlendFrac = 0;
    } else {
        if (mNext && (mPlayFlags & 0xF) == 2) {
            mNext = mNext->Exit(true);
        }
        if (mNext) {
            const CharGraphNode *gNode =
                mNext->mClip->FindNode(mClip, mNext->mBeat, mPlayFlags, mBlendWidth);
            if (gNode) {
                mBeat = gNode->nextBeat;
                float cur = gNode->curBeat;
                float nextBeat = mNext->mBeat;
                mRampIn = cur - nextBeat;
                mBlendFrac = 0;
                goto next;
            }
        }
        mBeat = clip->StartBeat();
        mRampIn = 0;
        mBlendFrac = 1;
    }
next:
    if (mPlayMultipleClips || (mPlayFlags & 0xF) == 8) {
        mBlendFrac = 1e-06f;
    }
    if (mBlendFrac == 1) {
        if (mClip->Range() > 0) {
            float f7 = RandomFloat(0, mClip->Range());
            float f10 = mClip->EndBeat() + mClip->StartBeat();
            f10 /= 2.0f;
            float f8 = mClip->StartBeat();
            mBeat = ModRange(f8, f10, mBeat + f7);
        }
    }
    mWeight = 0;
}

CharClipDriver::CharClipDriver(Hmx::Object *o, const CharClipDriver &driver)
    : mClip(o, driver.mClip) {
    mPlayFlags = driver.mPlayFlags;
    mBlendWidth = driver.mBlendWidth;
    mTimeScale = driver.mTimeScale;
    mRampIn = driver.mRampIn;
    mBeat = driver.mBeat;
    mDBeat = driver.mDBeat;
    mBlendFrac = driver.mBlendFrac;
    mAdvanceBeat = driver.mAdvanceBeat;
    mWeight = driver.mWeight;
    mNextEvent = driver.mNextEvent;
    mEventData = driver.mEventData;
    if (driver.mNext)
        mNext = new CharClipDriver(o, *driver.mNext);
    else
        mNext = nullptr;
}

void CharClipDriver::ScaleAdd(CharBones &bones, float f) {
    if (f != 0.0f) {
        mWeight = f * EaseSigmoid(mBlendFrac, 0.0f, 0.0f);
        bones.ScaleAdd(mClip, mWeight, mBeat, mDBeat);
        if (mPlayMultipleClips) {
            if (mNext)
                mNext->ScaleAdd(bones, f);
        } else {
            if (mNext)
                mNext->ScaleAdd(bones, f - mWeight);
        }
    }
}

void CharClipDriver::RotateTo(CharBones &bones, float f) {
    if (f != 0) {
        mWeight = f * EaseSigmoid(mBlendFrac, 0, 0);
        mClip->RotateTo(bones, mWeight, mBeat);
        if (mNext)
            mNext->RotateTo(bones, f - mWeight);
    }
}

int CharClipDriver::NumBeatEvents() const {
    if (mClip)
        return mClip->NumBeatEvents();
    return 0;
}

void CharClipDriver::DeleteStack() {
    if (mNext)
        mNext->DeleteStack();
    delete this;
}

CharClipDriver *CharClipDriver::Exit(bool b) {
    static Symbol exit("exit");
    if (b && mNext) {
        mNext = mNext->Exit(b);
    }
    CharClipDriver *ret = mNext;
    ExecuteEvent(exit);
    RndAnimatable *syncAnim = mClip->SyncAnim();
    if (syncAnim)
        syncAnim->EndAnim();
    delete this;
    return ret;
}

void CharClipDriver::ExecuteEvent(Symbol sym) {
    if (sym.Null())
        return;
    static Symbol clip_event("clip_event");
    Hmx::Object *owner = mClip.RefOwner();
    Hmx::Object *exportTarget = owner->Dir();
    static Message msg(clip_event, DataNode(0), DataNode(0), DataNode(0));
    msg[0] = DataNode(sym);
    msg[1] = DataNode(mClip.Ptr());
    exportTarget->Export(msg, true);
}

void CharClipDriver::SetBeatOffset(float offset, TaskUnits units, Symbol sym) {
    if (offset == 0.0f || !mClip)
        return;
    mBeat = mClip->StartBeat();
    if (!sym.Null()) {
        unsigned int i = 0;
        for (; i < mClip->mBeatEvents.size(); i++) {
            if (mClip->mBeatEvents[i].event == sym) {
                mBeat = mClip->mBeatEvents[i].beat;
                break;
            }
        }
        if (i == mClip->mBeatEvents.size()) {
            MILO_NOTIFY("%s could not find event %s", PathName(mClip), sym);
        }
    }
    if (units != kTaskBeats) {
        offset = mClip->DeltaSecondsToDeltaBeat(offset, mBeat);
    }
    mBeat += offset;
}
CharClipDriver *CharClipDriver::DeleteRef(ObjRef *ref, bool &b) {
    if (RefIs(ref, mClip)) {
        b = true;
        return Exit(false);
    } else if (mNext) {
        mNext = mNext->DeleteRef(ref, b);
    }
    return this;
}

float CharClipDriver::AlignToBeat(float oldBeat) {
    float align = (float)((mPlayFlags >> 12) & 0xF);
    if (align != 0.0f && mTimeScale == 1.0f && (mPlayFlags & 0xF0) != 0x20) {
        float delta = Mod(oldBeat - mBeat, align);
        if (delta > align * 0.5f) {
            delta -= align;
            if (delta + mBeat < mClip->StartBeat()) {
                delta += align;
            }
        }
        return delta;
    }
    return 0.0f;
}

void CharClipDriver::PlayEvents(float oldBeat) {
    if (mNextEvent == -1) {
        RndAnimatable *syncAnim = mClip->SyncAnim();
        if (syncAnim) {
            syncAnim->StartAnim();
        }
        static Symbol enter("enter");
        ExecuteEvent(enter);
        mNextEvent = 0;
    }
    while ((unsigned int)mNextEvent < mClip->mBeatEvents.size()) {
        CharClip::BeatEvent &ev = mClip->mBeatEvents[mNextEvent];
        if (ev.beat > mBeat)
            return;
        ExecuteEvent(ev.event);
        mNextEvent++;
    }
}

CharClipDriver *CharClipDriver::PreEvaluate(float beat, float deltaBeat, float deltaSeconds) {
    MILO_ASSERT(mBlendFrac >= 0, 0xab);
    if (mBlendWidth < 0.0f) {
        MILO_NOTIFY("CharClipDriver: blend width < 0 with clip %s", (char *)mClip->Name());
        mBlendWidth = 0.0f;
    }
    if (mNext) {
        mNext = mNext->PreEvaluate(beat, deltaBeat, deltaSeconds);
    }
    int flags = mPlayFlags;
    bool useRealTime = flags & CharClip::kPlayRealTime;
    bool useUserTime = flags & CharClip::kPlayUserTime;

    float advance;
    if (!mPlayMultipleClips) {
        if (mNext) {
        advance = mNext->mAdvanceBeat;
    } else {
        if (useRealTime) {
            advance = deltaSeconds;
        } else {
            advance = deltaBeat;
        }
    }
    } else {
        if (useRealTime) {
            advance = deltaSeconds;
        } else {
            advance = deltaBeat;
        }
    }

    if (0.0f < mRampIn || 0.0f < advance) {
        mRampIn -= advance;
    }

    if (!(mRampIn < 0.0f)) {
        mDBeat = 0.0f;
        mAdvanceBeat = 0.0f;
    } else {
        float oldBeat = mBeat;
        if (flags & 0x80) {
            mDBeat = 0.0f;
            mPlayFlags = flags & ~0x80;
        } else if (!useUserTime) {
            float db;
            if (useRealTime) {
                db = mClip->DeltaSecondsToDeltaBeat(deltaSeconds, oldBeat);
            } else {
                db = deltaBeat;
            }
            mDBeat = mTimeScale * db;
        }
        mBeat = mDBeat + mBeat;
        float align = AlignToBeat(beat);
        mBeat += align;
        mAdvanceBeat = mDBeat + align;
        PlayEvents(oldBeat);
        RndAnimatable *syncAnim = mClip->SyncAnim();
        if (syncAnim) {
            syncAnim->SetFrame(mClip->BeatToFrame(mBeat), 1.0f);
        }
        if (mBlendFrac < 1.0f) {
            if (!(mBlendWidth <= 0.0f)) {
                float inc;
                if (useUserTime) {
                    inc = deltaBeat;
                } else {
                    inc = mDBeat;
                }
                inc = inc / mBlendWidth;
                if (0.0f < inc) {
                    mBlendFrac += inc;
                }
            } else {
                mBlendFrac = 1.0f;
            }
            float clamped;
            if ((mBlendFrac - 1.0f >= 0.0f)) {
                clamped = 1.0f;
            } else {
                clamped = mBlendFrac;
            }
            mBlendFrac = clamped;
        }
    }

    if (!mPlayMultipleClips) {
        if (mNext) {
            if (mBlendFrac == 1.0f) {
                mNext = mNext->Exit(true);
            }
        }
    } else {
        if (mBeat > mClip->EndBeat()) {
            return Exit(false);
        }
    }
    return this;
}

float CharClipDriver::Evaluate(float beat, float deltaBeat, float deltaSeconds) {
    float nextResult = 0.0f;
    if (mNext) {
        nextResult = mNext->Evaluate(beat, deltaBeat, deltaSeconds);
    }
    if ((mPlayFlags & 0xF0) == CharClip::kPlayLoop) {
        float curBeat = mBeat;
        float endBeat = mClip->EndBeat();
        if (curBeat > endBeat) {
            float lengthBeats = mClip->LengthBeats();
            if (!(lengthBeats <= 0.0f)) {
                float fmodArg = endBeat - curBeat;
                float startBeat = mClip->StartBeat();
                float dist = std::fmod(fmodArg, lengthBeats);
                mBeat = dist + startBeat;
            } else {
                mBeat = mClip->StartBeat();
            }
            float align = AlignToBeat(beat);
            mBeat += align;
            mNextEvent = 0;
        } else if (curBeat < mClip->StartBeat()) {
            float startBeat = mClip->StartBeat();
            float lengthBeats = mClip->LengthBeats();
            float newBeat;
            if (!(lengthBeats <= 0.0f)) {
                float dist = std::fmod(startBeat - curBeat, lengthBeats);
                newBeat = endBeat - dist;
            } else {
                newBeat = startBeat;
            }
            mBeat = newBeat;
            float align = AlignToBeat(beat);
            mBeat += align;
            mNextEvent = mClip->NumBeatEvents();
        }
    }
    float sigmoid = EaseSigmoid(mBlendFrac, 0.0f, 0.0f);
    return (1.0f - sigmoid) * nextResult + sigmoid;
}
