#include "hamobj/HamCamShot.h"
#include "char/Character.h"
#include "flow/PropertyEventProvider.h"
#include "hamobj/HamDirector.h"
#include "hamobj/HamGameData.h"
#include "math/Mtx.h"
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/DataUtl.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Utl.h"
#include "os/Debug.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/Symbol.h"
#include "world/CameraShot.h"

HamCamShot *gHamCamShot;
std::list<HamCamShot::TargetCache> HamCamShot::sCache;

INIT_REVS(3, 0)

BinStream &operator>>(BinStream &bs, HamCamShot::Target &t);

BEGIN_LOADS(HamCamShot)
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    LOAD_SUPERCLASS(CamShot)
    d >> mTargets;
    d >> mZeroTime;
    d >> mMinTime;
    d >> mMaxTime;
    mNextShots.Load(bs, 1, nullptr, true);
    mOriginalSizeNextShots = mNextShots.size();
    int temp;
    if (d.rev > 1) {
        bs.ReadEndian(&temp, 4);
        mPlayerFlag = (HamPlayerFlags)temp;
    }
    if (d.rev > 2) {
        mMasterAnims.Load(bs, 1, nullptr, true);
    }
    ResetNextShot();
END_LOADS

void HamCamShot::EndAnim() {
    if (!mCurrentShot || mCurrentShot == this) {
        for (ObjList<Target>::iterator it = mTargets.begin(); it != mTargets.end();
             ++it) {
            Target &target = *it;
            if (!target.mTarget.Null()) {
                std::list<TargetCache>::iterator cacheIt = GetTargetCache(target.mTarget);
                if (target.mTeleport && target.mReturn && cacheIt->mTrans) {
                    TeleportTarget(cacheIt->mTrans, cacheIt->mTransform, true);
                }
                Character *theChar = dynamic_cast<Character *>(cacheIt->mTrans);
                if (theChar) {
                    theChar->SetLodType(kLODPerFrame);
                    if (target.mEnvOverride) {
                        theChar->SetEnv(cacheIt->mOldEnv);
                    }
                }
                sCache.erase(cacheIt);
            }
        }
        EndAnims(mMasterAnims);
        CamShot::EndAnim();
    } else {
        mCurrentShot->EndAnim();
        ResetNextShot();
    }
}

void HamCamShot::SetPreFrame(float frame, float blend) {
    mTargetsFlipped = true;

    bool inFirstShot = true;
    if (frame >= mDuration && mNextShots.size() != 0) {
        inFirstShot = false;
    }

    if (inFirstShot) {
        if (mCurrentShot != this) {
            ResetNextShot();
        }
    } else {
        float nextOffset = frame - mDuration;
        while (nextOffset < mNextShotOffset && mNextShotIt != mNextShots.begin()) {
            ++mNextShotIt;
            mNextShotDuration = (*mNextShotIt)->GetTotalDuration();
            mNextShotOffset -= mNextShotDuration;
            mCurrentShot = *mNextShotIt;
        }
        frame = nextOffset - mNextShotOffset;
        while (frame >= mNextShotDuration) {
            if (IterateNextShot()) {
                frame -= mNextShotDuration;
                mNextShotOffset += mNextShotDuration;
                if (mCurrentShot) {
                    mCurrentShot->EndAnim();
                }
                mCurrentShot = *mNextShotIt;
                mCurrentShot->StartAnim();
                mNextShotDuration = mCurrentShot->GetTotalDuration();
            } else {
                mNextShotDuration = kHugeFloat;
            }
        }
    }
    if (mCurrentShot != this) {
        mCurrentShot->SetFrame(frame, 1.0f);
    }
}

DataNode HamCamShot::OnAllowableNextShots(const DataArray *a) {
    DataArrayPtr result;
    {
        ObjDirItr<HamCamShot> dirIt(Dir(), true);
        while (dirIt) {
            if (this != dirIt) {
                if (!mNextShots.find(dirIt)) {
                    std::list<HamCamShot *> nextList;
                    dirIt->ListNextShots(nextList);
                    std::list<HamCamShot *>::iterator lit = nextList.begin();
                    while (lit != nextList.end()) {
                        if (*lit == this)
                            break;
                        ++lit;
                    }
                    if (lit == nextList.end()) {
                        result->Insert(result->Size(), DataNode(dirIt));
                    }
                }
            }
            ++dirIt;
        }
    }
    static DataNode &miloPropVar = DataVariable(Symbol("milo_prop_path"));
    if (miloPropVar.Type() == kDataArray && miloPropVar.Array(nullptr)->Size() == 2) {
        DataArray *arr = miloPropVar.Array(nullptr);
        int idx = arr->Node(1).Int(arr);
        ObjPtrList<HamCamShot>::iterator it = mNextShots.begin();
        while (idx != 0) {
            ++it;
            idx--;
        }
        result->Insert(result->Size(), DataNode(*it));
    }
    return DataNode(result);
}

