#pragma once
#include "rndobj/Draw.h"
#include "rndobj/Text.h"
#include "obj/ObjPtr_p.h"
#include "track/TrackWidgetImp.h"
#include "track/TrackDir.h"

class RndEnviron;

/** "Any object that is placed on the track and scrolls towards the
 * player.  Can have any number of meshes and an environment. Drawn efficiently
 * and pruned automatically by TrackDir." */
class TrackWidget : public RndDrawable {
public:
    enum WidgetType {
        kImmediateWidget = 0,
        kMultiMeshWidget = 1,
        kTextWidget = 2,
        kMatWidget = 3
    };
    TrackWidget();
    OBJ_CLASSNAME(TrackWidget)
    OBJ_SET_TYPE(TrackWidget)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    virtual void Mats(std::list<class RndMat *> &, bool);
    virtual void DrawShowing();
    virtual ~TrackWidget();

    void SetScale(float);
    void Clear();
    void SyncImp();
    void Init();
    void Poll();
    void CheckValid() const;
    void CheckScales() const;
    void SetInactive();
    void SetTextAlignment(RndText::Alignment);
    int Size() const;
    void ApplyOffsets(Transform &);
    void UpdateActiveStatus();
    void RemoveAt(float);
    void RemoveAt(float, int);
    float GetFirstInstanceY();
    void AddInstance(Transform, float);
    void AddTextInstance(const Transform &, class String, bool);
    void AddMeshInstance(const Transform &, RndMesh *, float);
    void SetTrackDir(TrackDir *dir) { mTrackDir = dir; }
    bool Empty();

    float NewYOffset(float secs) const { return mYOffset + mTrackDir->SecondsToY(secs); }
    void SetBaseLength(float len) { mBaseLength = len; }

    DataNode OnSetMeshes(const DataArray *);
    DataNode OnAddInstance(const DataArray *);
    DataNode OnAddTextInstance(const DataArray *);
    DataNode OnAddMeshInstance(const DataArray *);

    NEW_OBJ(TrackWidget)
    NEW_OVERLOAD
    DELETE_OVERLOAD
    DECLARE_REVS

    static void Register() { REGISTER_OBJ_FACTORY(TrackWidget); }

    bool mActive; // 0x24
    /** "Meshes used to draw widgets, drawn in order" */
    ObjPtrList<RndMesh> mMeshes; // 0x28
    bool mWideWidget; // 0x3c
    ObjPtrList<RndMesh> mMeshesLeft; // 0x40
    ObjPtrList<RndMesh> mMeshesSpan; // 0x54
    ObjPtrList<RndMesh> mMeshesRight; // 0x68
    /** "Environment used to draw widget" */
    ObjPtr<RndEnviron> mEnviron; // 0x7c
    /** "Length of unscaled geometry, should be 0 if no duration".
     * Ranges from 1e-2 to 1000. */
    float mBaseLength; // 0x88
    /** "Width of unscaled geometry, should be 0 if no scaling".
     * Ranges from 1e-2 to 1000. */
    float mBaseWidth; // 0x8c
    /** "Allow meshes to be rotated/scaled" */
    bool mAllowRotation; // 0x90
    int mMaxMeshes; // 0x94
    /** "X offset to be applied to all widget instances" */
    float mXOffset; // 0x98
    /** "Y offset to be applied to all widget instances" */
    float mYOffset; // 0x9c
    /** "Z offset to be applied to all widget instances" */
    float mZOffset; // 0xa0
    /** "Allow widget instances to shift their X/Z coordinates in coordination with their
     * smasher during a keyboard lane shift" */
    bool mAllowShift; // 0xa4
    TrackDir *mTrackDir; // 0xa8
    TrackWidgetImpBase *mImp; // 0xac
    int mWidgetType; // 0xb0
    int mCharsPerInst; // 0xb4
    int mMaxTextInstances; // 0xb8
    ObjPtr<RndFont> mFont; // 0xbc
    ObjPtr<RndText> mTextObj; // 0xc8
    RndText::Alignment mTextAlignment; // 0xd4
    /** "Primary color for text instances" */
    Hmx::Color mTextColor; // 0xd8
    /** "Secondary color for text instances" */
    Hmx::Color mAltTextColor; // 0xe8
    /** "Individual lines can have different rotations" */
    bool mAllowLineRotation; // 0xf8
    ObjPtr<RndMat> mMat; // 0xfc
    // vtordisp 0x108 | Hmx::Object vbase 0x10c | vtordisp 0x134 | RndHighlightable 0x138 | sizeof 0x140
};
