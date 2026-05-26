#include "char/CharDriver.h"
#include "char/Char.h"
#include "char/CharBoneDir.h"
#include "char/CharClip.h"
#include "char/CharClipDisplay.h"
#include "char/CharWeightable.h"
#include "macros.h"
#include "math/Rand.h"
#include "math/Utl.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Rnd.h"
#include "utl/FilePath.h"
#include "utl/MakeString.h"
#include "utl/Symbol.h"
#include "obj/Utl.h"
#include "world/CameraShot.h"

CharDriver::CharDriver()
    : mBones(this), mClips(this), mFirst(), mTestClip(this), mDefaultClip(this), mClipGroup(this),
      mDefaultPlayStarved(false), mOldBeat(1e+30), mRealign(false), mBeatScale(1.0f), mBlendWidth(1.0f),
      mApply(kApplyBlend), mInternalBones(), mPlayMultipleClips(false) {}

CharDriver::~CharDriver() {
    if (mFirst)
        mFirst->DeleteStack();
    delete mInternalBones;
}

CharClip *CharDriver::FirstClip() {
    if (mFirst)
        return mFirst->GetClip();
    else
        return nullptr;
}

CharClipDriver *CharDriver::FirstPlaying() {
    CharClipDriver *d;
    for (d = mFirst; d != nullptr && !d->mBlendFrac; d = d->Next())
        ;
    return d;
}

CharClip *CharDriver::FirstPlayingClip() {
    CharClipDriver *d = FirstPlaying();
    if (d)
        return d->GetClip();
    else
        return nullptr;
}

CharClipDriver *CharDriver::Last() {
    CharClipDriver *d = mFirst;
    while (d && d->Next())
        d = d->Next();
    return d;
}

CharClipDriver *CharDriver::Before(CharClipDriver *driver) {
    CharClipDriver *d = mFirst;
    while (d && d->Next() != driver)
        d = d->Next();
    return d;
}

void CharDriver::AddBeat(float lo, float hi) {
    if (mFirst) {
        mFirst->mBeat += RandomFloat(lo, hi);
    }
}

void CharDriver::Clear() {
    if (mFirst)
        mFirst->DeleteStack();
    mFirst = nullptr;
}

void CharDriver::Enter() {
    Clear();
    mLastNode = 0;
    mOldBeat = kHugeFloat;
    mBeatScale = 1.0f;
    RndPollable::Enter();
    if (mDefaultClip)
        Play(DataNode(mDefaultClip), 1, -1.0f, kHugeFloat, 0.0f);
    // Previously had an HX_NATIVE fallback here that force-played the first clip
    // found in mClips when no default clip existed. Removed because it interferes
    // with HamCharacter::SongAnimation() — playing an idle clip makes
    // Driver()->FirstClip() non-null, which causes SongAnimation() to return -1,
    // permanently blocking the clip playback gate in HamDirector::Poll().
    // On Xbox, characters with no default_clip_or_group simply have no idle clip.
}

void CharDriver::Exit() { RndPollable::Exit(); }

void CharDriver::Highlight() {
#ifdef MILO_DEBUG
    if (gCharHighlightY == -1.0f)
        CharDeferHighlight(this);
    else
        gCharHighlightY = Display(gCharHighlightY);
#endif
}

void CharDriver::SetClips(ObjectDir *dir) {
    if (dir != mClips) {
        mLastNode = NULL_OBJ;
        mClips = dir;
    }
}

void CharDriver::SetBones(CharBonesObject *obj) { mBones = obj; }

float CharDriver::TopClipFrame() {
    CharClipDriver *it = mFirst;
    if (!it)
        return 0;
    else {
        while (it->Next())
            it = it->Next();
        if (!it->GetClip())
            return 0;
        else {
            float avg = it->GetClip()->AverageBeatsPerSecond();
            float frame = 0;
            if (avg < 0)
                return frame;
            else
                frame = (it->mBeat - it->GetClip()->StartBeat()) / avg;
            return frame;
        }
    }
}

