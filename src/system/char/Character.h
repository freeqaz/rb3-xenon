#pragma once
#include "char/CharDriver.h"
#include "char/CharServoBone.h"
#include "math/Sphere.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Group.h"
#include "rndobj/Trans.h"
#include "utl/MemMgr.h"

class Waypoint;
class CharEyes;
class CharInterest;
class CharacterTest;
class ShadowBone;
class RndPollable;

enum LODType {
    kLODPerFrame = -1,
    kLOD0 = 0,
    kLOD1 = 1,
    kLOD2 = 2,
    kNumLods = 3
};

class Character : public RndDir {
    friend class BandWardrobe; // BandWardrobe.cpp calls OnGetCurrentInterests on its targets
public:
    struct Lod {
        Lod(Hmx::Object *owner) : mScreenSize(0), mGroup(owner, 0), mTransGroup(owner, 0) {}

        RndGroup *Group() const { return mGroup; }
        RndGroup *TransGroup() const { return mTransGroup; }

        /** "when the unit sphere centered on the bounding sphere
            is smaller than this screen height fraction,
            it will draw the next lod". Ranges from 0 to 10000. */
        float mScreenSize; // 0x0
        /** "group to show at this LOD. Drawables not in any lod group
            will be drawn at every LOD" */
        ObjPtr<RndGroup> mGroup; // 0x4
        /** "translucency group to show at this LOD. Drawables in it are
            guaranteed to be drawn last." */
        ObjPtr<RndGroup> mTransGroup; // 0x10
    };

    enum DrawMode {
        kCharDrawNone,
        kCharDrawOpaque,
        kCharDrawTranslucent,
        kCharDrawAll,
        // X2: value 4 is REAL and REACHABLE -- Character::Draw builds it
        // explicitly (`DrawLodOrShadow(lod, useAll ? (DrawMode)4 : mDrawMode)`,
        // Character.cpp:923) for the shadow render passes (rndDrawMode
        // kDrawShadowDepth/kDrawExtrude/kDrawShadowColor), and
        // DrawLodOrShadow:928 tests `drawMode == 4` to dispatch
        // mShadow->DrawShowing(). It was missing from the enumerator list, so
        // the enum's declared range stopped at 3 and clang folded that test to
        // a constant false (-Wtautological-constant-out-of-range-compare) --
        // i.e. CHARACTER SHADOWS WERE DEAD in any build that trusts the range.
        // MSVC does not narrow this way, so retail is unaffected and the X360
        // match is untouched: an added enumerator emits no code, and the
        // underlying type is `int` at 0..3 and at 0..4 alike. The two casts at
        // the call sites are left as-is so the matched bodies are byte-stable.
        kCharDrawShadow = 4
    };
    enum PollState {
        kCharCreated = 0,
        kCharSyncObject = 1,
        kCharEntered = 2,
        kCharPolled = 3,
        kCharExited = 4,
    };

    Character();
    // Hmx::Object
    virtual ~Character();
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(Character);
    OBJ_SET_TYPE(Character);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void PreSave(BinStream &) { UnhookShadow(); }
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // ObjMacros.h-dialect rev statics (see rb3-Wii Character.cpp's DECLARE_REVS):
    // retail's PreLoad reads a rev directly into these class statics rather than
    // constructing a full virtual BinStreamRev wrapper (verified from asm — no
    // ??0BinStream@@QAA@_N@Z / ??_7BinStreamRev@@6B@ in retail's PreLoad body).
    // Declared as one aggregate (not two loose statics) so the compiler is
    // permitted (by the standard's guaranteed member layout) to CSE the base
    // address across both fields into a single addi+sth/sth pair, matching
    // retail's measured .data layout (base+0 = altRev, base+4 = rev).
    struct RevState {
        __declspec(align(4)) unsigned short altRev;
        __declspec(align(4)) unsigned short rev;
    };
    static RevState gRevs;
    // RndDrawable
    virtual void UpdateSphere();
    virtual void DrawShowing();
    DRAW_DC3_VIRTUAL void DrawShadow(const Transform &, float);
    virtual RndDrawable *CollideShowing(const Segment &, float &, Plane &);
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();
    // ObjectDir
    virtual void SyncObjects();
    // NON-virtual, same as RndDir::CollideListSubParts (see rndobj/Dir.h): the
    // rb3-Wii oracle's Character declares no CollideListSubParts at all, and
    // retail's BandCharacter vtable puts Teleport at inherited-region slot 11
    // (measured: OnCamTeleport/OnClosetTeleport vcall `lwz r11,0x2c(r11)` vs our
    // 0x30). Keeping it virtual here reinserted the bogus DC3 slot that Dir.h
    // had already removed, shifting Teleport and every Character virtual after
    // it by +1 in every Character-descendant vtable.
    void
    CollideListSubParts(const Segment &, std::list<RndDrawable::Collision> &);

