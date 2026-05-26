#include "hamobj/ClipPlayer.h"
#include "HamRegulate.h"
#include "HamGameData.h"
#include "MoveMgr.h"
#include "char/CharClip.h"
#include "flow/PropertyEventProvider.h"
#include "hamobj/Difficulty.h"
#include "hamobj/HamCharacter.h"
#include "hamobj/HamDirector.h"
#include "hamobj/HamDriver.h"
#include "hamobj/SongUtl.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "rndobj/PropAnim.h"
#include "rndobj/PropKeys.h"
#include "utl/Loader.h"

const char *ClipPlayer::sRestStepNames[4] = {
    "rest_step_left", "rest_step_right", "rest_step_fwd", "rest_step_back"
};

void Annotate(DataArray *a, float f, const char *cc) {
    a->Insert(a->Size(), DataArrayPtr(BeatToFrame(f), cc));
}

float ClipPlayer::ClipLength(CharClip *clip) {
    return floorf(clip->EndBeat()) - ceilf(clip->StartBeat());
}

bool ClipPlayer::Init(RndPropAnim *anim) {
    mClipDir = TheHamDirector->ClipDir();
    if (anim) {
        PropKeys *clipKeys = anim->GetKeys(TheHamDirector, DataArrayPtr(Symbol("clip")));
        if (clipKeys) {
            mClipKeys = clipKeys->AsSymbolKeys();
        }
        PropKeys *clipCrossoverKeys =
            anim->GetKeys(TheHamDirector, DataArrayPtr(Symbol("clip_crossover")));
        if (clipCrossoverKeys) {
            mClipCrossoverKeys = clipCrossoverKeys->AsSymbolKeys();
        }
        PropKeys *masterKeys = TheHamDirector->GetMasterKeys("clip");
        if (masterKeys) {
            mMasterClipKeys = masterKeys->AsSymbolKeys();
        }
        if (mClipKeys && mMasterClipKeys && mClipDir) {
            Key<Symbol> *k1;
            Key<Symbol> *k2;
            if (TheHamDirector->GetPracticeFrames(k2, k1)) {
                mPracticeStart = Round(FrameToBeat(k1->frame));
                mPracticeEnd = Round(FrameToBeat(k2->frame)) - 1.0f;
                String str(k1->value);
                str.ReplaceAll('*', '\0');
                auto _tmp4 = mClipDir->Find<CharClip>(MakeString("%s_in", str.c_str()), false);
                mInClip =
                    _tmp4;
                str = k2->value;
                str.ReplaceAll('*', '\0');
                mOutClip =
                    mClipDir->Find<CharClip>(MakeString("%s_out", str.c_str()), false);
                mRestClip = mClipDir->Find<CharClip>("rest", false);
                for (int i = 0; i < 4; i++) {
                    mRestStepClips[i] =
                        mClipDir->Find<CharClip>(sRestStepNames[i], false);
                }
            }
            return true;
        }
    }
    return false;
}

bool ClipPlayer::Init(Difficulty d) {
    return Init(TheHamDirector->GetPropAnim(d, "song.anim", false));
}

bool ClipPlayer::Init(int x) {
    mPlayerIndex = x;
    return Init(TheHamDirector->SongAnim(x));
}

bool ClipPlayer::CanUseRestStep() {
    // In non-edit mode (or when transitions are enabled), check if the out clip
    // is compatible with rest steps. Rest steps require a 3-beat clip with no flag 0x4.
    if (!TheLoadMgr.EditMode() || !TheHamDirector->NoTransitions()) {
        CharClip *clip = mOutClip;
        if (clip && (ClipLength(clip) != 3 || clip->Flags() & 4)) {
            return false;
        }
    }
    return true;
}