DataNode CharDriver::OnSetFirstBeatOffset(DataArray *msg) {
    if (mFirst) {
        mFirst->SetBeatOffset(msg->Float(2), (TaskUnits)msg->Int(3), msg->Sym(4));
    }
    return 0;
}

DataNode CharDriver::OnPrint(const DataArray *) {
    MILO_LOG("%s\n", PathName(this));
    for (CharClipDriver *it = mFirst; it != nullptr; it = it->Next()) {
        MILO_LOG("   clip %s blend %.3f\n", it->GetClip()->Name(), it->mBlendFrac);
    }
    return 0;
}

// Overload that resolves a DataNode to a CharClip and delegates to Play(CharClip*, ...)
// The thisnode copy is required for proper DataNode reference counting
CharClipDriver *
CharDriver::Play(const DataNode &node, int i, float f1, float f2, float f3) {
    DataNode thisnode(node);
    CharClipDriver *driver = Play(FindClip(node, true), i, f1, f2, f3);
    mLastNode = thisnode;
    return driver;
}

DataNode CharDriver::OnSetDefaultClip(DataArray *arr) {
    if (mClips) {
        mDefaultClip = FindClip(arr->Str(2), true);
    }
    return mDefaultClip.Ptr();
}

void CharDriver::Transfer(const CharDriver &driver) {
    Clear();
    mClips = driver.mClips;
    mLastNode = driver.mLastNode;
    mRealign = driver.mRealign;
    mBeatScale = driver.mBeatScale;
    mBlendWidth = driver.mBlendWidth;
    if (driver.mFirst)
        mFirst = new CharClipDriver(this, *driver.mFirst);
}

DataNode CharDriver::OnPlay(const DataArray *msg) {
    int i2 = msg->Size() > 3 ? msg->Int(3) : 4;
    MILO_ASSERT(msg->Size()<=4, 0x39c);
    return Play(msg->Node(2), i2, -1, kHugeFloat, 0) != nullptr;
}

DataNode CharDriver::OnPlayGroup(const DataArray *msg) {
    MILO_ASSERT(msg->Size() <= 4, 0x3a2);
    int i2 = msg->Size() > 3 ? msg->Int(3) : 4;
    return PlayGroup(msg->Str(2), i2, -1, kHugeFloat, 0) != nullptr;
}

DataNode CharDriver::OnPlayGroupFlags(const DataArray *msg) {
    MILO_ASSERT(msg->Size() <= 5, 0x3aa);
    CharClipGroup *group = mClips->Find<CharClipGroup>(msg->Str(2), false);
    if (!group) {
        MILO_NOTIFY("%s could not find group %s", PathName(this), msg->Str(2));
        return 0;
    } else {
        int clipIdx = msg->Int(3);
        int i2 = msg->Size() > 4 ? msg->Int(4) : 4;
        return Play(group->GetClip(clipIdx), i2, -1, kHugeFloat, 0) != nullptr;
    }
}

DataNode CharDriver::OnGetClipOrGroupList(DataArray *) {
    Symbol clipName = "CharClip";
    Symbol clipGrpName = "CharClipGroup";
    std::list<Hmx::Object *> objects;
    if (mClips) {
        for (ObjDirItr<Hmx::Object> it(mClips, true); it != nullptr; ++it) {
            if (IsASubclass(it->ClassName(), clipName)
                || IsASubclass(it->ClassName(), clipGrpName)) {
                objects.push_back(it);
            }
        }
    }
    DataArrayPtr ptr;
    ptr->Resize(objects.size() + 1);
    int idx = 0;
    ptr->Node(idx++) = NULL_OBJ;
    for (std::list<Hmx::Object *>::iterator it = objects.begin(); it != objects.end();
         ++it) {
        ptr->Node(idx++) = *it;
    }
    ptr->SortNodes(0);
    return ptr;
}