    virtual void Teleport(Waypoint *);
    /** "Calculates a new bounding sphere" */
    virtual void CalcBoundingSphere();
    virtual bool MakeWorldSphere(Sphere &, bool);
    virtual float ComputeScreenSize(RndCam *);
    // Virtual, and declared HERE (right after ComputeScreenSize, before GetEyes)
    // to match retail's inherited-region slot order — same position the rb3-Wii
    // oracle's Character gives it. This slot is what the old bogus virtual
    // CollideListSubParts was accidentally compensating for.
    virtual void DrawLodOrShadow(int, DrawMode);
    DRAW_DC3_VIRTUAL void DrawOpaque();
    DRAW_DC3_VIRTUAL void DrawTranslucent();
    virtual CharEyes *GetEyes();
    virtual bool ValidateInterest(CharInterest *, ObjectDir *) { return true; }
    virtual bool SetFocusInterest(CharInterest *, int);
    virtual void SetInterestFilterFlags(int);
    virtual void ClearInterestFilterFlags();

    OBJ_MEM_OVERLOAD(0x57)
    NEW_OBJ(Character)

    void SetSphereBase(RndTransformable *);
    bool SetFocusInterest(Symbol, int);
    void MergeDraws(const Character *);
    void FindInterestObjects(ObjectDir *);
    void EnableBlinks(bool, bool);
    void SetInterestObjects(const ObjPtrList<CharInterest> &, ObjectDir *);
    void SetSelfShadow(bool selfshadow) { mSelfShadow = selfshadow; }
    bool SelfShadow() const { return mSelfShadow; }
    void SetLodType(LODType lod) { mForceLod = lod; }
    void SetDebugDrawInterestObjects(bool);
    void ForceBlink();
    void CopyBoundingSphere(Character *);
    CharServoBone *BoneServo();
    void DrawLod(int);
    void SetTeleport(bool t) { mTeleported = t; }
    void SetTeleported(bool t) { mTeleported = t; }
    void SetFrozen(bool b) { mFrozen = b; }
    void SetMinLod(int lod) { mForceLod = (LODType)lod; }
    int MinLod() const { return mForceLod; }
    void RepointSphereBase(ObjectDir *);
    void RemoveFromPoll(RndPollable *);
    CharDriver *Driver() const { return mDriver; }
    bool DebugDrawInterestObjects() const { return false; }
    bool Synced() const { return mPollState == kCharSyncObject; }
    bool Teleported() const { return mTeleported; }
    // rename once you have a better idea of what this does
    bool LODCheck() const { return mForceLod > 0; }
    PollState GetPollState() const { return mPollState; }
    int LastLod() const { return mLastLod; }

    static void Init();
    static void Terminate();
    static Character *Current() { return sCurrent; }
    static void SetCurrent(Character *c) { sCurrent = c; }
    void SetDrawMode(DrawMode m) { mDrawMode = m; }

#ifdef HX_NATIVE
    /** X7, native-only read accessor for the protected LOD vector.
     *
     *  BandCharacter::RebindOutfitBonesToOwnSkeleton (a native-only routine)
     *  has to reach each LOD's draw group and trans group: the band member's
     *  BODY CLOTHING meshes live under curLod->Group() and are NOT in mDraws,
     *  so a walk that skips them never sees the torso. It was reaching in at
     *  `dc->mLods[li]`, which does not compile -- mLods is protected.
     *
     *  An accessor rather than a friend declaration or a visibility change:
     *  this is inline, const-correct, HX_NATIVE-gated and adds no member, so
     *  the X360 arm's Character is byte-identical and its objdiff position
     *  cannot move. NumDraws()/GetDraw() above it are the same pattern already
     *  present on RndDir (rndobj/Dir.h:77-78). */
    const ObjVector<Lod> &Lods() const { return mLods; }
#endif

protected:
    virtual void AddedObject(Hmx::Object *);
    virtual void RemovingObject(Hmx::Object *);

