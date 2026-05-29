#pragma once
#include "math/Color.h"
#include "math/Mtx.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"

// size 0x60 (retail). DC3 (newer engine) extended this to 0x68 by adding the
// UV-tile-animation fields mCurrentTileIndex/mTileTime; retail RB3 has no
// tile animation, so those are HX_NATIVE-only (native build uses DC3 semantics).
class RndParticle {
public:
#ifdef HX_NATIVE
    MEM_ARRAY_OVERLOAD(Particle, 0x1E);
#else
    // Retail RB3 allocates the pool via the 2-arg MemAlloc(size, align) form
    // (verified: ParticleCommonPool::InitPool calls the heap allocator as
    // MemAlloc(size*0xb0, 0) with no __FILE__/__LINE__/name strings).
    static void *operator new[](unsigned int s) { return MemAlloc(s, 0); }
    static void *operator new[](unsigned int s, void *place) { return place; }
    static void operator delete[](void *v) { MemFree(v); }
#endif

    Hmx::Color col; // 0x0
    Hmx::Color colVel; // 0x10
    Vector4 pos; // 0x20
    Vector4 vel; // 0x30
    float deathFrame; // 0x40
    float birthFrame; // 0x44
    float size; // 0x48
    float sizeVel; // 0x4c
    float angle; // 0x50
    float swingArm; // 0x54
    RndParticle *prev; // 0x58
    RndParticle *next; // 0x5c
#ifdef HX_NATIVE
    int mCurrentTileIndex;
    float mTileTime;
#endif
};

// size 0xb0 (retail; matches the 0xb0 pool stride in ParticleCommonPool::InitPool).
// Offsets here assume the retail RndParticle base (0x60). DC3 (newer engine) added
// the birth-momentum / RPM-velocity fields mRPMVelocity..mBirthVelocityZ that push
// the struct to 0xc8; retail RB3 has no birth-momentum, so those are HX_NATIVE-only.
class RndFancyParticle : public RndParticle {
public:
    float growFrame; // 0x60
    float growVel; // 0x64
    float shrinkFrame; // 0x68
    float shrinkVel; // 0x6c
    Hmx::Color midcolVel; // 0x70
    float midcolFrame; // 0x80
    float beginGrow; // 0x84
    float midGrow; // 0x88
    float endGrow; // 0x8c
    Vector4 bubbleDir; // 0x90
    float bubbleFreq; // 0xa0
    float bubblePhase; // 0xa4
    float RPF; // 0xa8
    float swingArmVel; // 0xac
#ifdef HX_NATIVE
    float mRPMVelocity;
    float mPitchAngularVel;
    float mBirthVelocityX;
    float mBirthVelocityY;
    float mBirthVelocityZ;
#endif
};

class ParticleCommonPool {
public:
    ParticleCommonPool()
        : mPoolParticles(nullptr), mPoolFreeParticles(nullptr), mNumActiveParticles(0),
          mHighWaterMark(0) {}
    void InitPool();
    RndParticle *AllocateParticle();
    RndParticle *FreeParticle(RndParticle *);
    int NumActiveParticles() const { return mNumActiveParticles; }
    int HighWaterMark() const { return mHighWaterMark; }
    MEM_OVERLOAD(ParticleCommonPool, 0x254)

private:
    // Pool holds fancy particles; indexing must use the 0xb0 fancy stride
    // (see ParticleCommonPool::InitPool — mulli ...,0xb0). Matches DC3: only
    // mPoolParticles is RndFancyParticle*, the free-list head stays RndParticle*.
    RndFancyParticle *mPoolParticles; // 0x0
    RndParticle *mPoolFreeParticles; // 0x4
    int mNumActiveParticles; // 0x8
    int mHighWaterMark; // 0xc
};

class PartOverride {
public:
    PartOverride();

    unsigned int mask; // 0x0
    float life; // 0x4
    float speed; // 0x8
    float size; // 0xc
    float deltaSize; // 0x10
    Hmx::Color startColor; // 0x14
    Hmx::Color midColor; // 0x24
    Hmx::Color endColor; // 0x34
    Vector2 pitch; // 0x44
    Vector2 yaw; // 0x4c
    RndMesh *mesh; // 0x54
    Box box; // 0x58
};

// Attractor
class Attractor {
public:
    Attractor(Hmx::Object *owner) : mAttractor(owner), mStrength(1) {}
    Attractor(const Attractor &a, Hmx::Object *owner)
        : mAttractor(owner, a.mAttractor), mStrength(a.mStrength) {}
    void Save(BinStream &) const;
    void Load(BinStreamRev &);

    /** "Attracts/repulses particles" */
    ObjPtr<RndTransformable> mAttractor; // 0x0
    /** "Positive value means attract particles, negative means repulse" */
    float mStrength; // 0x14
};