DataNode ClipPlayer::AnnotatePractice() {
    bool cont = mPracticeEnd != kHugeFloat;
    if (!cont) {
        return 0;
    }
    DataArray *arr = new DataArray(0);
    float f30 = 1.0f;
    if (!TheLoadMgr.EditMode() || !TheHamDirector->NoTransitions()) {
        CharClip *inClip = mInClip;
        if (inClip) {
            const char *name = inClip->Name();
            float f31 = mPracticeStart + f30;
            float clipLen = ClipLength(inClip);
            Annotate(arr, f31 - clipLen, name);
            Annotate(arr, mPracticeStart + f30, "");
        }
    }
    float f31 = mPracticeEnd;
    if (!TheLoadMgr.EditMode() || !TheHamDirector->NoTransitions()) {
        CharClip *outClip = mOutClip;
        if (outClip) {
            Annotate(arr, f31 - f30, outClip->Name());
            f31 += ClipLength(outClip) - 2.0f;
        }
    }
    if (CanUseRestStep()) {
        Annotate(arr, f31, "rest_step");
        f31 = mPracticeEnd + 4.0f;
    }
    Annotate(arr, f31, "rest");
    Annotate(arr, mPracticeStart - (float)(long long)(TheHamDirector->StartLoopMargin() << 2), "loop");
    Annotate(arr, (float)(long long)(TheHamDirector->EndLoopMargin() << 2) + mPracticeEnd + f30, "loop");
    DataNode node(arr, kDataArray);
    arr->Release();
    return node;
}

DataNode ClipPlayer::AnnotateClip(float frame) {
    int idx = mClipKeys->KeyLessEq(frame);
    if (idx < 0) goto fail;
    {
        Key<Symbol> &key = mClipKeys->at(idx);
        DataArray *arr;
        const char *name;
        float annotBeat;

        if (mClipKeys == mMasterClipKeys) {
            const char *nextName = "";
            auto _tmp0 = mClipKeys->size();
            if ((unsigned int)(idx + 1) < _tmp0) {
                nextName = mClipKeys->at(idx + 1).value.Str();
            }
            name = key.value.Str();
            float clipBeat = FrameToBeat(key.frame);
            float outStart, outEnd, outNextStart;
            if (!GetClipRange(name, nextName, clipBeat, outStart, outEnd, outNextStart))
                goto fail;
            arr = new DataArray(0);
            Annotate(arr, outStart, "start");
            Annotate(arr, outEnd, "end");
            if (outNextStart != kHugeFloat) {
                name = "blend";
                annotBeat = outNextStart;
                goto do_annotate;
            }
        } else {
            if ((unsigned int)(idx + 1) >= mClipKeys->size())
                goto fail;
            Key<Symbol> &nextKey = mClipKeys->at(idx + 1);
            CharClip *transClip = GetTransitionBefore(&nextKey);
            if (!transClip) goto fail;
            arr = new DataArray(0);
            float transLen = ClipLength(transClip);
            name = transClip->Name();
            annotBeat = FrameToBeat(nextKey.frame) - transLen + 1.0f;
        do_annotate:
            Annotate(arr, annotBeat, name);
        }
        DataNode node(arr, kDataArray);
        arr->Release();
        return node;
    }
fail:
    return 0;
}

void ClipPlayer::PlayAnims(HamCharacter *c, float f1, float f2, int x) {
    mTargetClip = x;
    mBeat = FrameToBeat(f1);
    mPrevBeat = FrameToBeat(f2);
    mDriver = c->SongDriver();
    mClipCount = 0;
    mDriver->Clear();
    HamRegulate *reg = c->Regulator();
    PlayNormal(-kHugeFloat, nullptr, "");
    reg->RegulateWay(c->GetWaypoint(), 8);
}

namespace {
    float ClipStart(CharClip *clip, float beat, float &outClipBeat, float &outEndBeat) {
        float frac = fmodf(beat, 1.0f);
        if (frac != 0.0f) {
            float ceiling = ceilf(beat);
            if (ceiling - beat < 0.0001f) {
                beat = ceilf(beat);
            }
        }
        unsigned int loopCount = (clip->PlayFlags() >> 12) & 0xF;
        float loopOffset = 0.0f;
        if ((float)loopCount != 0.0f) {
            loopOffset = Mod(beat - clip->StartBeat(), (float)loopCount);
        }
        outClipBeat = beat - loopOffset;
        outEndBeat = clip->LengthBeats() + (beat - loopOffset);
        return clip->StartBeat() + loopOffset;
    }
}