CharClipDriver *CharDriver::Play(CharClip *clip, int i, float f1, float f2, float f3) {
    if (!clip) {
        MILO_NOTIFY_ONCE("%s: Could not find clip to play.", PathName(this));
        return nullptr;
    } else {
        mLastNode = clip;
        if (f1 == -1.0f)
            f1 = mBlendWidth;
        if (mPlayMultipleClips) {
            for (CharClipDriver *it = mFirst; it != nullptr; it = it->Next()) {
                if (clip == it->GetClip())
                    return nullptr;
            }
        }
        mFirst =
            new CharClipDriver(this, clip, i, f1, mFirst, f2, f3, mPlayMultipleClips);
        return mFirst;
    }
}

CharClipDriver *
CharDriver::PlayGroup(const char *cc, int i, float f1, float f2, float f3) {
    if (!mClips) {
        MILO_NOTIFY("%s has no clips", PathName(this));
        return nullptr;
    } else {
        CharClipGroup *grp = mClips->Find<CharClipGroup>(cc, false);
        if (!grp) {
            MILO_NOTIFY("%s could not find group %s", PathName(this), cc);
            return nullptr;
        } else
            return PlayGroup(grp, i, f1, f2, f3);
    }
}

CharClipDriver *
CharDriver::PlayGroup(CharClipGroup *grp, int i, float f1, float f2, float f3) {
    mClipGroup = grp;
    CharClip *clip = grp->GetClip(0);
#ifdef HX_NATIVE
    if (!clip) return nullptr;
#endif
    return Play(clip, i, f1, f2, f3);
}

void CharDriver::SyncInternalBones() {
    Clear();
    mLastNode = NULL_OBJ;
    if (mInternalBones && mClipType.Null()) {
        RELEASE(mInternalBones);
    } else if (!mInternalBones && mApply == kApplyBlendWeights && !mClipType.Null()) {
        mInternalBones = new CharBonesAlloc();
    }
    if (mInternalBones) {
        mInternalBones->ClearBones();
        CharBoneDir::StuffBones(*mInternalBones, mClipType);
    }
}

void CharDriver::SetClipWeightMap() {
    mClipWeightMap.clear();
    for (CharClipDriver *it = mFirst; it != nullptr; it = it->Next()) {
        CharClip *clip = it->GetClip();
        std::map<CharClip *, float>::iterator found = mClipWeightMap.find(clip);
        if (found != mClipWeightMap.end()) {
            found->second += it->mWeight;
        } else {
            mClipWeightMap.insert(std::pair<CharClip *, float>(clip, it->mWeight));
        }
    }
}

float CharDriver::EvaluateFlags(int flags) {
    float weight = 1;
    float result = 0;
    for (CharClipDriver *it = mFirst; it != nullptr; it = it->Next()) {
        float sigmoid = EaseSigmoid(it->mBlendFrac, 0.0f, 0.0f);
        if ((it->mClip->Flags() & flags) != 0) {
            result += sigmoid * weight;
        }
        weight *= 1.0f - sigmoid;
    }
    return result;
}

bool CharDriver::Replace(ObjRef *from, Hmx::Object *to) {
    bool deleted = false;
    if (mFirst != nullptr) {
        mFirst = mFirst->DeleteRef(from, deleted);
    }
    if (deleted != false) {
        return true;
    }
    return CharWeightable::Replace(from, to);
}

BEGIN_SAVES(CharDriver)
    SAVE_REVS(0xe, 0);
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(CharWeightable)
    bs << mBones;
    bs << mClips;
    bs << mBlendWidth;
    bs << mRealign;
    bs << mApply;
    bs << mClipType;
    bs << mPlayMultipleClips;
    bs << mTestClip;
    bs << mDefaultClip;
    bs << mDefaultPlayStarved;
END_SAVES

INIT_REVS(0xe, 0)