void HamCamShot::UpdateTargetsFlipped() {
    static Symbol gameplay_mode("gameplay_mode");
    static Symbol dance_battle("dance_battle");

    bool flipped = AreTargetsFlipped();
    Symbol mode = TheHamProvider->Property(gameplay_mode, true)->Sym(NULL);
    bool isDanceBattle = (mode == dance_battle);

    if (TheHamDirector) {
        TheHamDirector->SetPhraseMetersFlipped(flipped);
    }

    WorldDir *venueWorld = TheHamDirector ? TheHamDirector->GetVenueWorld() : NULL;

    if (venueWorld != NULL) {
        static Symbol game_stage("game_stage");
        static Symbol intro("intro");
        static Symbol crew_battle_intro("crew_battle_intro");
        static Symbol crewbattle_intro("crewbattle_intro");
        static Symbol BattleIntro("BattleIntro");

        if (isDanceBattle && flipped) {
            Symbol stage = TheHamProvider->Property(game_stage, true)->Sym(NULL);
            if (stage == intro) {
                TheDebug << MakeString("Camshot %s\n", (char *)Name());
                int targetIdx = 0;
                for (ObjList<Target>::iterator it = mTargets.begin();
                     it != mTargets.end();
                     ++it) {
                    HamCharacter *character = CharacterNameToCharacter(it->mTarget);
                    ObjectDir *clipsDir = NULL;
                    if (character != NULL) {
                        clipsDir = character->Find<ObjectDir>("clips", true);
                    }

                    if (((mPlayerFlag == kHamPlayer1 && targetIdx % 2 == 0)
                         || (mPlayerFlag == kHamPlayer0 && targetIdx % 2 == 1))
                        && character != NULL) {
                        if (clipsDir != NULL) {
                            Hmx::Object *found =
                                clipsDir->Find<Hmx::Object>("crewbattle_intro", false);
                            if (found != NULL) {
                                it->mAnimGroup = crewbattle_intro;
                            } else {
                                found = clipsDir->Find<Hmx::Object>("BattleIntro", false);
                                if (found != NULL) {
                                    it->mAnimGroup = BattleIntro;
                                } else {
                                    found = clipsDir->Find<Hmx::Object>(
                                        "crew_battle_intro", false
                                    );
                                    if (found != NULL) {
                                        it->mAnimGroup = crew_battle_intro;
                                    }
                                }
                            }
                        }
                    }

                    const char *clipsDirName;
                    if (clipsDir != NULL) {
                        clipsDirName = clipsDir->Name();
                    } else {
                        clipsDirName = "NULL";
                    }
                    const char *charName;
                    if (character != NULL) {
                        charName = character->Name();
                    } else {
                        charName = "NULL";
                    }
                    TheDebug << MakeString(
                        "   Target %d: character = '%s' clips = '%s' animGroup = '%s'\n",
                        targetIdx,
                        charName,
                        clipsDirName,
                        it->mAnimGroup
                    );
                    targetIdx++;
                }
            }
        }
    }

    if (flipped != mFlipActive) {
        mFlipActive = flipped;
        CreateFlippedShowHideList();

        WorldDir *venueWorld2 = TheHamDirector ? TheHamDirector->GetVenueWorld() : NULL;

        if (venueWorld2 != NULL) {
            for (ObjVector<CamShotFrame>::iterator kit = mKeyframes.begin();
                 kit != mKeyframes.end();
                 ++kit) {
                CamShotFrame &frame = *kit;
                std::vector<RndTransformable *> newTargets;
                for (ObjPtrList<RndTransformable>::iterator tit = frame.mTargets.begin();
                     tit != frame.mTargets.end();
                     ++tit) {
                    RndTransformable *target = *tit;
                    const char *name = target->Name();
                    char buf[240];
                    const char *p = name;
                    char c;
                    do {
                        c = *p;
                        buf[p - name] = c;
                        p++;
                    } while (c != '\0');

                    RndTransformable *newTarget = target;
                    if (!flipped) {
                        if (strstr(name, "player0") && mPlayerFlag == kHamPlayer0) {
                            buf[6] = '1';
                            newTarget = venueWorld2->Find<RndTransformable>(buf, true);
                        } else if (strstr(name, "player1") && mPlayerFlag == kHamPlayer1) {
                            buf[6] = '0';
                            newTarget = venueWorld2->Find<RndTransformable>(buf, true);
                        }
                    } else {
                        if (strstr(name, "player0") && mPlayerFlag == kHamPlayer1) {
                            buf[6] = '1';
                            newTarget = venueWorld2->Find<RndTransformable>(buf, true);
                        } else if (strstr(name, "player1") && mPlayerFlag == kHamPlayer0) {
                            buf[6] = '0';
                            newTarget = venueWorld2->Find<RndTransformable>(buf, true);
                        }
                    }
                    newTargets.push_back(newTarget);
                }
                while (!frame.mTargets.empty()) {
                    frame.mTargets.pop_back();
                }
                for (std::vector<RndTransformable *>::iterator jit = newTargets.begin();
                     jit != newTargets.end();
                     ++jit) {
                    frame.mTargets.push_back(*jit);
                }
            }
        }

        if (flipped) {
            mShowList = mFlipPostProcOverrides;
            mHideList = mFlipDrawOverrides;
            mGenHideList = mFlipEndHideList;
            mGenHideVector = mFlipEndShowVector;
        } else {
            mShowList = mFlipShowList;
            mHideList = mFlipHideList;
            mGenHideList = mFlipGenHideList;
            mGenHideVector = mFlipGenHideVector;
        }
    }
}

