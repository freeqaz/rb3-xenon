#pragma once
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Group.h"
#include "ui/PanelDir.h"
#include "obj/ObjPtr_p.h"

class RndGroup;
class RndMesh;
class TrackTest;
class TrackWidget;
class ArpeggioShapePool;

/** "Base class for track system. Contains configuration for
 * track speed, length, slot positions. Manages TrackWidget instances." */
class TrackDir : public PanelDir {
public:
    TrackDir();
    OBJ_CLASSNAME(TrackDir)
    OBJ_SET_TYPE(TrackDir)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    virtual ~TrackDir();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SyncObjects();
    virtual void DrawShowing();
    virtual void Poll();
    // everything below this is for TrackDir (i.e. not from PanelDir)
    // NOTE: SyncFingerFeedback is declared LAST (after PostDraw) to match the
    // retail vtable — retail places it at the tail of TrackDir's virtuals
    // (GemTrackDir vtable @0x82026d3c slot 35), NOT before SetDisplayRange like
    // the rb3-Wii dev header. Declaring it here would push SetDisplayRange (and
    // every TrackDir virtual after it) one slot too high.
    virtual void SetDisplayRange(float) {}
    virtual void SetDisplayOffset(float, bool) {}
    virtual RndDir *SmasherPlate();
    virtual float GetFretPosOffset(int) const { return 0; }
    virtual int GetNumFretPosOffsets() const { return 0; }
    virtual float GetCurrentChordLabelPosOffset() const;
    virtual int PrepareChordMesh(unsigned int);
    virtual RndMesh *GetChordMesh(unsigned int, bool) { return nullptr; }
    virtual void SetUnisonProgress(float) {}
    virtual void ClearChordMeshRefCounts();
    virtual void DeleteUnusedChordMeshes();
    virtual void AddChordRepImpl(
        RndMesh *,
        TrackWidget *,
        TrackWidget *,
        TrackWidget *,
        float,
        const std::vector<int> &,
        class String
    ) {
        MILO_ASSERT(0, 0x68);
    }
    virtual ArpeggioShapePool *GetArpeggioShapePool();
    virtual bool IsBlackKey(int) const;
    virtual void KeyMissLeft() {}
    virtual void KeyMissRight() {}
    virtual bool IsActiveInSession() const { return false; }
    virtual void PreDraw() {}
    virtual void PostDraw() {}
    // Declared LAST to match retail vtable slot order (see note above).
    virtual void SyncFingerFeedback();

    void AddActiveWidget(class TrackWidget *);
    void AddTestWidget(class TrackWidget *, int);
    void ClearAllWidgets();
    void ClearAllGemWidgets();
    /** "Toggle running the track in test mode" */
    void ToggleRunning();
    float CutOffY() const;
    void SetupKeyShifting(RndDir *);
    void ResetKeyShifting();
    void PollActiveWidgets();
    float TopSeconds() const;
    float BottomSeconds() const;
    float SecondsToY(float) const;
    float YToSeconds(float) const;
    void SetSlotXfm(int, const Transform &);
    void MakeSecondsXfm(float, Transform &) const;
    void MakeWidgetXfm(int, float, Transform &) const;
    void MakeSlotXfm(int, Transform &) const;
    void SetScrollSpeed(float);
    float ViewTimeSeconds() const;
    void SetRunning(bool);
    bool WarnOnResort() const { return mWarnOnResort; }
    const Transform &SlotAt(int idx) const { return mSlots[idx]; }
    bool IsEnabled() const {
        return IsActiveInSession() || mShowingWhenEnabled->Showing();
    }

    NEW_OBJ(TrackDir)

    static void Register() { REGISTER_OBJ_FACTORY(TrackDir); }

    bool mRunning; // 0x238
    /** "Should contain everything to draw (except widget resources)" */
    ObjPtr<RndGroup> mDrawGroup; // 0x23c
    /** "Animated at rate where frame=y position of now bar" */
    ObjPtr<RndGroup> mAnimGroup; // 0x248
    /** "World units widgets move per second". Ranges from 1 to 10000. */
    float mYPerSecond; // 0x254
    /** "Distance where widgets are pushed onto track" */
    float mTopY; // 0x258
    /** "Distance where widgets are pruned from track" */
    float mBottomY; // 0x25c
    std::vector<Transform> mSlots; // 0x260
    std::vector<Transform> vec2; // 0x26c
    /** "WARN if widget instances are added out of order? (can be off for prototyping)" */
    bool mWarnOnResort; // 0x278
    std::vector<TrackWidget *> mActiveWidgets; // 0x27c
    ObjPtr<RndGroup> mShowingWhenEnabled; // 0x288
    ObjPtr<RndGroup> mStationaryBack; // 0x294
    ObjPtr<RndGroup> mKeyShiftStationaryBack; // 0x2a0
    ObjPtr<RndGroup> mStationaryBackAfterKeyShift; // 0x2ac
    ObjPtr<RndGroup> mMovingBack; // 0x2b8
    ObjPtr<RndGroup> mKeyShiftMovingBack; // 0x2c4
    ObjPtr<RndGroup> mKeyShiftStationaryMiddle; // 0x2d0
    ObjPtr<RndGroup> mStationaryMiddle; // 0x2dc
    ObjPtr<RndGroup> mMovingFront; // 0x2e8
    ObjPtr<RndGroup> mKeyShiftMovingFront; // 0x2f4
    ObjPtr<RndGroup> mKeyShiftStationaryFront; // 0x300
    ObjPtr<RndGroup> mStationaryFront; // 0x30c
    ObjPtr<RndGroup> mAlwaysShowing; // 0x318
    ObjPtr<RndTransformable> mRotatorCam; // 0x324
    ObjPtr<RndEnviron> mTrack; // 0x330
    ObjPtr<RndEnviron> mTrackGems; // 0x33c
    Transform unk2d8; // 0x348
    Transform unk308; // 0x388
    Transform unk338; // 0x3c8
    float unk368; // 0x408
    // NOTE: rb3-Wii (a DEV/MILO_DEBUG build) has `TrackTest *mTest;` here, but
    // RB3 retail (our X360 target) stripped MILO_DEBUG, so this 4-byte member is
    // absent. We force-define MILO_DEBUG in macros.h, which would otherwise
    // compile it in and inflate TrackDir by 4 — shifting the BandTrack base
    // subobject (and every member after it, e.g. mInUse) +4 vs retail. Removed
    // unconditionally to land BandTrack@0x40c / mInUse@0x429 like the target asm.
    // No compiled TU references TrackDir::mTest.
};