BEGIN_LOADS(CharDriver)
    LOAD_REVS(bs)
    ASSERT_REVS(0xe, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(CharWeightable)
    if (d.rev < 3) {
        int x;
        d >> x;
    }
    d >> mBones;
    if (d.rev < 8) {
        FilePath fp;
        d >> fp;
        if (d.rev > 6 && fp.empty()) {
            d >> mClips;
        }
    } else {
        d >> mClips;
    }
    if (d.rev > 8) {
        d >> mBlendWidth;
    }
    if (d.rev > 1) {
        d >> mRealign;
    } else {
        mRealign = false;
    }
    if (d.rev > 5)
        d >> (int &)mApply;
    else if (d.rev > 4) {
        bool b48;
        d >> b48;
        mApply = (ApplyMode)(b48 != false);
    } else
        mApply = kApplyBlend;
    if (d.rev > 9)
        d >> mClipType;
    if (d.rev > 0xC)
        d >> mPlayMultipleClips;
    if (d.rev <= 9 && mClips) {
        mClipType = mClips->Type();
        if (mClipType.Null()) {
            for (ObjDirItr<CharClip> it(mClips, true); it != nullptr; ++it) {
                mClipType = it->Type();
                break;
            }
        }
    }
    SyncInternalBones();
    if (d.rev > 3) {
        mTestClip.Load(bs, false, mClips);
    }
    if (d.rev > 0xB) {
        mDefaultClip.Load(bs, false, mClips);
    }
    if (d.rev > 0xD)
        d >> mDefaultPlayStarved;
END_LOADS

static CharClip *MyFindClip(const DataNode &n, ObjectDir *dir) {
    const DataNode &node = n.Evaluate();
    Hmx::Object *obj;
    if (node.Type() == kDataObject) {
        obj = node.UncheckedObj();
    } else {
        MILO_ASSERT(node.Type() == kDataSymbol || node.Type() == kDataString, 0x12C);
        obj = dir->FindObject(node.LiteralStr(), false, true);
    }
    if (!obj)
        return nullptr;
    CharClip *clip = dynamic_cast<CharClip *>(obj);
    if (clip)
        return clip;
    CharClipGroup *group = dynamic_cast<CharClipGroup *>(obj);
    if (!group) {
        MILO_NOTIFY_ONCE(
            "%s: MyFindClip %s bad object type, not CharClip or CharClipGroup",
            PathName(dir),
            PathName(obj)
        );
        return nullptr;
    }
    return group->GetClip(0);
}

CharClip *CharDriver::FindClip(const DataNode &node, bool warn) {
    if (!mClips) {
        MILO_FAIL("%s: trying to FindClip with no mClips", PathName(this));
    }
    CharClip *clip = MyFindClip(node, mClips);
    if (!clip && warn) {
        String str;
        str << node;
        MILO_NOTIFY_ONCE("%s: missing \"%s\" in %s", PathName(this), str, mClips->Name());
    }
    return clip;
}

float CharDriver::Display(float f) {
    CharClipDisplay::Init(Dir());
    float lineSpacing = CharClipDisplay::LineSpacing();
    std::vector<CharClipDisplay> displays;
    for (CharClipDriver *it = mFirst; it != nullptr; it = it->Next()) {
        displays.push_back(CharClipDisplay());
        displays.back().mCursorBeat = it->mBeat;
        displays.back().SetClip(it->mClip, false);
        displays.back().mBlendWeight = it->mBlendFrac;
    }
    unsigned int displayCount = displays.size();
    float y = f * (float)TheRnd.Height() + (float)displayCount * lineSpacing;

    if (displayCount > 0) {
        int i = 0;
        do {
            displays[i].mDrawPosY = y - (float)i * lineSpacing;
            i++;
        } while ((unsigned int)i < displayCount);
    }

    Hmx::Object *source = CharClipDisplay::FindSource(this);
    int headerLines = 1 + (source != nullptr);
    float origF = f;
    Hmx::Rect rect(0, origF, 1.0f, f = (y + (float)headerLines * lineSpacing) / (float)TheRnd.Height() - origF);
    Hmx::Color bgColor(0, 0, 0, 0.5f);
    TheRnd.DrawRectScreen(rect, bgColor, nullptr, nullptr, nullptr);

    float oldBeat = mOldBeat;
    const char *pathName = PathName(this);
    float sEm = CharClipDisplay::GetSEm();
    float textOfs = lineSpacing * 0.1f;
    const char *dirName = Dir()->Name();
    Hmx::Color textColor(1, 1, 1, 1);
    TheRnd.DrawString(
        MakeString("%s %s, beat: %.2f", dirName, pathName, oldBeat),
        Vector2(sEm + textOfs, y + lineSpacing),
        textColor,
        true
    );

    if (displayCount > 0) {
        unsigned int i = 0;
        do {
            displays[i].DrawTrack();
            i++;
        } while (i < displayCount);
    }

    CharClipDisplay *nextDisplay = &displays[0] + 1;
    for (CharClipDriver *it = mFirst; it != nullptr; it = it->Next()) {
        CharClipDriver *next = it->Next();
        if (!next) break;

        CharClipDisplay *prevDisplay = nextDisplay - 1;
        CharClip::NodeVector *nodes =
            next->GetClip()->GetTransitions().FindNodes(it->GetClip());
        if (nodes != nullptr && nodes->size > 0) {
            int curOfs = 0;
            int nextOfs = 0;
            int i = 0;
            do {
                float xCur = nextDisplay->GetX(nodes->nodes[i].curBeat);
                for (int j = 0; j < i; j++) {
                    float xj = nextDisplay->GetX(nodes->nodes[j].curBeat);
                    if (std::fabs(xCur - xj) < 8.0f) {
                        curOfs += 11;
                    }
                }
                Hmx::Color redColor(1, 0, 0);
                TheRnd.DrawString(
                    MakeString("%d", (const CamShotFrame::BlendEaseMode &)i),
                    Vector2(xCur, nextDisplay->mDrawPosY + (float)curOfs + 1.0f),
                    redColor,
                    true
                );

                float xNext = prevDisplay->GetX(nodes->nodes[i].nextBeat);
                for (int j = 0; j < i; j++) {
                    float xj = prevDisplay->GetX(nodes->nodes[j].nextBeat);
                    if (std::fabs(xNext - xj) < 8.0f) {
                        nextOfs += 11;
                    }
                }
                Hmx::Color greenColor(0, 1, 0);
                TheRnd.DrawString(
                    MakeString("%d", (const CamShotFrame::BlendEaseMode &)i),
                    Vector2(xNext, prevDisplay->mDrawPosY - 14.0f - (float)nextOfs),
                    greenColor,
                    true
                );

                i++;
            } while (i < nodes->size);
        }

        nextDisplay->DrawBlend(next->mBeat + it->mRampIn, it->mBlendWidth);
        float rampIn = it->mRampIn;
        float negRampIn = -rampIn;
        rampIn = negRampIn >= 0.0f ? rampIn : 0.0f;
        prevDisplay->DrawBlend(rampIn + it->mBeat, it->mBlendWidth);

        nextDisplay = nextDisplay + 1;
    }

    if (displayCount > 0) {
        unsigned int i = 0;
        do {
            displays[i].DrawCursor();
            i++;
        } while (i < displayCount);
    }

    if (source) {
        static Message msg("debug_draw", DataNode(2.0f), DataNode(2.0f));
        msg[0] = displays[0].mDrawPosY + lineSpacing;
        msg[1] = TheTaskMgr.Beat();
        source->Handle(msg, false);
    }

    return f;
}

void CharDriver::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    change.push_back(mBones);
}