void HamCamShot::Reteleport(const Vector3 &offset, bool teleport, Symbol sym) {
    for (ObjList<Target>::iterator it = mTargets.begin(); it != mTargets.end(); ++it) {
        Target &target = *it;
        Target *flipTarget = &target;
        if (mTargetsFlipped) {
            flipTarget = GetFlipTarget(&target);
        }

        if (target.mTarget.Null())
            continue;
        if (teleport && !target.mTeleport)
            continue;
        if (!sym.Null() & sym != target.mTarget)
            continue;

        std::list<TargetCache>::iterator cacheIt = CreateTargetCache(target.mTarget);
        if (cacheIt->mTrans) {
            cacheIt->mTransform = cacheIt->mTrans->LocalXfm();
            Transform xfm = flipTarget->mTo;

            static Symbol player0("player0");
            static Symbol player1("player1");
            static Symbol backup0("backup0");
            static Symbol backup1("backup1");
            static Symbol DC_PLAYER_FREESTYLE("DC_PLAYER_FREESTYLE");
            static Symbol INTRO_QUICK("INTRO_QUICK");
            static Symbol INTRO_PLAYLIST("INTRO_PLAYLIST");

            if (TheGameData->SidesSwapped()
                && (mPlayerFlag == kHamPlayerBoth || mPlayerFlag == kHamPlayerOff)
                && (target.mTarget == player0 || target.mTarget == player1
                    || target.mTarget == backup0 || target.mTarget == backup1)) {
                static Symbol AUTHORED_CAM_CATS("AUTHORED_CAM_CATS");
                DataArray *cats = DataGetMacro(AUTHORED_CAM_CATS);

                bool doSwap = false;
                if (cats != NULL) {
                    DataNode catNode(mCategory);
                    if (cats->Contains(catNode)) {
                        doSwap = true;
                    }
                }
                if (!doSwap) {
                    if (mCategory == DC_PLAYER_FREESTYLE || mCategory == INTRO_QUICK
                        || mCategory == INTRO_PLAYLIST) {
                        doSwap = true;
                    }
                }

                if (doSwap) {
                    Symbol otherName = GetFlipTarget(flipTarget->mTarget);
                    for (ObjList<Target>::iterator it2 = mTargets.begin();
                         it2 != mTargets.end();
                         ++it2) {
                        if (it2->mTarget == otherName) {
                            xfm = it2->mTo;
                            break;
                        }
                    }
                }
            }

            Transform result;
            Multiply(xfm, WorldXfm(), result);
            result.v += offset;
            TeleportTarget(cacheIt->mTrans, result, false);
        }
    }
    sCache.clear();
}