void ClipPlayer::PlayClip(CharClip *clip, float f1, float f2, HamDriver::LayerArray *arr) {
    if (clip) {
        float f50, f4c;
        ClipStart(clip, f1, f50, f4c);
        mClipCount++;
        if (!TheLoadMgr.EditMode() || (mTargetClip <= 0 || mClipCount == mTargetClip)) {
            HamDriver::LayerClip *layerClip = mDriver->NewLayerClip();
            layerClip->mClip = clip;
            layerClip->mClipBeat = f50 - mBeatOffset;
            layerClip->mBeat = f2 - mBeatOffset;
            arr->mLayers.push_front(layerClip);
            if (TheLoadMgr.EditMode() && mTargetClip > 0) {
                layerClip->mBeat = -kHugeFloat;
            }
        }
    }
}

bool ClipPlayer::PushExpertClip(int i1, HamDriver::LayerArray *arr) {
    if (i1 < 0)
        return false;
    else {
        Key<Symbol> &curKey = mClipKeys->at(i1);
        float beat = FrameToBeat(curKey.frame);
        bool b2 = false;
        if (mBeat < beat + 1.0f) {
            b2 = PushExpertClip(i1 - 1, arr);
        }
        float f6 = b2 ? beat : -kHugeFloat;
        PlayClip(mClipDir->Find<CharClip>(curKey.value.Str(), false), beat, f6, arr);
        return true;
    }
}

CharClip *ClipPlayer::GetTransitionBefore(Key<Symbol> *key) {
    if (key) {
        if (key != &mClipKeys->at(0)
            && (!TheLoadMgr.EditMode() || !TheHamDirector->NoTransitions())) {
            char name[256];
            strcpy(name, (key - 1)->value.Str());
            strcat(name, "_");
            strcat(name, key->value.Str());
            return mClipDir->Find<CharClip>(name, false);
        }
    }
    return nullptr;
}

template <class _T>
__declspec(noinline) auto _outline_EditMode(_T* _obj) -> decltype(_obj->EditMode()) {
    return _obj->EditMode();
}

CharClip *ClipPlayer::GetPrevRoutineTransition(int idx) {
    if (idx <= 0) return nullptr;

    int maxIdx = (int)mClipKeys->size() - 1;
    if (maxIdx < idx) idx = maxIdx;

    if (!TheLoadMgr.EditMode() || !TheHamDirector->NoTransitions()) {
        Key<Symbol> *curKey = &mClipKeys->at(idx);
        Key<Symbol> *prevKey = &mClipKeys->at(idx - 1);

        float beat = FrameToBeat(prevKey->frame);
        if (beat > 0.0f) {
            beat += 1.0f;
        }

        CharClip *c2 = nullptr;
        CharClip *c1 = nullptr;
#ifdef HX_NATIVE
        if (!GetRoutineCrossoverClips(beat, prevKey->value.Str(), &c1, &c2))
            return nullptr;
#else
        GetRoutineCrossoverClips(beat, prevKey->value.Str(), &c1, &c2);
#endif

        return GetRoutineTransition(c1->Name(), curKey);
    }
    return nullptr;
}

CharClip *ClipPlayer::GetRoutineTransition(const char *cc, Key<Symbol> *key) {
    if (key) {
        if (key != &mClipKeys->at(0)
            && (!TheLoadMgr.EditMode() || !TheHamDirector->NoTransitions())) {
            char name[256];
            strcpy(name, cc);
            strcat(name, "_");
            strcat(name, key->value.Str());
            return mClipDir->Find<CharClip>(name, false);
        }
    }
    return nullptr;
}