static bool CharDriverStarved(CharClipDriver *first) {
    if (!first) return true;
    if (first->Next() || (first->mPlayFlags & 0xF0) == 0x10)
        return false;
    return true;
}

void CharDriver::SetBeatScale(float beatscale, bool) {
    CharClipDriver *playing = FirstPlaying();
    if (playing) {
        float oldBeatScale = mBeatScale;
        float ratio = oldBeatScale / beatscale;
        for (CharClipDriver *d = playing; d != nullptr; d = d->Next()) {
            if ((playing->mPlayFlags & 0xF600) != CharClip::kPlayRealTime) {
                d->mTimeScale *= ratio;
                CharClip::SetDefaultBeatAlignModeFlag(d->mPlayFlags, CharClip::kPlayBeatTime);
            }
        }
    }
    mBeatScale = beatscale;
}

void CharDriver::Poll() {
    float beat = mBeatScale * TheTaskMgr.Beat();
    float deltaBeat = mBeatScale * TheTaskMgr.DeltaBeat();
    if (mRealign && 0 < beat) {
        beat = mBeatScale * ((float)(TheTaskMgr.CurrentBeat()) + (float)(TheTaskMgr.CurrentTick()) / 480.0f);
        if (mOldBeat == kHugeFloat)
            mOldBeat = beat;
        if ((float)std::floor(mOldBeat) != (float)std::floor(beat)) {
            CharClipDriver *playing = FirstPlaying();
            if (playing) {
                int firstFlags = (playing->mPlayFlags >> 0xC) & 0xF;
                int flags = firstFlags;
                for (CharClipDriver *it = playing->Next(); it != nullptr; it = it->Next()) {
                    int iFlags = (it->mPlayFlags >> 0xC) & 0xF;
                    if (flags < iFlags)
                        flags = iFlags;
                }
                flags--;
                if (flags > 0) {
                    float fb = (float)std::floor(beat);
                    float fo = (float)floorf(mOldBeat);
                    int i12 = (int)fb ^ ((int)fo + 1);
                    CharClipDriver *d = playing;
                    if (i12 & flags) {
                        while (d) {
                            d->mPlayFlags &= 0xffff0fff;
                            d = d->Next();
                        }
                        if (firstFlags - 1 > 0 && (i12 & firstFlags - 1)) {
                            Play(playing->GetClip(), 0x38, -1, kHugeFloat, 0);
                        }
                    }
                }
            }
        }
    }
    mOldBeat = beat;
    if (CharDriverStarved(mFirst) && !mStarvedHandler.Null()) {
        Dir()->Handle(Message(mStarvedHandler), true);
    }
    if (CharDriverStarved(mFirst) && mFirst && (mFirst->mPlayFlags & 0xF0) == 0x30) {
        int flags = mFirst->mPlayFlags;
        CharClip::SetDefaultBlendFlag(flags, 4);
        Play(mFirst->GetClip(), flags, -1, kHugeFloat, 0);
    }
    if (CharDriverStarved(mFirst) && mFirst && (mFirst->mPlayFlags & 0xF0) == 0x40) {
        Play(mLastNode, 0x44, -1, kHugeFloat, 0);
    }
    if (CharDriverStarved(mFirst) && mDefaultClip && mDefaultPlayStarved) {
        Play(DataNode(mDefaultClip), 0x44, -1, kHugeFloat, 0);
    }
    if (mFirst) {
        mFirst = mFirst->PreEvaluate(beat, deltaBeat, TheTaskMgr.DeltaSeconds());
    }
    if (mFirst) {
        float weight = Weight();
        deltaBeat = mFirst->Evaluate(beat, deltaBeat, TheTaskMgr.DeltaSeconds());
        deltaBeat = -(weight * deltaBeat - 1.0f);
        if (mPlayMultipleClips)
            deltaBeat = weight;
        if (mBones) {
            if (mApply == kApplyBlend || mApply == kApplyBlendWeights) {
                if (mInternalBones) {
                    mInternalBones->Enter();
                    mFirst->ScaleAdd(*mInternalBones, weight);
                    mInternalBones->Blend(*mBones);
                    goto apply_end;
                }
                mFirst->GetClip()->ScaleDown(*mBones, deltaBeat);
            } else if (mApply != kApplyAdd) {
                MILO_ASSERT(mApply == kApplyRotateTo, 0x22F);
                mFirst->RotateTo(*mBones, weight);
                goto apply_end;
            }
            mFirst->ScaleAdd(*mBones, weight);
        apply_end:;
        }
    }
}