HamCamShot::HamCamShot()
    : mTargets(this), mMinTime(0), mMaxTime(0), mZeroTime(0), mPlayerFlag(kHamPlayerOff),
      mNextShots(this), mCurrentShot(this), mNextShotOffset(0), mNextShotDuration(0),
      mInSetFrame(0), mTotalDuration(0), mListingShots(0), mTargetsFlipped(0),
      mMasterAnims(this), mOriginalSizeNextShots(0), mFlipHideList(this),
      mFlipShowList(this), mFlipGenHideList(this), mFlipDrawOverrides(this),
      mFlipPostProcOverrides(this), mFlipEndHideList(this), mFlipActive(false) {
    mNearPlane = 10;
    mFarPlane = 10000;
    mNextShotIt = 0;
}

BEGIN_HANDLERS(HamCamShot)
    HANDLE(test_delta, OnTestDelta)
    HANDLE_EXPR(duration_seconds, GetTotalDurationSeconds())
    HANDLE_EXPR(duration, GetTotalDuration())
    HANDLE_ACTION(store, Store())
    HANDLE(add_target, AddTarget)
    HANDLE_EXPR(initial_shot, InitialShot())
    HANDLE_EXPR(num_shots, GetNumShots())
    HANDLE(allowable_next_shots, OnAllowableNextShots)
    HANDLE(list_all_next_shots, OnListAllNextShots)
    HANDLE_EXPR(find_target, FindTarget(_msg->Sym(2)))
    HANDLE(list_targets, OnListTargets)
    HANDLE_EXPR(get_original_size_next_shots, mOriginalSizeNextShots)
    HANDLE_ACTION(flip_target_anim_groups, FlipTargetAnimGroups())
    HANDLE_SUPERCLASS(CamShot)
END_HANDLERS

#define SYNC_PROP_SET_TARGET_BIT(s, member)                                              \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (_op == kPropSet) {                                                       \
                member = _val.Int();                                                     \
            } else {                                                                     \
                _val = member;                                                           \
            }                                                                            \
            return true;                                                                 \
        }                                                                                \
    }

BEGIN_CUSTOM_PROPSYNC(HamCamShot::Target)
    SYNC_PROP_SET(target, o.mTarget, o.UpdateTarget(_val.Sym(), gHamCamShot))
    SYNC_PROP(to, o.mTo)
    SYNC_PROP_MODIFY(anim_group, o.mAnimGroup, gHamCamShot->StartAnim())
    SYNC_PROP(fast_forward, o.mFastForward)
    SYNC_PROP(forward_event, o.mForwardEvent)
    SYNC_PROP_SET_TARGET_BIT(force_lod, o.mForceLOD)
    SYNC_PROP_SET_TARGET_BIT(teleport, o.mTeleport)
    SYNC_PROP_SET_TARGET_BIT(return, o.mReturn)
    SYNC_PROP_SET_TARGET_BIT(self_shadow, o.mSelfShadow)
    SYNC_PROP(env_override, o.mEnvOverride)
    SYNC_PROP_SET(target_ptr, gHamCamShot->FindTarget(o.mTarget), )
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(HamCamShot)
    gHamCamShot = this;
    SYNC_PROP(targets, mTargets)
    SYNC_PROP_SET(
        player_flag, (int &)mPlayerFlag, mPlayerFlag = (HamPlayerFlags)_val.Int()
    )
    SYNC_PROP(zero_time, mZeroTime)
    SYNC_PROP(min_time, mMinTime)
    SYNC_PROP(max_time, mMaxTime)
    SYNC_PROP_MODIFY(next_shots, mNextShots, CheckNextShots(); ResetNextShot();)
    SYNC_PROP(master_anims, mMasterAnims)
    SYNC_SUPERCLASS(CamShot)
END_PROPSYNCS

BinStream &operator<<(BinStream &bs, const HamCamShot::Target &t) {
    bs << t.mTarget;
    unsigned char teleport = t.mTeleport;
    bs.Write(&teleport, 1);
    bs << t.mTo;
    bs << t.mAnimGroup;
    unsigned char ret = t.mReturn;
    bs.Write(&ret, 1);
    bs << t.mFastForward;
    bs << t.mForwardEvent;
    unsigned char selfShadow = t.mSelfShadow;
    bs.Write(&selfShadow, 1);
    unsigned char p4 = t.unk68p4;
    bs.Write(&p4, 1);
    unsigned char p3 = t.unk68p3;
    bs.Write(&p3, 1);
    bs << t.mEnvOverride;
    unsigned char forceLOD = t.mForceLOD;
    bs.Write(&forceLOD, 1);
    return bs;
}