#ifdef HX_NATIVE
bool
#else
void
#endif
ClipPlayer::GetRoutineCrossoverClips(
    float f1, const char *cc, CharClip **c1, CharClip **c2
) {
#ifdef HX_NATIVE
    if (!mClipDir) return false;
    if (TheMoveMgr && TheMoveMgr->HasRoutine()) {
#else
    if (TheMoveMgr->HasRoutine()) {
#endif
        const std::pair<const MoveVariant *, const MoveVariant *> *moveVars =
            TheMoveMgr->GetRoutineMeasure(mPlayerIndex, Round(f1 / 4.0f));
        if (moveVars) {
            if (moveVars->first) {
                *c1 = mClipDir->Find<CharClip>(moveVars->first->Name().Str(), false);
            }
            if (moveVars->second) {
                *c2 = mClipDir->Find<CharClip>(moveVars->second->Name().Str(), false);
            }
        }
    }
    if (!*c1) {
        *c1 = *c2;
        if (!*c1) {
            *c1 = mClipDir->Find<CharClip>(cc, false);
#ifdef HX_NATIVE
            if (!*c1 && mMasterClipKeys && mMasterClipKeys->size() > 0) {
#else
            if (!*c1) {
#endif
                *c1 = mClipDir->Find<CharClip>(mMasterClipKeys->at(0).value.Str(), false);
            }
        }
    }
    if (!*c2) {
        *c2 = *c1;
    }
#ifdef HX_NATIVE
    return *c1 != nullptr;
#endif
}

bool ClipPlayer::GetClipRange(
    const char *clipName1, const char *clipName2, float beat,
    float &outStart, float &outEnd, float &outNextStart
) {
    CharClip *clip1 = mClipDir->Find<CharClip>(clipName1, false);
    if (clip1) {
        beat = ClipStart(clip1, beat, outStart, outEnd);
        outNextStart = kHugeFloat;

        CharClip *clip2 = mClipDir->Find<CharClip>(clipName2, false);
        if (clip2) {
            const CharGraphNode *node = clip1->FindLastNode(clip2, beat);
            if (node) {
                outNextStart = (node->curBeat - clip1->StartBeat()) + outEnd;
            }
        }
        return true;
    }
    return false;
}

void ClipPlayer::PushClip(int idx, HamDriver::LayerArray *arr) {
    if (idx < 0) return;
    if (mClipKeys->empty()) return;

    int maxIdx = (int)mClipKeys->size() - 1;
    if (maxIdx < idx) idx = maxIdx;

    Key<Symbol> &key = mClipKeys->at(idx);

    CharClip *transClip;
    float offset;
    float beat = FrameToBeat(key.frame);
    if (mBeat < beat + 1.0f) {
        transClip = GetTransitionBefore(&key);
    } else {
        transClip = nullptr;
    }

    if (transClip) {
        offset = ClipLength(transClip) - 2.0f;
    } else {
        offset = 0.0f;
    }

    if (mBeat < beat - offset) {
        PushClip(idx - 1, arr);
    }

    float blendBeat;
    if (transClip) {
        float transLen = ClipLength(transClip);
        float transStart = (beat - transLen) + 1.0f;
        PlayClip(transClip, transStart, transStart, arr);
        blendBeat = beat;
    } else {
        blendBeat = beat - 1.0f;
    }

    if (mBeat > blendBeat) {
        Key<Symbol> *practiceKey = TheHamDirector->GetMasterPracticeFrame(Symbol(key.value.Str()));
        if (practiceKey) {
            Keys<Symbol, Symbol> *savedKeys = mClipKeys;
            mClipKeys = mMasterClipKeys;

            float practBeat = FrameToBeat(practiceKey->frame);
            float beatDiff = practBeat - beat;

            mBeatOffset += beatDiff;
            mBeat += beatDiff;
            mPracticeStart += beatDiff;
            mPracticeEnd += beatDiff;

            PlayNormal(mBeatOffset + blendBeat, arr, key.value.Str());

            mBeat -= beatDiff;
            mPracticeStart -= beatDiff;
            mClipKeys = savedKeys;
            mPracticeEnd -= beatDiff;
            mBeatOffset -= beatDiff;
        } else {
            MILO_NOTIFY_ONCE(
                "%s: can't find %s in expert practice track",
                TheGameData->GetSong(), key.value.Str()
            );
        }
    }
}

bool ClipPlayer::PushRoutineBuilderClip(int idx, HamDriver::LayerArray *arr) {
    if (idx < 0 || mClipKeys->size() < 1) return false;

    Difficulty difficulty = TheGameData->Player(mPlayerIndex)->GetDifficulty();

    int maxIdx = (int)mClipKeys->size() - 1;
    int nextIdx = idx + 1;
    if (maxIdx < idx) idx = maxIdx;
    int maxIdx2 = (int)mClipKeys->size() - 1;
    if (maxIdx2 < nextIdx) nextIdx = maxIdx2;

    Key<Symbol> &curKey = mClipKeys->at(idx);

    CharClip *prevTrans = GetPrevRoutineTransition(idx);
    if (prevTrans) {
        if (!(prevTrans->DifficultyMask() & (1 << difficulty))) {
            prevTrans = nullptr;
        }
    }

    CharClip *nextTrans = nullptr;
    bool pushed = false;
    float beat = FrameToBeat(curKey.frame);
    float one = 1.0f;
    float startBeat;
    if (beat > 0.0f) {
        startBeat = beat + one;
    } else {
        startBeat = beat;
    }
    float crossoverBeat = startBeat + 1.5f;
    float startBeatPlusOne = startBeat + one;
    float blendStart = prevTrans ? startBeat : beat;

    if (mBeat < startBeatPlusOne) {
        pushed = PushRoutineBuilderClip(idx - 1, arr);
    }

    if (prevTrans && mBeat < startBeat) {
        return false;
    }

    CharClip *c2 = nullptr;
    CharClip *c1 = nullptr;
#ifdef HX_NATIVE
    if (!GetRoutineCrossoverClips(startBeat, curKey.value.Str(), &c2, &c1))
        return pushed;
#else
    GetRoutineCrossoverClips(startBeat, curKey.value.Str(), &c2, &c1);
#endif

    float nextBeat;
    if (idx != nextIdx) {
        Key<Symbol> &nextKey = mClipKeys->at(nextIdx);
        nextTrans = GetRoutineTransition(c1->Name(), &nextKey);
        if (nextTrans) {
            if (!(nextTrans->DifficultyMask() & (1 << difficulty))) {
                nextTrans = nullptr;
            }
        }
        nextBeat = FrameToBeat(nextKey.frame);
    } else {
        nextBeat = startBeat + 3.0f;
    }

    float two = 2.0f;
    float transStart = startBeat + two;
    if (nextTrans) {
        transStart = (nextBeat - ClipLength(nextTrans)) + two;
        nextBeat = transStart + one;
    }

    float hugeNeg = -kHugeFloat;
    float crossoverBeatPlusOne = crossoverBeat + one;
    float blend;

    if (c2 == c1) {
        if (mBeat >= beat && (nextTrans == nullptr || mBeat <= nextBeat)) {
            blend = pushed ? blendStart : hugeNeg;
            goto playClipC1;
        }
    } else {
        if (mBeat >= beat && mBeat <= crossoverBeatPlusOne) {
            blend = pushed ? blendStart : hugeNeg;
            PlayClip(c2, beat, blend, arr);
            pushed = true;
        }
        if (mBeat >= crossoverBeat && (nextTrans == nullptr || mBeat <= nextBeat)) {
            blend = pushed ? crossoverBeat : hugeNeg;
        playClipC1:
            PlayClip(c1, beat, blend, arr);
            pushed = true;
        }
    }

    if (nextTrans && mBeat >= transStart) {
        blend = pushed ? transStart : hugeNeg;
        PlayClip(nextTrans, transStart, blend, arr);
        pushed = true;
    }

    return pushed;
}

void ClipPlayer::PlayNormal(float f1, HamDriver::LayerArray *arr, const char *cc) {
    HamDriver::LayerArray *newArr;
    if (arr != NULL) {
        newArr = new HamDriver::LayerArray();
        arr->mLayers.push_back(newArr);
        strncpy(newArr->mName, cc, 0x1F);
    } else {
        newArr = &mDriver->Layers();
    }
    newArr->mBeat = f1 - mBeatOffset;
    if (!mClipKeys) {
        if (TheLoadMgr.EditMode()) {
            const char *msg = "No 'clips' keyframes in your song.anim.  Please don't save this song!";
            MILO_NOTIFY_ONCE(msg);
        }
    } else {
        static Symbol merge_moves("merge_moves");
        int prop = TheHamProvider->Property(merge_moves, true)->Int();
        float beat = mBeat;
        if (prop != 0 && TheMoveMgr && TheMoveMgr->HasRoutine()) {
            PushRoutineBuilderClip(mClipKeys->KeyLessEq(BeatToFrame(beat)), newArr);
        } else if (mClipKeys == mMasterClipKeys) {
            PushExpertClip(mClipKeys->KeyLessEq(BeatToFrame(beat)), newArr);
        } else {
            PushClip(mClipKeys->KeyGreaterEq(BeatToFrame(beat)), newArr);
        }
    }
}