DataNode CharDriver::OnEvaluateFlags(const DataArray *msg) {
    return EvaluateFlags(msg->Int(2));
}

DataNode CharDriver::OnGetFirstFlags(const DataArray *msg) {
    CharClip *clip = FirstPlayingClip();
    return clip ? clip->Flags() : 0;
}

BEGIN_HANDLERS(CharDriver)
    HANDLE(play, OnPlay)
    HANDLE(play_group, OnPlayGroup)
    HANDLE(play_group_flags, OnPlayGroupFlags)
    HANDLE_ACTION(add_beat, AddBeat(_msg->Float(2), _msg->Float(_msg->Size() - 1)))
    HANDLE(evaluate_flags, OnEvaluateFlags)
    HANDLE(get_first_flags, OnGetFirstFlags)
    HANDLE_EXPR(first_clip, mFirst ? (Hmx::Object *)mFirst->GetClip() : (Hmx::Object *)0)
    HANDLE_ACTION(set_clip_type, mClipType = _msg->Sym(2))
    HANDLE_ACTION(set_beat_scale, SetBeatScale(_msg->Float(2), true))
    HANDLE_ACTION(transfer, Transfer(*_msg->Obj<CharDriver>(2)))
    HANDLE(print, OnPrint)
    HANDLE(set_default_clip, OnSetDefaultClip)
    HANDLE(set_first_beat_offset, OnSetFirstBeatOffset)
    HANDLE_ACTION(clear, Clear())
    HANDLE(get_clip_or_group_list, OnGetClipOrGroupList)
    HANDLE_EXPR(default_clip, mDefaultClip.Ptr())
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_COPYS(CharDriver)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharDriver)
    BEGIN_COPYING_MEMBERS
        mBones = c->GetBones();
        COPY_MEMBER(mClips)
        COPY_MEMBER(mRealign)
        COPY_MEMBER(mBeatScale)
        COPY_MEMBER(mBlendWidth)
        COPY_MEMBER(mTestClip)
        COPY_MEMBER(mClipType)
        COPY_MEMBER(mApply)
        COPY_MEMBER(mDefaultClip)
        COPY_MEMBER(mDefaultPlayStarved)
        COPY_MEMBER(mPlayMultipleClips)
        SyncInternalBones();
    END_COPYING_MEMBERS