BinStream &operator>>(BinStream &bs, HamCamShot::Target &t) {
    bs >> t.mTarget;

    char teleport;
    bs.Read(&teleport, 1);
    t.mTeleport = (teleport != 0);

    bs >> t.mTo;
    bs >> t.mAnimGroup;

    char ret;
    bs.Read(&ret, 1);
    t.mReturn = (ret != 0);

    bs.ReadEndian(&t.mFastForward, 4);
    bs >> t.mForwardEvent;

    char selfShadow;
    bs.Read(&selfShadow, 1);
    t.mSelfShadow = (selfShadow != 0);
    char p4;
    bs.Read(&p4, 1);
    t.unk68p4 = (p4 != 0);
    char p3;
    bs.Read(&p3, 1);
    t.unk68p3 = (p3 != 0);

    t.mEnvOverride.Load(bs, true, nullptr);

    char forceLOD;
    bs.Read(&forceLOD, 1);
    t.mForceLOD = forceLOD;

    return bs;
}

BEGIN_SAVES(HamCamShot)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(CamShot)
    bs << mTargets;
    bs << mZeroTime;
    bs << mMinTime;
    bs << mMaxTime;
    bs << mNextShots;
    bs << mPlayerFlag;
    bs << mMasterAnims;
END_SAVES

BEGIN_COPYS(HamCamShot)
    COPY_SUPERCLASS(CamShot)
    CREATE_COPY(HamCamShot)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mTargets)
        COPY_MEMBER(mZeroTime)
        COPY_MEMBER(mMinTime)
        COPY_MEMBER(mMaxTime)
        COPY_MEMBER(mNextShots)
        COPY_MEMBER(mPlayerFlag)
        COPY_MEMBER(mMasterAnims)
        ResetNextShot();
    END_COPYING_MEMBERS
END_COPYS

void HamCamShot::StartAnim() {
    if (mCurrentShot && mCurrentShot != this) {
        mCurrentShot->EndAnim();
    }
    UpdateTargetsFlipped();
    ResetNextShot();
    CamShot::StartAnim();
    StartAnims(mMasterAnims);
    for (ObjList<Target>::iterator it = mTargets.begin(); it != mTargets.end(); ++it) {
        Target &cur = *it;
        if (!cur.mTarget.Null()) {
            std::list<TargetCache>::iterator cache = CreateTargetCache(cur.mTarget);
            Character *theChar = dynamic_cast<Character *>(cache->mTrans);
            if (theChar) {
                theChar->SetSelfShadow(cur.mSelfShadow);
                theChar->SetLodType((LODType)cur.mForceLOD);
                static Message msg("play_group", 0, 0, 0, 0, 0);
                msg[0] = theChar;
                msg[1] = cur.mAnimGroup;
                msg[2] = cur.mFastForward / FramesPerUnit();
                msg[3] = Units();
                msg[4] = cur.mForwardEvent;
                HandleType(msg);
                if (cur.mEnvOverride) {
                    cache->mOldEnv = theChar->GetEnv();
                    theChar->SetEnv(cur.mEnvOverride);
                }
            }
        }
    }
    Reteleport(Vector3::ZeroVec(), true, gNullStr);
    mTotalDuration = GetTotalDuration();
    static Message camshot_changed("camshot_changed");
    TheHamProvider->Export(camshot_changed, true);
    sCache.clear();
}

void HamCamShot::ListAnimChildren(std::list<RndAnimatable *> &children) const {
    CamShot::ListAnimChildren(children);
    for (ObjPtrList<RndAnimatable>::iterator it = mMasterAnims.begin();
         it != mMasterAnims.end();
         ++it) {
        children.push_back(*it);
    }
}

bool HamCamShot::TargetTeleportTransform(Symbol s, Transform &xfm) {
    for (ObjList<Target>::iterator it = mTargets.begin(); it != mTargets.end(); ++it) {
        Target &cur = *it;
        if (cur.mTeleport && s == cur.mTarget) {
            xfm = cur.mTo;
            return true;
        }
    }
    return false;
}

