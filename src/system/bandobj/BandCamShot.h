#pragma once
#include "rndobj/Trans.h"
#include "world/CameraShot.h"
#include "rndobj/Env.h"
#include "math/Mtx.h"

class Character;

class OldTrigger {
public:
    float frame; // 0x0
    Symbol trigger; // 0x4
};

class BandCamShot : public CamShot {
public:
    struct Target {
        // (retail mangles this `UTarget@BandCamShot@@` => struct, not class)
        Target(Hmx::Object *o)
            : mFastForward(0), mEnvOverride(o), mForceLod(-1), mTeleport(1), mReturn(1),
              mSelfShadow(1), unk1(0), unk2(1), mHide(0) {
            mXfm.Reset();
        }

        void Store(BandCamShot *);
        void UpdateTarget(Symbol, BandCamShot *);

        Symbol mTarget; // 0x0
        Transform mXfm; // 0x4
        Symbol mAnimGroup; // 0x18
        float mFastForward; // 0x1c
        Symbol mForwardEvent; // 0x20
        ObjPtr<RndEnviron> mEnvOverride; // 0x24
        // 0x5c: retail keeps mForceLod in its own `int` allocation unit
        // (`srawi r,r,29` => 3 bits at 31..29) and puts the six flags in a
        // separate 1-byte `bool` unit at 0x5c+4 = 0x60 (`lbz` + single-bit
        // `extrwi`/`clrrwi.` at masks 0x80,0x40,0x20,0x10,0x08,0x04).
        int mForceLod : 3;
        bool mTeleport : 1;
        bool mReturn : 1;
        bool mSelfShadow : 1;
        bool unk1 : 1;
        bool unk2 : 1;
        bool mHide : 1;
    };

    // size 0x20
    class TargetCache {
    public:
        Symbol unk0;
        RndTransformable *unk4;
        RndEnviron *unk8;
        Transform unkc;
    };

    BandCamShot();
    OBJ_CLASSNAME(BandCamShot);
    OBJ_SET_TYPE_ENGINE(BandCamShot);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual ~BandCamShot() {}
    virtual void StartAnim();
    virtual void EndAnim();
    virtual void SetFrame(float, float);
    virtual float EndFrame();
    virtual void SetPreFrame(float, float);
    virtual CamShot *CurrentShot() { return mCurShot; }

    RndTransformable *FindTarget(Symbol, bool);
    float GetTotalDuration();
    float GetTotalDurationSeconds();
    void Store();
    void View();
    void Freeze();
    void FreezeChar(Character *, bool);
    void ViewFreeze();
    BandCamShot *InitialShot();
    int GetNumShots();
    void AnimateShot(float, float);
    void TeleportTarget(RndTransformable *, const Transform &, bool);
    std::list<TargetCache>::iterator CreateTargetCache(Symbol);
    std::list<TargetCache>::iterator GetTargetCache(Symbol);

    static void DeleteTargetCache(std::list<TargetCache>::iterator);

    bool ShouldSetNextShot(float f1) const;

    DataNode OnTestDelta(DataArray *);
    DataNode AddTarget(DataArray *);
    DataNode OnAllowableNextShots(const DataArray *);
    DataNode OnListAnimGroups(const DataArray *);

private:
    // NB(rb3-xenon): retail-verified private (target manglings are
    // `AAA?AVDataNode...`, not `QAA`) — matches DC3 HamCamShot's grouping.
    DataNode OnListAllNextShots(const DataArray *);
    DataNode OnListTargets(const DataArray *);

protected:
    // NB(rb3-xenon): retail-verified protected (target manglings are
    // `MAA`/`IAA`, not `UAA`/`QAA`) — matches DC3 HamCamShot's grouping.
    virtual bool CheckShotStarted();
    virtual bool CheckShotOver(float);
    virtual void SetFrameEx(float, float);

    void CheckNextShots();
    void ResetNextShot();
    bool IterateNextShot();
    bool ListNextShots(std::list<BandCamShot *> &);

public:
    NEW_OVERLOAD;
    DELETE_OVERLOAD;
    static std::list<BandCamShot::TargetCache> sCache;
    static int sHideAllCharactersHack;
    NEW_OBJ(BandCamShot)
    static void Init() { Register(); }
    static void Register() { REGISTER_OBJ_FACTORY(BandCamShot); }

    // Retail RB3-360 layout, recovered from objdiff and corroborated member for
    // member by DC3's HamCamShot (the same class one engine revision later).
    // Two corrections vs. what we had:
    //   * mTargets is an ObjList (0xc bytes: std::list + owner), not an
    //     ObjVector (0x10) -- HamCamShot::mTargets is ObjList<Target> too. The
    //     extra word pushed every member below by +4.
    //   * unk168 (HamCamShot::mInSetFrame) sits BETWEEN unk160 and unk164
    //     (HamCamShot::mNextShotOffset / mNextShotDuration / mInSetFrame /
    //     mTotalDuration), so it gets its own 4-byte allocation unit.
    // Offsets below are the retail ones, verified against seven distinct
    // this-relative accesses in AddDircut/PlayNextShot/ResetNextShot/
    // ListNextShots/SetFrameEx/CheckShotStarted/CheckShotOver.
    ObjList<Target> mTargets; // 0x19c
    int mMinTime; // 0x1a8
    int mMaxTime; // 0x1ac
    float mZeroTime; // 0x1b0
    ObjPtrList<BandCamShot> mNextShots; // 0x1b4
    ObjPtrList<BandCamShot>::iterator mShotIter; // 0x1c8
    ObjPtr<BandCamShot> mCurShot; // 0x1cc
    float unk15c; // 0x1d8
    float unk160; // 0x1dc
    bool unk168; // 0x1e0  (HamCamShot::mInSetFrame)
    float unk164; // 0x1e4  (HamCamShot::mTotalDuration)
    bool unk169; // 0x1e8  (HamCamShot::mListingShots)
    bool unk16a; // 0x1e9  (HamCamShot::mTargetsFlipped)
    bool mAnimsDuringNextShots; // 0x1ea
};

BinStream &operator<<(BinStream &bs, const BandCamShot::Target &t);