/** "A ParticleSys object generates, animates, and draws large
    numbers of similar sprites. Currently particles are rendered only
    as points on the PC." */
class RndParticleSys : public RndAnimatable,
                       public RndPollable,
                       public RndTransformable,
                       public RndDrawable {
public:
    enum Type {
        kBasic = 0,
        kFancy = 1
    };
    class Burst {
    public:
        Burst() : mPeakRate(0), mHalfDuration(0), mInvHalfDuration(0), mRemainingDuration(0) {}
        bool Set(float, float);
        float Emit(float);
        float mPeakRate;
        float mHalfDuration;
        float mInvHalfDuration;
        float mRemainingDuration;
    };
    // Hmx::Object
    virtual ~RndParticleSys();
    virtual bool Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(ParticleSys);
    OBJ_SET_TYPE(ParticleSys);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndAnimatable
    virtual void SetFrame(float, float);
    virtual float EndFrame();
    // RndPollable
    virtual void Poll();
    virtual void Enter();
    // RndDrawable
    virtual void UpdateSphere();
    virtual bool MakeWorldSphere(Sphere &, bool);
    virtual void Mats(std::list<class RndMat *> &, bool);
    virtual void DrawShowing();
    // RndHighlightable
    virtual void Highlight() { RndDrawable::Highlight(); }

    virtual void SetPreserveParticles(bool b) { mPreserveParticles = b; }
    virtual void SetPool(int, Type);
    virtual void SetPersistentPool(int, Type);

    OBJ_MEM_OVERLOAD(0x90)
    NEW_OBJ(RndParticleSys)
    static void Init() { REGISTER_OBJ_FACTORY(RndParticleSys) }

    void SetRelativeMotion(float, RndTransformable *);
    void SetSubSamples(int);
    void SetMesh(RndMesh *);
    void FreeAllParticles();
    void SetAnimatedUV(bool);
    void SetTileHoldTime(float);
    void SetNumTiles(int);
    void SetGrowRatio(float);
    void SetShrinkRatio(float);
    void SetFrameDrive(bool);
    void SetPauseOffscreen(bool);
    RndParticle *AllocParticle();
    RndParticle *FreeParticle(RndParticle *);
    void MakeLocToRel(Transform &);
    void ExplicitParticles(int, bool, PartOverride &);
    void InitParticle(RndParticle *, const Transform *);
    void SetMat(RndMat *mat) { mMat = mat; }
    RndMat *GetMat() const { return mMat; }

    float CalcFrame() {
        if (mFrameDrive)
            return GetFrame();
        else
            return mElapsedTime;
    }

    const Hmx::Color &StartColorLow() const { return mStartColorLow; }
    const Hmx::Color &StartColorHigh() const { return mStartColorHigh; }
    void SetStartColor(const Hmx::Color &low, const Hmx::Color &high) {
        mStartColorLow = low;
        mStartColorHigh = high;
    }

    const Hmx::Color &EndColorLow() const { return mEndColorLow; }
    const Hmx::Color &EndColorHigh() const { return mEndColorHigh; }
    void SetEndColor(const Hmx::Color &low, const Hmx::Color &high) {
        mEndColorLow = low;
        mEndColorHigh = high;
    }
    const Vector2 &EmitRate() const { return mEmitRate; }
    void SetEmitRate(float x, float y) { mEmitRate.Set(x, y); }
    const Vector2 &Speed() const { return mSpeed; }
    void SetSpeed(float x, float y) { mSpeed.Set(x, y); }
    const Vector2 &Life() const { return mLife; }
    void SetLife(float x, float y) { mLife.Set(x, y); }
    const Vector2 &StartSize() const { return mStartSize; }
    void SetStartSize(float x, float y) { mStartSize.Set(x, y); }
    const Vector2 &DeltaSize() const { return mDeltaSize; }
    const Vector2 &Pitch() const { return mPitch; }
    const Vector2 &Yaw() const { return mYaw; }
    const Vector3 &BoxExtent1() const { return mBoxExtent1; }
    const Vector3 &BoxExtent2() const { return mBoxExtent2; }

    void SetBoxExtent(const Vector3 &v1, const Vector3 &v2) {
        mBoxExtent1 = v1;
        mBoxExtent2 = v2;
    }

    const Vector2 &BubbleSize() const { return mBubbleSize; }
    void SetBubbleSize(float x, float y) {
        mBubbleSize.x = x;
        mBubbleSize.y = y;
    }
    const Vector2 &BubblePeriod() const { return mBubblePeriod; }
    void SetBubblePeriod(float x, float y) {
        mBubblePeriod.x = x;
        mBubblePeriod.y = y;
    }
    const Vector3 &ForceDir() const { return mForceDir; }
    void SetForceDir(const Vector3 &v) { mForceDir = v; }
    void SetDeltaSize(float x, float y) {
        mDeltaSize.x = x;
        mDeltaSize.y = y;
    }
    void SetRotate(bool b) { mRotate = b; }
    void SetAlignWithVelocity(bool b) { mAlignWithVelocity = b; }
    void SetStretchWithVelocity(bool b) { mStretchWithVelocity = b; }
    void SetConstantArea(bool b) { mConstantArea = b; }

    void SetMaxBurst(int i) { mMaxBurst = i; }
    void SetTimeBetweenBursts(float f1, float f2) { mBurstInterval.Set(f1, f2); }
    void SetPeakRate(float f1, float f2) { mBurstPeak.Set(f1, f2); }
    void SetDuration(float f1, float f2) { mBurstLength.Set(f1, f2); }
    void SetRPM(float f1, float f2) { mRPM.Set(f1, f2); }
    void SetRPMDrag(float f) { mRPMDrag = f; }
    void SetStartOffset(float f1, float f2) { mStartOffset.Set(f1, f2); }
    void SetEndOffset(float f1, float f2) { mEndOffset.Set(f1, f2); }
    void SetDrag(float f) { mDrag = f; }
    void SetStretchScale(float f) { mStretchScale = f; }

    const Hmx::Color &MidColorLow() const { return mMidColorLow; }
    const Hmx::Color &MidColorHigh() const { return mMidColorHigh; }
    RndMesh *GetMesh() const { return mMeshEmitter; }
    RndParticle *ActiveParticles() const { return mActiveParticles; }
#ifdef HX_NATIVE
    int NumTilesAcross() const { return mNumTilesAcross; }
    int NumTilesDown() const { return mNumTilesDown; }
#endif

protected:
    RndParticleSys();

    void UpdateParticles();
    void UpdateRelativeXfm();
    void MoveParticles(float, float);
    void CreateParticles(float, float, const Transform &);
    void RunFastForward();
    float CheckBursts(float);
    void InitParticle(float, RndParticle *, const Transform *, PartOverride &);

    DataNode OnSetStartColor(const DataArray *);
    DataNode OnSetStartColorInt(const DataArray *);
    DataNode OnSetEndColor(const DataArray *);
    DataNode OnSetEndColorInt(const DataArray *);
    DataNode OnSetEmitRate(const DataArray *);
    DataNode OnAddEmitRate(const DataArray *);
    DataNode OnSetBurstInterval(const DataArray *);
    DataNode OnSetBurstPeak(const DataArray *);
    DataNode OnSetBurstLength(const DataArray *);
    DataNode OnExplicitPart(const DataArray *);
    DataNode OnExplicitParts(const DataArray *);
    DataNode OnSetLife(const DataArray *);
    DataNode OnSetSpeed(const DataArray *);
    DataNode OnSetRotate(const DataArray *);
    DataNode OnSetSwingArm(const DataArray *);
    DataNode OnSetDrag(const DataArray *);
    DataNode OnSetAlignment(const DataArray *);
    DataNode OnSetStartSize(const DataArray *);
    DataNode OnSetMat(const DataArray *);
    DataNode OnSetPos(const DataArray *);
    DataNode OnActiveParticles(const DataArray *);

    Type mType; // 0x118
    /** "maximum number of particles". Ranges from 0 to 3072. */
    int mMaxParticles; // 0x11c
    RndParticle *mPersistentParticles; // 0x120
    RndParticle *mFreeParticles; // 0x124
    RndParticle *mActiveParticles; // 0x128
    int mNumActive; // 0x12c
    float mEmitCount; // 0x130
    bool mFrameDrive; // 0x134
    float mLastFrame;
    int mDrawCount;
    /** "Freezes the particle motion when they are offscreen, CPU savings" */
    bool mPauseOffscreen; // 0x140
    float mPausedTime;
    Vector2 mBubblePeriod; // 0x148
    Vector2 mBubbleSize; // 0x150
    /** "Frame range of particle life." */
    Vector2 mLife; // 0x158
    /** "Min point and max point, in object coordinates,
        of box region that particles are emitted from." */
    Vector3 mBoxExtent1; // 0x160
    /** "Min point and max point, in object coordinates,
        of box region that particles are emitted from." */
    Vector3 mBoxExtent2; // 0x170
    /** "Speed range, in world units per frame, of particles." */
    Vector2 mSpeed; // 0x180
    Vector2 mPitch; // 0x188
    Vector2 mYaw; // 0x190
    /** "Frame range to generate particles." */
    Vector2 mEmitRate; // 0x198
    /** "Size range, in world units, of particles." */
    Vector2 mStartSize; // 0x1a0
    /** "Change in size of particles, in world units." */
    Vector2 mDeltaSize; // 0x1a8
    /** "Random color ranges for start and end color of particles." */
    Hmx::Color mStartColorLow; // 0x1b0
    /** "Random color ranges for start and end color of particles." */
    Hmx::Color mStartColorHigh; // 0x1c0
    /** "Random color ranges for start and end color of particles." */
    Hmx::Color mEndColorLow; // 0x1d0
    /** "Random color ranges for start and end color of particles." */
    Hmx::Color mEndColorHigh; // 0x1e0
    ObjPtr<RndMesh> mMeshEmitter; // 0x1f0
    /** "material for particle system" */
    ObjPtr<RndMat> mMat; // 0x204
    bool mPreserveParticles; // 0x218
    Transform mRelativeXfm; // 0x21c
    Transform mLastWorldXfm; // 0x25c
    /** "If [motion_parent] is set, is amount to make movement relative to it,
        0-1, 1 being fully relative to the parent" */
    float mRelativeMotion; // 0x29c
    /** "Makes particles move relative to this Trans" */
    ObjOwnerPtr<RndTransformable> mMotionParent; // 0x2a0
    Vector3 mMotionParentDelta;
    /** "Specify a collide plane to reflect particles.
        Used to bounce particles off surfaces." */
    ObjPtr<RndTransformable> mBounce; // 0x2c4
    /** "Force direction in world coordinates,
        in units per frame added to each particle's velocity.
        Can be used for gravity." */
    Vector3 mForceDir; // 0x2d8
    float mDrag; // 0x2e8
    bool mBubble; // 0x2ec
    /** "Spawns particles on enter, so we start out with particles there" */
    bool mFastForward; // 0x2ed
    bool mNeedForward; // 0x2ee
    bool mRotate; // 0x2ef
    Vector2 mRPM; // 0x2f0
    float mRPMDrag; // 0x2f8
    bool mRandomDirection; // 0x2fc
    Vector2 mStartOffset; // 0x300
    Vector2 mEndOffset; // 0x308
    bool mAlignWithVelocity; // 0x310
    bool mStretchWithVelocity; // 0x311
    bool mConstantArea; // 0x312
    bool mPerspectiveStretch; // 0x313
    float mStretchScale; // 0x314
    /** "Ratio of screen height to width" */
    float mScreenAspect; // 0x318
    int mSubSamples; // 0x31c
    Transform mSubSampleXfm; // 0x320
    float mGrowRatio; // 0x360
    float mShrinkRatio; // 0x364
    float mMidColorRatio; // 0x368
    Hmx::Color mMidColorLow; // 0x36c
    Hmx::Color mMidColorHigh; // 0x37c
    /** "Add relative parent's momentum to each particle's initial speed.
        Fancy property must be true." */
    bool mBirthMomentum; // 0x38c
    /** "Amount of birth momentum to use. 0 is none, 1 is full amount,
        2 is twice the momentum, -1 means use the opposite of the momentum
        relative to the motion parent. Fancy property must be true." */
    float mBirthMomentumAmount; // 0x390
    std::vector<Burst> mBursts; // 0x394
    int mMaxBurst; // 0x3a0
    float mTimeTillBurst; // 0x3a4
    Vector2 mBurstInterval; // 0x3a8
    Vector2 mBurstPeak; // 0x3b0
    Vector2 mBurstLength; // 0x3b8
    int mExplicitParts; // 0x3c0
    float mElapsedTime; // 0x3c4
    /** "uses material texture as page tiles to animated through" */
    bool mAnimateUVs; // 0x3c8
    /** "animation loops to beginning if true" */
    bool mLoopUVAnim; // 0x3c9
    /** "particle animation starts at random location" */
    bool mRandomAnimStart; // 0x3ca
    /** "speed in seconds it takes to switch to next tile" */
    float mTileHoldTime; // 0x3cc
    /** "number of tiles across in specified texture" */
    int mNumTilesAcross; // 0x3d0
    /** "number of tiles down in specified texture" */
    int mNumTilesDown; // 0x3d4
    /** "total number of tiles used for animation" */
    int mNumTilesTotal; // 0x3d8
    /** "starting frame of animation" */
    int mStartingTile; // 0x3dc
    float mTotalTileTime; // 0x3e0
    float mInvTotalTileTime; // 0x3e4
    /** "Add point forces which attract or repel particles" */
    ObjVector<Attractor> mAttractors; // 0x3e8
};

extern ParticleCommonPool *gParticlePool;
extern PartOverride gNoPartOverride;
void InitParticleSystem();