bool HamCamShot::IterateNextShot() {
    bool ret = true;
    MILO_ASSERT(!mNextShots.empty(), 0x166);
    ObjPtrList<HamCamShot>::iterator it = mNextShotIt;
    if (it == 0) {
        it = mNextShots.begin();
        mNextShotIt = it;
    } else {
        ++mNextShotIt;
        if (mNextShotIt == 0) {
            ret = false;
            mNextShotIt = it;
        }
    }
    return ret;
}

void HamCamShot::Target::Store(HamCamShot *shot) {
    if (!mTarget.Null()) {
        std::list<TargetCache>::iterator it = shot->CreateTargetCache(mTarget);
        if (it->mTrans) {
            mTo = it->mTrans->LocalXfm();
        }
        HamCamShot::sCache.erase(it);
    }
}

void HamCamShot::Target::UpdateTarget(Symbol s, HamCamShot *shot) {
    if (mTarget != s) {
        mTarget = s;
        mAnimGroup = "";
    }
    Store(shot);
}

std::list<HamCamShot::TargetCache>::iterator HamCamShot::GetTargetCache(Symbol s) {
    for (std::list<TargetCache>::iterator it = sCache.begin(); it != sCache.end(); ++it) {
        if (s == it->mTargetName)
            return it;
    }
    if (!TheLoadMgr.EditMode()) {
        MILO_NOTIFY(
            "%s creating target cache for %s, targets changed while playing camera",
            PathName(this),
            s
        );
    }
    return CreateTargetCache(s);
}

std::list<HamCamShot::TargetCache>::iterator HamCamShot::CreateTargetCache(Symbol s) {
    TargetCache cache;
    sCache.insert(sCache.begin(), cache);
    sCache.begin()->mTargetName = s;
    sCache.begin()->mTrans = FindTarget(s);
    return sCache.begin();
}

void HamCamShot::Store() {
    for (ObjList<Target>::iterator it = mTargets.begin(); it != mTargets.end(); ++it) {
        it->Store(this);
    }
}

DataNode HamCamShot::AddTarget(DataArray *target) {
    MILO_ASSERT(target->Size() != 2, 0x213);
    mTargets.push_back(Target(this));
    mTargets.back().mTarget = target->Sym(2);
    mTargets.back().Store(this);
    return 0;
}

DataNode HamCamShot::OnTestDelta(DataArray *a) {
    float f = a->Float(2);
    return (mMinTime == 0 || f >= mMinTime) && (mMaxTime == 0 || f <= mMaxTime);
}

DataNode HamCamShot::OnListTargets(const DataArray *a) {
    static Message msg("list_targets");
    DataNode handled = HandleType(msg);
    if (handled.Type() != kDataUnhandled) {
        return handled.Array();
    } else {
        return ObjectList(Dir(), "Trans", true);
    }
}

DataNode HamCamShot::OnListAllNextShots(const DataArray *a) {
    std::list<HamCamShot *> shots;
    ListNextShots(shots);
    DataArrayPtr ptr;
    for (std::list<HamCamShot *>::iterator it = shots.begin(); it != shots.end(); ++it) {
        ptr->Insert(ptr->Size(), *it);
    }
    return ptr;
}

RndTransformable *HamCamShot::FindTarget(Symbol target) {
    static Message msg("find_target", 0);
    msg[0] = target;
    DataNode handled = HandleType(msg);
    if (handled.Type() != kDataUnhandled) {
        return handled.Obj<RndTransformable>();
    } else {
        return Dir()->Find<RndTransformable>(target.Str(), false);
    }
}

void HamCamShot::TeleportTarget(RndTransformable *trans, const Transform &xfm, bool b3) {
    trans->SetLocalXfm(xfm);
    Character *theChar = dynamic_cast<Character *>(trans);
    if (theChar) {
        theChar->SetTeleport(true);
        static Message msg("teleport_char", 0, 0);
        msg[0] = trans;
        msg[1] = b3;
        HandleType(msg);
    }
}

void HamCamShot::ResetNextShot() {
    mNextShotIt = 0;
    mCurrentShot = this;
    mNextShotOffset = 0;
    mNextShotDuration = 0;
}

