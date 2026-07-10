#pragma once
#include "math/Rand.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#include "world/CameraShot.h"
#include "world/Crowd.h"
#include "world/FreeCamera.h"

class WorldDir;

/** "Searches for and sequences CamShots" — retail is a STANDALONE polymorphic
 * class (vtable@0 = { virtual Handle, virtual dtor }), NOT Hmx::Object. sizeof
 * 0x34. Layout matches rb3-Wii exactly (retail 0xc ObjPtr). */
class CameraManager {
public:
    class Category {
    public:
        bool operator<(const Category &c) const { return mName < c.mName; }

        Symbol mName;
        ObjPtrList<CamShot> *mShots;
    };

    struct PropertyFilter {
        DataNode prop; // 0x0
        DataNode match; // 0x8
        int mask; // 0x10
    };

    CameraManager(WorldDir *);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~CameraManager();

    NEW_POOL_OVERLOAD(CameraManager)
    DELETE_POOL_OVERLOAD(CameraManager)
    static Rand sRand;
    static int sSeed;

    CamShot *NextShot() const { return mNextShot; }
    CamShot *CurrentShot() const { return mCurrentShot; }
    void SetBlendTime(float) {}
    bool HasFreeCam() const { return mFreeCam; }
    void ForceCamShot(CamShot *);
    FreeCamera *GetFreeCam(int);
    void DeleteFreeCam();
    CamShot *ShotAfter(CamShot *);
    CamShot *FindCameraShot(Symbol, const std::vector<PropertyFilter> &);
    CamShot *MiloCamera();
    void ForceCameraShot(CamShot *, bool);
    void PrePoll();
    void Randomize();
    void Enter();
    int NumCameraShots(
        Symbol s, const std::vector<PropertyFilter> &, std::list<CamShot *> *
    );
    void SetNextShot(CamShot *);
    void SyncObjects(WorldDir *);
    CamShot *PickCameraShot(Symbol, const std::vector<PropertyFilter> &);
    void Poll();

private:
    void StartShot_(CamShot *);
    float CalcFrame();
    void FirstShotOk(Symbol);
    void RandomizeCategory(ObjPtrList<CamShot> &);
    bool ShotMatches(CamShot *, const std::vector<PropertyFilter> &);

    DataNode OnPickCameraShot(DataArray *);
    DataNode OnFindCameraShot(DataArray *);
    DataNode OnCycleShot(DataArray *);
    DataNode OnRandomSeed(DataArray *);
    DataNode OnIterateShot(DataArray *);
    DataNode OnNumCameraShots(DataArray *);
    DataNode OnGetShotList(DataArray *);
    Symbol MakeCategoryAndFilters(DataArray *da, std::vector<PropertyFilter> &, float *);
    ObjPtrList<CamShot> &FindOrAddCategory(Symbol);

    /** "Controlling world object" */
    WorldDir *mParent; // 0x4
    std::vector<Category> mCameraShotCategories; // 0x8
    /** "Which shot to play right now" */
    ObjPtr<CamShot> mNextShot; // 0x14
    ObjPtr<CamShot> mCurrentShot; // 0x20
    float mCamStartTime; // 0x2c
    FreeCamera *mFreeCam; // 0x30
};