END_COPYS

BEGIN_PROPSYNCS(CharDriver)
    SYNC_PROP(bones, mBones)
    SYNC_PROP_SET(clips, mClips.Ptr(), SetClips(_val.Obj<ObjectDir>()))
    SYNC_PROP_SET(clip_type, mClipType, SetClipType(_val.Sym()))
    SYNC_PROP(realign, mRealign)
    SYNC_PROP_SET(apply, mApply, SetApply((ApplyMode)_val.Int()))
    SYNC_PROP_SET(first_playing_clip, FirstPlayingClip(), )
    SYNC_PROP(beat_scale, mBeatScale)
    SYNC_PROP(blend_width, mBlendWidth)
    SYNC_PROP(default_clip_or_group, mDefaultClip)
    SYNC_PROP(default_play_starved, mDefaultPlayStarved)
    SYNC_PROP(test_clip, mTestClip)
    SYNC_PROP(play_multiple_clips, mPlayMultipleClips)
    SYNC_PROP(display_zoom, CharClipDisplay::sZoom)
    SYNC_SUPERCLASS(CharWeightable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

#ifndef HX_NATIVE
// Template instantiation for std::map<CharClip*, float>
namespace stlpmtx_std {

template class _Rb_tree<CharClip*, less<CharClip*>, pair<CharClip* const, float>,
    _Select1st<pair<CharClip* const, float> >,
    priv::_MapTraitsT<pair<CharClip* const, float> >,
    StlNodeAlloc<_Rb_tree_node<pair<CharClip* const, float> > > >;

} // namespace stlpmtx_std
#endif