bool HamCamShot::ListNextShots(std::list<HamCamShot *> &shots) {
    if (mListingShots) {
        MILO_NOTIFY("%s infinite camera shot loop detected!", PathName(this));
        return false;
    } else {
        mListingShots = true;
        for (ObjPtrList<HamCamShot>::iterator it = mNextShots.begin();
             it != mNextShots.end();
             it) {
            shots.push_back(*it);
            if (!(*it)->ListNextShots(shots)) {
                mNextShots.erase(it++);
            } else {
                ++it;
            }
        }
        mListingShots = false;
        return true;
    }
}

int HamCamShot::GetNumShots() {
    std::list<HamCamShot *> shots;
    ListNextShots(shots);
    return shots.size() + 1;
}

float HamCamShot::GetTotalDuration() {
    float dur = mDuration;
    std::list<HamCamShot *> shots;
    ListNextShots(shots);
    for (std::list<HamCamShot *>::iterator it = shots.begin(); it != shots.end(); ++it) {
        dur += (*it)->mDuration;
    }
    return dur;
}

float HamCamShot::GetTotalDurationSeconds() {
    float dur = GetDurationSeconds();
    std::list<HamCamShot *> shots;
    ListNextShots(shots);
    for (std::list<HamCamShot *>::iterator it = shots.begin(); it != shots.end(); ++it) {
        dur += (*it)->GetDurationSeconds();
    }
    return dur;
}

void HamCamShot::CheckNextShots() {
    std::list<HamCamShot *> shots;
    ListNextShots(shots);
    if (TheLoadMgr.EditMode()) {
        mOriginalSizeNextShots = mNextShots.size();
    }
}

float HamCamShot::EndFrame() { return GetTotalDuration(); }

void HamCamShot::SetFrame(float frame, float blend) {
    if (!mTargetsFlipped) {
        SetPreFrame(frame, blend);
    }
    float origFrame = frame;
    bool inRange = (frame < mDuration) || mNextShots.empty();
    if (!inRange) {
        frame -= mNextShotOffset + mDuration;
    }
    if (CheckShotOver(origFrame)) {
        CamShot::SetShotOver();
    }
    if (this == mCurrentShot) {
        CamShot::SetFrame(frame, blend);
    } else {
        for (ObjPtrList<RndAnimatable>::iterator it = mAnims.begin(); it != mAnims.end();
             ++it) {
            (*it)->SetFrame(frame, 1.0f);
        }
        mCurrentShot->SetFrameEx(frame, blend);
        RndAnimatable::SetFrame(origFrame, blend);
    }
    CamShot::SetFrames(mMasterAnims, origFrame);
    mTargetsFlipped = false;
}

void HamCamShot::SetFrameEx(float frame, float blend) {
    mInSetFrame = true;
    SetFrame(frame, blend);
    mInSetFrame = false;
}

bool HamCamShot::AreTargetsFlipped() const {
    static Symbol flip_camshot_targets("flip_camshot_targets");
    const DataNode *prop = TheHamProvider->Property(flip_camshot_targets, true);
    if (prop) {
        return prop->Int(NULL) != 0;
    } else {
        return false;
    }
}

Symbol HamCamShot::GetFlipTarget(Symbol s) const {
    static Symbol player0("player0");
    static Symbol player1("player1");
    static Symbol backup0("backup0");
    static Symbol backup1("backup1");
    if (s == player0) {
        return player1;
    } else if (s == player1) {
        return player0;
    } else if (s == backup0) {
        return backup1;
    } else if (s == backup1) {
        return backup0;
    }
    return s;
}

HamCamShot::Target *HamCamShot::GetFlipTarget(Target *target) {
    Symbol origTarget = target->mTarget;
    Symbol flipped = GetFlipTarget(origTarget);
    Target *result;
    if (origTarget != flipped) {
        ObjList<Target>::iterator it = mTargets.begin();
        for (; it != mTargets.end(); ++it) {
            result = &*it;
            if (it->mTarget == flipped) {
                goto done;
            }
        }
    }
    result = target;
done:
    return result;
}

RndDrawable *HamCamShot::GetFlipCharacter(RndDrawable *draw) {
    static Symbol player0("player0");
    static Symbol player1("player1");
    static Symbol backup0("backup0");
    static Symbol backup1("backup1");
    auto endIt = draw->Name();
    Symbol name(endIt);
    if (!TheHamDirector)
        return draw;
    HamCharacter *c;
    if (name == player0) {
        c = TheHamDirector->GetCharacter(1);
    } else if (name == player1) {
        c = TheHamDirector->GetCharacter(0);
    } else if (name == backup0) {
        c = TheHamDirector->GetBackup(1);
    } else if (name == backup1) {
        c = TheHamDirector->GetBackup(0);
    } else {
        return draw;
    }
    return c;
}