    void UnhookShadow();
    void SyncShadow();
    void SetShadow(RndGroup *);

    static Character *sCurrent;

    ShadowBone *AddShadowBone(RndTransformable *);

    DataNode OnPlayClip(DataArray *);
    DataNode OnCopyBoundingSphere(DataArray *);
    DataNode OnGetCurrentInterests(DataArray *);

    // Retail RB3 Character: mShadow / mTransGroup are single owned RndGroup
    // pointers (ObjPtr<RndGroup>, 0xc) — NOT DrawPtrVec (0x1c). DC3 introduced
    // the vec form later; RB3 retail (and the rb3-Wii oracle) use the single-
    // pointer form, which shrinks Character by 0x20 and shifts every member
    // after the shadow groups down toward the retail offsets.
    ObjVector<Lod> mLods; // 0x1dc
    int mLastLod; // 0x1ec
    /** "Forces LOD, kLODPerFrame is normal behavior of picking per frame,
        the others force the lod (0 is highest res lod, 2 is lowest res lod)" */
    LODType mForceLod; // 0x1f0
    /** "Group containing shadow geometry" */
    ObjPtr<RndGroup> mShadow; // 0x1f4
    /** "translucency group to show independent of lod. Drawables in it are
        guaranteed to be drawn last." */
    ObjPtr<RndGroup> mTransGroup; // 0x200
    CharDriver *mDriver; // 0x20c
    /** "Whether this character should be self-shadowed." */
    bool mSelfShadow; // 0x210
    /** "Does the character have a spot-light cutout?" */
    bool mSpotCutout; // 0x211
    /** "Does the character render a floor shadow?" */
    bool mFloorShadow; // 0x212
    /** "Base for bounding sphere, such as bone_pelvis.mesh" */
    ObjOwnerPtr<RndTransformable> mSphereBase; // 0x214
    /** "bounding sphere for the character, fixed" */
    Sphere mBounding; // 0x220
    std::vector<ShadowBone *> mShadowBones; // 0x234
    PollState mPollState; // 0x240
    /** "Test Character by animating it" */
    CharacterTest *mTest; // 0x244
    /** "if true, is frozen in place, no polling happens" */
    bool mFrozen; // 0x248
    DrawMode mDrawMode; // 0x24c
    bool mTeleported; // 0x250
    /** "select an interest object here and select 'force_interest' below
        to force the character to look at it." */
    Symbol mInterestToForce; // 0x254
    ObjPtr<RndEnviron> unk2a0; // 0x258
    Vector3 *unk2b4; // 0x264
    // NOTE: DC3 has a `DrawPtrVec mShowableProps` (showable_props) and a
    // `bool mDebugDrawInterestObjects` here. RB3 retail has NEITHER: the
    // "showable_props" / "prop_N_showing" / "debug_draw_interest_objects"
    // property strings are all absent from the retail XEX, Character::Save/Copy
    // stop at mFrozen, and rb3-Wii gates mDebugDrawInterestObjects behind
    // MILO_DEBUG (off in retail). Together they add 0x20 to sizeof(Character),
    // shifting every member of every subclass (BandCharacter/Char/Crowd/
    // HamCharacter/...) down 0x20. Dropping both realigns the whole family.
    // (This project's src/macros.h force-defines MILO_DEBUG, so the member must
    // be removed outright rather than #ifdef-gated.)
};

class AutoSetCurrentCharacter {
public:
    AutoSetCurrentCharacter(Character *c) : mSavedChar(Character::Current()) {
        Character::SetCurrent(c);
    }

    ~AutoSetCurrentCharacter() { Character::SetCurrent(mSavedChar); }

private:
    Character *mSavedChar;
};
