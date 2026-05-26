#pragma once
#include "gesture/CameraInput.h"
#include "gesture/Skeleton.h"
#include "gesture/SkeletonHistory.h"
#include "obj/Object.h"
#include "os/CritSec.h"
#include "xdk/nui/nuiskeleton.h"

class SkeletonUpdate;

class SkeletonUpdateHandle {
    friend DWORD SkeletonUpdateThread(LPVOID);
    friend DataNode OnToggleSkeletalUpdateThread(DataArray *);
    friend DataNode OnCycleNumStubSkeletons(DataArray *);
    friend DataNode OnCycleFakeShellSkeletons(DataArray *);
    friend DataNode OnCycleActiveFakeShellSkeleton(DataArray *);
    friend DataNode OnSetFakeSkeletonSidesSwapped(DataArray *);
    friend DataNode OnGetFakeSkeletonSidesSwapped(DataArray *);

public:
    SkeletonUpdateHandle(SkeletonUpdate *);
    ~SkeletonUpdateHandle();

    std::vector<SkeletonCallback *> &Callbacks();
    CameraInput *GetCameraInput() const;
    void SetCameraInput(CameraInput *);
    void RemoveCallback(SkeletonCallback *);
    void AddCallback(SkeletonCallback *);
    bool HasCallback(SkeletonCallback *);
    void PostUpdate();
    const SkeletonHistory *History() const;

private:
    SkeletonUpdate *mInst; // 0x0

    static CriticalSection sCritSec;
};

class SkeletonUpdate : public SkeletonHistoryArchive,
                       public SkeletonHistory,
                       public Hmx::Object {
    friend class SkeletonUpdateHandle;
    friend DWORD SkeletonUpdateThread(LPVOID);
    friend DataNode OnToggleSkeletalUpdateThread(DataArray *);
    friend DataNode OnCycleNumStubSkeletons(DataArray *);
    friend DataNode OnCycleFakeShellSkeletons(DataArray *);
    friend DataNode OnCycleActiveFakeShellSkeleton(DataArray *);
    friend DataNode OnSetFakeSkeletonSidesSwapped(DataArray *);
    friend DataNode OnGetFakeSkeletonSidesSwapped(DataArray *);

public:
    // SkeletonHistory
    virtual bool PrevSkeleton(const Skeleton &, int, ArchiveSkeleton &, int &) const;
    // Hmx::Object
    virtual ~SkeletonUpdate();

    static void Init();
    static void CreateInstance();
    static void Terminate();
    static bool HasInstance();
    static HANDLE NewSkeletonEvent();
    static HANDLE SkeletonUpdatedEvent() { return sSkeletonUpdatedEvent; }
    static SkeletonUpdateHandle InstanceHandle();

private:
    SkeletonUpdate();

    virtual bool Replace(ObjRef *, Hmx::Object *);

    void SetCameraInput(CameraInput *);
    void PostUpdate();
    void Update();
    void UpdateCallbacks();
    void UpdateFakeArmPos();
    void InsertFakeArmPos(SkeletonData &);

    static SkeletonUpdate *sInstance;
    static HANDLE sNewSkeletonEvent;
    static HANDLE sSkeletonUpdatedEvent;

#ifdef HX_NATIVE
    // Native-only fallback: provides SkeletonHistory when sInstance is null
    // (SkeletonUpdate is not instantiated on native because it requires Xbox
    // threading and NUI hardware). Set by GestureMgr_NativeInit().
    static const SkeletonHistory *sNativeHistoryFallback;
public:
    static void SetNativeHistoryFallback(const SkeletonHistory *h) { sNativeHistoryFallback = h; }
private:
#endif

    bool mHasNewFrame; // 0x78
    ObjOwnerPtr<CameraInput> mCameraInput; // 0x7c
    bool mIsCameraConnected; // 0x90
    bool mIsCameraOverride; // 0x91
    std::vector<SkeletonCallback *> mCallbacks; // 0x94
    SkeletonFrame mSkeletonFrame; // 0xa0
    Skeleton mSkeletons[6]; // 0x1268
    Skeleton *mSkeletonsLeft[2]; // 0x5360
    Skeleton *mSkeletonsRight[2]; // 0x5368
    int unk5370;
    int unk5374;
    int unk5378;
    int unk537c;
    int mSkeletonTrackingIDs[2]; // 0x5380
    int unk5388; // 0x5388
    int unk538c; // 0x538c
    bool mSwapSides; // 0x5390 - sides swapped?
    int unk5394;
    float unk5398;
    bool mIsUpdateThreadActive; // 0x539c
    HANDLE mUpdateThread;
    NUI_SKELETON_FRAME *mNUISkeletonFrame; // 0x53a4
};