HamCharacter *CharacterNameToCharacter(Symbol s) {
    static Symbol player0("player0");
    static Symbol player1("player1");
    static Symbol backup0("backup0");
    static Symbol backup1("backup1");
    if (s == player0) {
        return TheHamDirector->GetCharacter(0);
    } else if (s == player1) {
        return TheHamDirector->GetCharacter(1);
    } else if (s == backup0) {
        return TheHamDirector->GetBackup(0);
    } else if (s == backup1) {
        return TheHamDirector->GetBackup(1);
    }
    return NULL;
}

void HamCamShot::FlipTargetAnimGroups() {
    static Symbol player0("player0");
    static Symbol player1("player1");

    ObjList<Target>::iterator p0;
    for (p0 = mTargets.begin(); p0 != mTargets.end(); ++p0) {
        if (p0->mTarget == player0)
            break;
    }

    ObjList<Target>::iterator p1;
    for (p1 = mTargets.begin(); p1 != mTargets.end(); ++p1) {
        if (p1->mTarget == player1)
            break;
    }

    if (p0 != mTargets.end()) {
        p0->mTarget = player1;
    }
    if (p1 != mTargets.end()) {
        p1->mTarget = player0;
    }
}

void HamCamShot::CreateFlippedShowHideList() {
    // Only build if all flip lists are currently empty
    if (mFlipHideList.size() > 0 || mFlipShowList.size() > 0
        || mFlipGenHideList.size() > 0 || mFlipGenHideVector.size() > 0
        || mFlipDrawOverrides.size() > 0 || mFlipPostProcOverrides.size() > 0
        || mFlipEndHideList.size() > 0 || mFlipEndShowVector.size() > 0)
        return;

    // Copy mHideList → mFlipHideList (original), mFlipDrawOverrides (flipped)
    for (ObjPtrList<RndDrawable>::iterator it = mHideList.begin(); it != mHideList.end();
         ++it) {
        RndDrawable *draw = *it;
        mFlipHideList.push_back(draw);
        mFlipDrawOverrides.push_back(GetFlipCharacter(draw));
    }

    // Copy mShowList → mFlipShowList (original), mFlipPostProcOverrides (flipped)
    for (ObjPtrList<RndDrawable>::iterator it = mShowList.begin(); it != mShowList.end();
         ++it) {
        RndDrawable *draw = *it;
        mFlipShowList.push_back(draw);
        mFlipPostProcOverrides.push_back(GetFlipCharacter(draw));
    }

    // Copy mGenHideList → mFlipGenHideList (original), mFlipEndHideList (flipped)
    for (ObjPtrList<RndDrawable>::iterator it = mGenHideList.begin();
         it != mGenHideList.end();
         ++it) {
        RndDrawable *draw = *it;
        mFlipGenHideList.push_back(draw);
        mFlipEndHideList.push_back(GetFlipCharacter(draw));
    }

    // Copy mGenHideVector → mFlipGenHideVector (original), mFlipEndShowVector (flipped)
    for (std::vector<RndDrawable *>::iterator it = mGenHideVector.begin();
         it != mGenHideVector.end();
         ++it) {
        RndDrawable *draw = *it;
        mFlipGenHideVector.push_back(draw);
        mFlipEndShowVector.push_back(GetFlipCharacter(draw));
    }
}

HamCamShot *HamCamShot::InitialShot() {
    HamCamShot *initialShot = this;
    ObjRef::iterator it = initialShot->Refs().begin();
    while (it != initialShot->Refs().end()) {
        HamCamShot *cur = dynamic_cast<HamCamShot *>((*it).RefOwner());
        if (cur) {
            for (ObjPtrList<HamCamShot>::iterator ni = cur->mNextShots.begin();
                 ni != cur->mNextShots.end();
                 ++ni) {
                if (*ni == initialShot) {
                    initialShot = cur;
                    MILO_ASSERT(cur != this, 0x268);
                    it = initialShot->Refs().begin();
                    break;
                }
            }
        } else {
            ++it;
        }
    }
    return initialShot;
}
