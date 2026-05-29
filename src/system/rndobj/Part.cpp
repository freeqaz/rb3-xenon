#include "rndobj/Part.h"
#include "math/Geo.h"
#include "math/Rand.h"
#include "math/Rot.h"
#include "math/Trig.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/System.h"
#include "os/Timer.h"
#include "rndobj/Anim.h"
#include "rndobj/Draw.h"
#include "rndobj/Mesh.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "rndobj/Mat.h"
#include "os/File.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include <cmath>

PartOverride gNoPartOverride;
ParticleCommonPool *gParticlePool;

namespace {
    int ParticlePoolSize() {
        return SystemConfig("rnd", "particlesys", "global_limit")->Int(1);
    }

    DataNode PrintParticlePoolSize(DataArray *) {
        MILO_LOG("Particle Pool Size:\n");
        if (gParticlePool) {
            int size = ParticlePoolSize();
            MILO_LOG(
                "   %d particles can be allocated, %.1f KB.\n",
                size,
                (float)((unsigned int)(size * 200) * 0.0009765625f)
            );
            MILO_LOG(
                "   %d particles active, %d is the high water mark.\n",
                gParticlePool->NumActiveParticles(),
                gParticlePool->HighWaterMark()
            );
            MILO_LOG(
                "   Adding 30%%, suggesting a particle global limit of %d (set in default.dta).\n",
                (int)(gParticlePool->HighWaterMark() * 1.3f)
            );
        }
        return 0;
    }
}

BinStream &operator<<(BinStream &bs, const RndParticle &p) {
    bs << p.pos << p.col << p.size;
    return bs;
}

BinStream &operator>>(BinStream &bs, RndParticle &p) {
    bs >> p.pos >> p.col >> p.size;
    return bs;
}

PartOverride::PartOverride()
    : mask(0), life(0), speed(0), deltaSize(0), startColor(0), midColor(0), endColor(0),
      pitch(0, 0), yaw(0, 0), mesh(0), box(Vector3(0, 0, 0), Vector3(0, 0, 0)) {}

void InitParticleSystem() {
    if (!gParticlePool) {
        gParticlePool = new ParticleCommonPool();
    }
    if (gParticlePool) {
        gParticlePool->InitPool();
    }
    DataRegisterFunc("print_particle_pool_size", PrintParticlePoolSize);
}

void ParticleCommonPool::InitPool() {
    int size = ParticlePoolSize();
    mPoolParticles = new RndFancyParticle[size];
    for (int i = 0; i < size - 1; i++) {
        mPoolParticles[i].prev = nullptr;
        mPoolParticles[i].next = &mPoolParticles[i + 1];
    }
    mPoolParticles[size - 1].prev = nullptr;
    mPoolParticles[size - 1].next = nullptr;
    mPoolFreeParticles = mPoolParticles;
}

RndParticle *ParticleCommonPool::AllocateParticle() {
    RndParticle *cur = mPoolFreeParticles;
    RndParticle *ret = nullptr;
    if (cur) {
        mPoolFreeParticles = cur->next;
        cur->prev = cur;
        mNumActiveParticles++;
        ret = cur;
        if (mNumActiveParticles > mHighWaterMark) {
            mHighWaterMark = mNumActiveParticles;
        }
    }
    return ret;
}

BEGIN_CUSTOM_PROPSYNC(Attractor)
    SYNC_PROP(attractor, o.mAttractor)
    SYNC_PROP(strength, o.mStrength)
END_CUSTOM_PROPSYNC

BinStream &operator<<(BinStream &bs, const Attractor &a) {
    a.Save(bs);
    return bs;
}

void Attractor::Save(BinStream &bs) const {
    bs << mAttractor;
    bs << mStrength;
}

void Attractor::Load(BinStreamRev &d) {
    d >> mAttractor;
    d >> mStrength;
}

BinStreamRev &operator>>(BinStreamRev &d, Attractor &a) {
    a.Load(d);
    return d;
}

RndParticleSys::RndParticleSys()
    : mType(kBasic), mMaxParticles(0), mPersistentParticles(nullptr),
      mFreeParticles(nullptr), mActiveParticles(nullptr), mNumActive(0), mEmitCount(0),
      mFrameDrive(0), mLastFrame(0), mDrawCount(0), mPauseOffscreen(0), mPausedTime(0),
      mBubblePeriod(10, 10), mBubbleSize(1, 1), mLife(100, 100), mBoxExtent1(0, 0, 0),
      mBoxExtent2(0, 0, 0), mSpeed(1, 1), mPitch(0, 0), mYaw(0, 0), mEmitRate(1, 1),
      mStartSize(gUnitsPerMeter / 4, gUnitsPerMeter / 4), mDeltaSize(0, 0),
      mStartColorLow(1, 1, 1), mStartColorHigh(1, 1, 1), mEndColorLow(1, 1, 1),
      mEndColorHigh(1, 1, 1), mMeshEmitter(this), mMat(this), mPreserveParticles(0),
      mMotionParent(this), mBounce(this), mForceDir(0, 0, 0), mDrag(0), mBubble(0),
      mFastForward(0), mNeedForward(0), mRotate(0), mRPM(0, 0), mRPMDrag(0),
      mRandomDirection(1), mStartOffset(0, 0), mEndOffset(0, 0), mAlignWithVelocity(0),
      mStretchWithVelocity(0), mConstantArea(0), mPerspectiveStretch(0), mStretchScale(1),
      mScreenAspect(1), mSubSamples(0), mGrowRatio(0), mShrinkRatio(1),
      mMidColorRatio(0.5), mMidColorLow(1, 1, 1), mMidColorHigh(1, 1, 1),
      mBirthMomentum(0), mBirthMomentumAmount(1), mMaxBurst(0), mTimeTillBurst(0),
      mBurstInterval(15, 35), mBurstPeak(4, 8), mBurstLength(20, 30), mExplicitParts(0),
      mElapsedTime(0), mAnimateUVs(0), mLoopUVAnim(1), mRandomAnimStart(0),
      mTileHoldTime(0), mNumTilesAcross(1), mNumTilesDown(1), mNumTilesTotal(1),
      mStartingTile(0), mTotalTileTime(1), mInvTotalTileTime(1), mAttractors(this) {
    SetRelativeMotion(0, this);
    SetSubSamples(0);
}

bool RndParticleSys::Replace(ObjRef *ref, Hmx::Object *obj) {
    if (ref == &mMotionParent) {
        RndTransformable *trans = dynamic_cast<RndTransformable *>(obj);
        SetRelativeMotion(mRelativeMotion, trans);
        return true;
    }
    return RndTransformable::Replace(ref, obj);
}

RndParticleSys::~RndParticleSys() {
    if (mPreserveParticles) {
        if (mPersistentParticles)
            delete[] mPersistentParticles;
    } else if (mActiveParticles) {
        for (RndParticle *p = mActiveParticles; p != nullptr; p = FreeParticle(p))
            ;
    }
}

BEGIN_HANDLERS(RndParticleSys)
    HANDLE_EXPR(hi_emit_rate, Max(mEmitRate.x, mEmitRate.y))
    HANDLE(set_start_color, OnSetStartColor)
    HANDLE(set_end_color, OnSetEndColor)
    HANDLE(set_start_color_int, OnSetStartColorInt)
    HANDLE(set_end_color_int, OnSetEndColorInt)
    HANDLE(set_emit_rate, OnSetEmitRate)
    HANDLE(set_burst_interval, OnSetBurstInterval)
    HANDLE(set_burst_peak, OnSetBurstPeak)
    HANDLE(set_burst_length, OnSetBurstLength)
    HANDLE(add_emit_rate, OnAddEmitRate)
    HANDLE(launch_part, OnExplicitPart)
    HANDLE(launch_parts, OnExplicitParts)
    HANDLE(set_life, OnSetLife)
    HANDLE(set_speed, OnSetSpeed)
    HANDLE(set_rotate, OnSetRotate)
    HANDLE(set_swing_arm, OnSetSwingArm)
    HANDLE(set_drag, OnSetDrag)
    HANDLE(set_alignment, OnSetAlignment)
    HANDLE(set_start_size, OnSetStartSize)
    HANDLE(set_mat, OnSetMat)
    HANDLE(set_pos, OnSetPos)
    HANDLE_ACTION(set_mesh, SetMesh(_msg->Obj<RndMesh>(2)))
    HANDLE(active_particles, OnActiveParticles)
    HANDLE_EXPR(max_particles, mMaxParticles)
    HANDLE_ACTION(
        set_relative_parent,
        SetRelativeMotion(mRelativeMotion, _msg->Obj<RndTransformable>(2))
    )
    HANDLE_ACTION(clear_all_particles, FreeAllParticles())
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

bool AngleVectorSync(Vector2 &vec, DataNode &_val, DataArray *_prop, int _i, PropOp _op) {
    auto _tmp2 = _prop->Size();
    if (_i == _tmp2)
        return true;
    else {
        Symbol sym = _prop->Sym(_i);
        static Symbol y("y");
        static Symbol x("x");
        float *coord = nullptr;
        if (sym == x) {
            coord = &vec.x;
            goto sync;
        } else if (sym == y) {
            coord = &vec.y;
            goto sync;
        } else
            return false;
    sync:
        if (_op == kPropSet)
            *coord = DegreesToRadians(_val.Float());
        else if (_op == kPropGet)
            _val = RadiansToDegrees(*coord);
        else
            return false;
    }
    return true;
}

BEGIN_PROPSYNCS(RndParticleSys)
    SYNC_PROP(mat, mMat)
    SYNC_PROP_SET(animate_uvs, mAnimateUVs, SetAnimatedUV(_val.Int()))
    SYNC_PROP(loop_uv_anim, mLoopUVAnim)
    SYNC_PROP(random_anim_start, mRandomAnimStart)
    SYNC_PROP_SET(tile_hold_time, mTileHoldTime, SetTileHoldTime(_val.Float()))
    SYNC_PROP_SET(num_tiles_across, mNumTilesAcross, mNumTilesAcross = Max(_val.Int(), 1))
    SYNC_PROP_SET(num_tiles_down, mNumTilesDown, mNumTilesDown = Max(_val.Int(), 1))
    SYNC_PROP_SET(num_tiles_total, mNumTilesTotal, SetNumTiles(_val.Int()))
    SYNC_PROP(starting_tile, mStartingTile)
    SYNC_PROP_SET(max_parts, mMaxParticles, SetPool(_val.Int(), mType))
    SYNC_PROP(emit_rate, mEmitRate)
    SYNC_PROP(screen_aspect, mScreenAspect)
    SYNC_PROP(life, mLife)
    SYNC_PROP(speed, mSpeed)
    SYNC_PROP(start_size, mStartSize)
    SYNC_PROP(delta_size, mDeltaSize)
    SYNC_PROP(force_dir, mForceDir)
    SYNC_PROP(bounce, mBounce)
    SYNC_PROP(start_color_low, mStartColorLow)
    SYNC_PROP(start_color_high, mStartColorHigh)
    SYNC_PROP(start_alpha_low, mStartColorLow.alpha)
    SYNC_PROP(start_alpha_high, mStartColorHigh.alpha)
    SYNC_PROP(end_color_low, mEndColorLow)
    SYNC_PROP(end_color_high, mEndColorHigh)
    SYNC_PROP(end_alpha_low, mEndColorLow.alpha)
    SYNC_PROP(end_alpha_high, mEndColorHigh.alpha)
    SYNC_PROP(preserve, mPreserveParticles)
    SYNC_PROP_SET(fancy, mType, SetPool(mMaxParticles, (Type)_val.Int()))
    SYNC_PROP_SET(grow_ratio, mGrowRatio, SetGrowRatio(_val.Float()))
    SYNC_PROP_SET(shrink_ratio, mShrinkRatio, SetShrinkRatio(_val.Float()))
    SYNC_PROP(drag, mDrag)
    SYNC_PROP(mid_color_ratio, mMidColorRatio)
    SYNC_PROP(mid_color_low, mMidColorLow)
    SYNC_PROP(mid_color_high, mMidColorHigh)
    SYNC_PROP(mid_alpha_low, mMidColorLow.alpha)
    SYNC_PROP(mid_alpha_high, mMidColorHigh.alpha)
    SYNC_PROP(bubble, mBubble)
    SYNC_PROP(bubble_period, mBubblePeriod)
    SYNC_PROP(bubble_size, mBubbleSize)
    SYNC_PROP(max_burst, mMaxBurst)
    SYNC_PROP(time_between, mBurstInterval)
    SYNC_PROP(peak_rate, mBurstPeak)
    SYNC_PROP(duration, mBurstLength)
    SYNC_PROP(spin, mRotate)
    SYNC_PROP(rpm, mRPM)
    SYNC_PROP(rpm_drag, mRPMDrag)
    SYNC_PROP(start_offset, mStartOffset)
    SYNC_PROP(end_offset, mEndOffset)
    SYNC_PROP(random_direction, mRandomDirection)
    SYNC_PROP(velocity_align, mAlignWithVelocity)
    SYNC_PROP(stretch_with_velocity, mStretchWithVelocity)
    SYNC_PROP(stretch_scale, mStretchScale)
    SYNC_PROP(constant_area, mConstantArea)
    SYNC_PROP(perspective, mPerspectiveStretch)
    SYNC_PROP_SET(mesh_emitter, mMeshEmitter.Ptr(), SetMesh(_val.Obj<RndMesh>()))
    SYNC_PROP(box_extent_1, mBoxExtent1)
    SYNC_PROP(box_extent_2, mBoxExtent2) {
        static Symbol _s("pitch");
        if (sym == _s) {
            AngleVectorSync(mPitch, _val, _prop, _i + 1, _op);
            return true;
        }
    }
    {
        static Symbol _s("yaw");
        if (sym == _s) {
            AngleVectorSync(mYaw, _val, _prop, _i + 1, _op);
            return true;
        }
    }
    SYNC_PROP_SET(
        motion_parent,
        mMotionParent.Ptr(),
        SetRelativeMotion(mRelativeMotion, _val.Obj<RndTransformable>())
    )
    SYNC_PROP_SET(
        relative_motion, mRelativeMotion, SetRelativeMotion(_val.Float(), mMotionParent)
    )
    SYNC_PROP_SET(subsamples, mSubSamples, SetSubSamples(_val.Int()))
    SYNC_PROP_SET(frame_drive, mFrameDrive, SetFrameDrive(_val.Int()))
    SYNC_PROP(pre_spawn, mFastForward)
    SYNC_PROP_SET(pause_offscreen, mPauseOffscreen, SetPauseOffscreen(_val.Int()))
    SYNC_PROP(attractors, mAttractors)
    SYNC_PROP(birth_momentum, mBirthMomentum)
    SYNC_PROP(birth_momentum_amount, mBirthMomentumAmount)
    SYNC_SUPERCLASS(RndAnimatable)
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(RndParticleSys)
    SAVE_REVS(0x29, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndPollable)
    SAVE_SUPERCLASS(RndAnimatable)
    SAVE_SUPERCLASS(RndTransformable)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mLife;
    bs << mScreenAspect;
    bs << mBoxExtent1;
    bs << mBoxExtent2;
    bs << mSpeed;
    bs << mPitch;
    bs << mYaw;
    bs << mEmitRate;
    bs << mMaxBurst;
    bs << mBurstInterval;
    bs << mBurstPeak;
    bs << mBurstLength;
    bs << mStartSize;
    bs << mDeltaSize;
    bs << mStartColorLow;
    bs << mStartColorHigh;
    bs << mEndColorLow;
    bs << mEndColorHigh;
    bs << mBounce;
    bs << mForceDir;
    bs << mMat;
    bs << mType;
    bs << mGrowRatio;
    bs << mShrinkRatio;
    bs << mMidColorRatio;
    bs << mMidColorLow;
    bs << mMidColorHigh;
    bs << mMaxParticles;
    bs << mBubblePeriod;
    bs << mBubbleSize;
    bs << mBubble;
    bs << mRotate;
    bs << mRPM;
    bs << mRPMDrag;
    bs << mRandomDirection;
    bs << mDrag;
    bs << mStartOffset;
    bs << mEndOffset;
    bs << mAlignWithVelocity;
    bs << mStretchWithVelocity;
    bs << mConstantArea;
    bs << mStretchScale;
    bs << mPerspectiveStretch;
    bs << mRelativeMotion;
    bs << mMotionParent;
    bs << mMeshEmitter;
    bs << mSubSamples;
    bs << mFrameDrive;
    bs << mPauseOffscreen;
    bs << mFastForward;
    bs << mAnimateUVs;
    bs << mTileHoldTime;
    bs << mNumTilesAcross;
    bs << mNumTilesDown;
    bs << mNumTilesTotal;
    bs << mStartingTile;
    bs << mLoopUVAnim;
    bs << mRandomAnimStart;
    bs << mAttractors;
    bs << mBirthMomentum;
    bs << mBirthMomentumAmount;
    bs << mPreserveParticles;
    mNeedForward = mFastForward;
    if (mPreserveParticles) {
        bs << mNumActive;
        for (RndParticle *p = mActiveParticles; p != nullptr; p = p->next) {
            bs << *p;
        }
    }
END_SAVES

BEGIN_COPYS(RndParticleSys)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndPollable)
    COPY_SUPERCLASS(RndAnimatable)
    COPY_SUPERCLASS(RndTransformable)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(RndParticleSys)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mPreserveParticles)
        if (mPreserveParticles) {
            SetPool(c->mMaxParticles, c->mType);
            for (RndParticle *p = c->mActiveParticles; p != nullptr; p = p->next) {
                RndParticle *alloced = AllocParticle();
                if (!alloced)
                    break;
                RndParticle *next = alloced->next;
                RndParticle *prev = alloced->prev;
                *alloced = *p;
                alloced->next = next;
                alloced->prev = prev;
            }
        }
        COPY_MEMBER(mNumActive)
        mLastFrame = GetFrame();
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mLife)
            COPY_MEMBER(mScreenAspect)
            COPY_MEMBER(mBoxExtent1)
            COPY_MEMBER(mBoxExtent2)
            COPY_MEMBER(mSpeed)
            COPY_MEMBER(mPitch)
            COPY_MEMBER(mYaw)
            COPY_MEMBER(mEmitRate)
            COPY_MEMBER(mMaxBurst)
            COPY_MEMBER(mBurstInterval)
            COPY_MEMBER(mBurstPeak)
            COPY_MEMBER(mBurstLength)
            COPY_MEMBER(mStartSize)
            COPY_MEMBER(mDeltaSize)
            COPY_MEMBER(mStartColorLow)
            COPY_MEMBER(mStartColorHigh)
            COPY_MEMBER(mEndColorLow)
            COPY_MEMBER(mEndColorHigh)
            COPY_MEMBER(mBounce)
            COPY_MEMBER(mForceDir)
            COPY_MEMBER(mMat)
            COPY_MEMBER(mBubblePeriod)
            COPY_MEMBER(mBubbleSize)
            COPY_MEMBER(mBubble)
            COPY_MEMBER(mRotate)
            COPY_MEMBER(mRPM)
            COPY_MEMBER(mRPMDrag)
            COPY_MEMBER(mRandomDirection)
            COPY_MEMBER(mDrag)
            COPY_MEMBER(mStartOffset)
            COPY_MEMBER(mEndOffset)
            COPY_MEMBER(mAlignWithVelocity)
            COPY_MEMBER(mStretchWithVelocity)
            COPY_MEMBER(mConstantArea)
            COPY_MEMBER(mPerspectiveStretch)
            COPY_MEMBER(mStretchScale)
            COPY_MEMBER(mFastForward)
            mNeedForward = mFastForward;
            COPY_MEMBER(mGrowRatio)
            COPY_MEMBER(mShrinkRatio)
            COPY_MEMBER(mMidColorRatio)
            COPY_MEMBER(mMidColorLow)
            COPY_MEMBER(mMidColorHigh)
            COPY_MEMBER(mMeshEmitter)
            COPY_MEMBER(mFrameDrive)
            COPY_MEMBER(mPauseOffscreen)
            mElapsedTime = mPausedTime = 0;
            COPY_MEMBER(mAnimateUVs)
            COPY_MEMBER(mLoopUVAnim)
            COPY_MEMBER(mRandomAnimStart)
            COPY_MEMBER(mTileHoldTime)
            COPY_MEMBER(mNumTilesAcross)
            COPY_MEMBER(mNumTilesDown)
            COPY_MEMBER(mNumTilesTotal)
            COPY_MEMBER(mStartingTile)
            COPY_MEMBER(mTotalTileTime)
            COPY_MEMBER(mInvTotalTileTime)
            COPY_MEMBER(mBirthMomentum)
            COPY_MEMBER(mBirthMomentumAmount)
            mAttractors.clear();
            for (unsigned int i = 0; i != c->mAttractors.size(); i++) {
                mAttractors.push_back(Attractor(c->mAttractors[i], this));
            }
            if (!mPreserveParticles) {
                SetPool(c->mMaxParticles, c->mType);
            }
            RndTransformable *parent =
                c->mMotionParent.Ptr() ? c->mMotionParent.Ptr() : this;
            SetRelativeMotion(c->mRelativeMotion, parent);
            SetSubSamples(c->mSubSamples);
        }
    END_COPYING_MEMBERS
END_COPYS

void RndParticleSys::SetFrame(float frame, float blend) {
    RndAnimatable::SetFrame(frame, blend);
    if (mFrameDrive) {
        UpdateParticles();
        mLastFrame = frame;
        mPausedTime = 0;
    }
}

float RndParticleSys::EndFrame() {
    if (mFrameDrive) {
        return Max(mLife.x, mLife.y);
    } else
        return 0;
}

void RndParticleSys::Enter() {
    mNeedForward = mFastForward;
    mElapsedTime = 0;
    RndPollable::Enter();
}

void RndParticleSys::Poll() {
    if (!mFrameDrive) {
        mElapsedTime += (GetRate() == k30_fps_ui ? TheTaskMgr.DeltaUISeconds()
                                                 : TheTaskMgr.DeltaSeconds())
            * 30.0f;
        if (mDrawCount == 0) {
            if (Showing()
                && (mActiveParticles || mExplicitParts || mEmitRate.x > 0
                    || mEmitRate.y > 0 || mMaxBurst > 0)) {
                UpdateRelativeXfm();
                UpdateParticles();
            } else {
                mLastFrame = CalcFrame();
            }
        } else if (mActiveParticles && mDrawCount % 60 == 0 && !mPreserveParticles) {
            float currentFrame = CalcFrame();
            RndParticle *p = mActiveParticles;
            while (p) {
                bool dead = currentFrame >= p->deathFrame || currentFrame < p->birthFrame;
                if (dead) {
                    p = FreeParticle(p);
                } else {
                    p = p->next;
                }
            }
        }
        if (mSubSamples > 0 && Dirty()) {
            MakeLocToRel(mSubSampleXfm);
        }
        mDrawCount++;
    }
}

void RndParticleSys::UpdateSphere() {
    Sphere s;
    MakeWorldSphere(s, true);
    Transform tf;
    FastInvert(WorldXfm(), tf);
    Multiply(s, tf, s);
    SetSphere(s);
}

void RndParticleSys::DrawShowing() {
    if (mFrameDrive) {
        UpdateRelativeXfm();
    } else {
        if (mDrawCount > 1) {
            UpdateRelativeXfm();
            UpdateParticles();
        } else if (mRelativeMotion == 1) {
            UpdateRelativeXfm();
        }
        mDrawCount = 0;
    }
#ifdef HX_NATIVE
    extern void DrawParticlesBillboard(RndParticleSys*);
    DrawParticlesBillboard(this);
#endif
}

void RndParticleSys::Mats(std::list<RndMat *> &mats, bool) {
    if (mMat) {
        MatShaderOptions shaderOpts = GetDefaultMatShaderOpts(this, mMat);
        mMat->SetShaderOpts(shaderOpts);
        mats.push_back(mMat);
    }
}

INIT_REVS(0x29, 0)

BEGIN_LOADS(RndParticleSys)
    LOAD_REVS(bs)
    volatile int rev = d.rev;
    ASSERT_REVS(0x29, 0)
    BinStream *stream = &d.stream;
    if (rev > 0x16)
        LOAD_SUPERCLASS(Hmx::Object);
    if (rev > 0x1B)
        LOAD_SUPERCLASS(RndPollable);
    if (rev > 0) {
        LOAD_SUPERCLASS(RndAnimatable)
        LOAD_SUPERCLASS(RndTransformable)
        LOAD_SUPERCLASS(RndDrawable)
    }
    (*stream) >> (Key<float> &)mLife;
    if (rev > 0x23)
        (*stream) >> mScreenAspect;
    (*stream) >> mBoxExtent1;
    (*stream) >> mBoxExtent2;
    (*stream) >> (Key<float> &)mSpeed;
    (*stream) >> (Key<float> &)mPitch;
    (*stream) >> (Key<float> &)mYaw;
    (*stream) >> (Key<float> &)mEmitRate;
    if (rev > 0x20) {
        (*stream) >> mMaxBurst;
        (*stream) >> (Key<float> &)mBurstInterval;
        (*stream) >> (Key<float> &)mBurstPeak;
        (*stream) >> (Key<float> &)mBurstLength;
    }
    (*stream) >> (Key<float> &)mStartSize;
    if (rev > 0xF)
        (*stream) >> (Key<float> &)mDeltaSize;
    (*stream) >> mStartColorLow;
    (*stream) >> mStartColorHigh;
    (*stream) >> mEndColorLow;
    (*stream) >> mEndColorHigh;
    if (rev > 0x19)
        (*stream) >> mBounce;
    else if (rev > 1) {
        bool ba7;
        Plane p150;
        d >> ba7;
        if (rev > 0xB) {
            (*stream) >> (Hmx::Color &)p150;
        } else {
            Vector3 v1;
            float f1, f2, f3;
            (*stream) >> v1;
            (*stream) >> f1 >> f2 >> f3;
            p150.Set(f1, f2, f3, -(v1.x * f1 + v1.y * f2 + v1.z * f3));
        }
        if (ba7) {
            bool old = TheLoadMgr.EditMode();
            TheLoadMgr.SetEditMode(true);
            char *base = (char *)FileGetBase(Name());
            mBounce = Dir()->New<RndTransformable>(
                MakeString("%s_bounce.trans", base)
            );
            TheLoadMgr.SetEditMode(old);
            Transform tf140;
            float a = p150.a;
            float b = p150.b;
            float c = p150.c;
            float d = p150.d;
            tf140.m.x.x = c;
            tf140.m.x.y = 0.0f;
            tf140.m.x.z = -a;
            tf140.m.z.x = a;
            tf140.m.z.y = b;
            tf140.m.z.z = c;
            tf140.m.y.x = b * tf140.m.x.z - c * tf140.m.x.y;
            tf140.m.y.y = c * c - tf140.m.x.z * a;
            tf140.m.y.z = a * tf140.m.x.y - b * c;
            float inv = -(d / (a * a + (b * b + c * c)));
            tf140.v.x = a * inv;
            tf140.v.y = b * inv;
            tf140.v.z = c * inv;
            Normalize(tf140.m.x, tf140.m.x);
            Normalize(tf140.m.y, tf140.m.y);
            mBounce->SetWorldXfm(tf140);
        }
    } else {
        std::list<Plane> planes;
        d >> planes;
    }
    (*stream) >> mForceDir;
    (*stream) >> mMat;
    if (rev > 0x17 && rev < 0x19) {
        char buf[0x80];
        (*stream).ReadString(buf, 0x80);
        if (!mMat && buf[0] != '\0') {
            mMat = LookupOrCreateMat(buf, Dir());
        }
    }
    if (rev > 0x11) {
        (*stream) >> (int &)mType >> mGrowRatio >> mShrinkRatio >> mMidColorRatio;
        (*stream) >> mMidColorLow >> mMidColorHigh;
    } else if (rev < 0xD) {
        int i94;
        (*stream) >> i94;
    }
    (*stream) >> mMaxParticles;
    if (rev > 2) {
        if (rev < 7) {
            int i98;
            (*stream) >> i98;
        } else if (rev < 0xD) {
            int i9c;
            (*stream) >> i9c;
        }
    }
    if (rev > 3) {
        (*stream) >> (Key<float> &)mBubblePeriod >> (Key<float> &)mBubbleSize;
        (*stream) >> mBubble;
    }
    if (rev > 0x1D) {
        d >> mRotate;
        (*stream) >> (Key<float> &)mRPM >> mRPMDrag;
        if (rev > 0x24) {
            d >> mRandomDirection;
        }
        (*stream) >> mDrag;
    }
    if (rev > 0x1F) {
        (*stream) >> (Key<float> &)mStartOffset >> (Key<float> &)mEndOffset;
        d >> mAlignWithVelocity;
        d >> mStretchWithVelocity;
        d >> mConstantArea;
        (*stream) >> mStretchScale;
    }
    if (rev > 0x21) {
        d >> mPerspectiveStretch;
    }
    if (rev > 4 && rev < 15) {
        bool baf;
        d >> baf;
        int u1 = 0;
        if (baf)
            u1 = 2;
        if (mMat)
            mMat->SetZMode((ZMode)u1);
    }
    if (rev > 5 && rev < 17) {
        String str;
        (*stream) >> str;
    }
    if (rev == 8) {
        bool b1b0;
        d >> b1b0;
    }
    if (rev > 0xC && rev < 0xE) {
        int i1a0;
        (*stream) >> i1a0;
    }
    if (rev > 0x13) {
        (*stream) >> mRelativeMotion;
    } else if (rev > 0xC) {
        bool i1b1;
        d >> i1b1;
        mRelativeMotion = i1b1;
    }
    if (rev > 0x1A)
        (*stream) >> mMotionParent;
    SetRelativeMotion(mRelativeMotion, mMotionParent);
    if (rev > 0x12)
        (*stream) >> mMeshEmitter;
    if (rev > 0x1E || rev == 0x15)
        (*stream) >> mSubSamples;
    SetSubSamples(mSubSamples);
    if (rev > 0x1B) {
        d >> mFrameDrive;
    } else
        mFrameDrive = true;
    if (rev > 0x22) {
        d >> mPauseOffscreen;
    } else
        mPauseOffscreen = false;
    if (rev > 0x1C) {
        d >> mFastForward;
    } else
        mFastForward = false;
    mNeedForward = mFastForward;

    if (rev > 0x26) {
        d >> mAnimateUVs;
        (*stream) >> mTileHoldTime;
        (*stream) >> mNumTilesAcross;
        (*stream) >> mNumTilesDown;
        (*stream) >> mNumTilesTotal;
        (*stream) >> mStartingTile;
        d >> mLoopUVAnim;
        d >> mRandomAnimStart;
        mTotalTileTime = (float)mNumTilesTotal * mTileHoldTime;
        if (mTotalTileTime - 0.0001f < 0.0f) {
            mTotalTileTime = 0.0001f;
        }
        mInvTotalTileTime = 1.0f / mTotalTileTime;
    }
    if (rev > 0x27) {
        d >> mAttractors;
    }
    if (rev > 0x28) {
        d >> mBirthMomentum;
        (*stream) >> mBirthMomentumAmount;
    }

    if (rev <= 0xA || (d >> mPreserveParticles, !mPreserveParticles)) {
        SetPool(mMaxParticles, mType);
    } else {
        int count;
        RndParticle tempParticle;
        (*stream) >> count;
        SetPool(mMaxParticles, mType);
        for (int i = 0; i < count; i++) {
            RndParticle *p = AllocParticle();
            if (p) {
                p->angle = 0;
                p->swingArm = 0;
                p->vel.x = 0;
                p->vel.y = 0;
                p->vel.z = 0;
                p->vel.w = 0;
            } else {
                MILO_NOTIFY_ONCE(
                    "Unable to allocate all particles for %s\n",
                    (char *)PathName(this)
                );
                p = &tempParticle;
            }
            (*stream) >> *p;
        }
    }

    mPausedTime = 0;
    mLastFrame = GetFrame();
END_LOADS

void RndParticleSys::SetPool(int max, Type ty) {
    if (mPreserveParticles) {
        SetPersistentPool(max, ty);
    } else {
        for (RndParticle *p = mActiveParticles; p != nullptr; p = FreeParticle(p))
            ;
        mType = ty;
        mMaxParticles = max;
        int limit = SystemConfig()
            ? SystemConfig("rnd", "particlesys", "local_limit")->Int(1)
            : mMaxParticles;
        if (mMaxParticles > limit) {
            MILO_NOTIFY(
                "Max particles for %s is too high (%d > %d). The max number of particles has been reset to %d.\n",
                PathName(this),
                mMaxParticles,
                limit,
                limit
            );
            mMaxParticles = limit;
        }
        mActiveParticles = nullptr;
        mNumActive = 0;
        mEmitCount = 0;
    }
}

void RndParticleSys::SetPersistentPool(int max, Type ty) {
    delete[] mPersistentParticles;
    mMaxParticles = max;
    mType = ty;

    // Allocate particle pool based on type
    if (max != 0) {
        if (ty == kFancy) {
            mPersistentParticles = new RndFancyParticle[max];
            RndFancyParticle *fp = (RndFancyParticle *)mPersistentParticles;
            // Build linked list: each particle points to the next
            for (int i = 0; i != max; i++) {
                (fp++)->next = fp;
            }
            fp->next = nullptr;
        } else {
            mPersistentParticles = new RndParticle[max];
            RndParticle *p = (RndParticle *)mPersistentParticles;
            // Build linked list: each particle points to the next
            for (int i = 0; i != max; i++) {
                (p++)->next = p;
            }
            p->next = nullptr;
        }
    } else {
        mPersistentParticles = nullptr;
    }

    // Initialize free list and state
    mActiveParticles = nullptr;
    mNumActive = 0;
    mFreeParticles = mPersistentParticles;
    mEmitCount = 0;
}

void RndParticleSys::SetTileHoldTime(float f1) {
    mTileHoldTime = f1;
    mTotalTileTime = mNumTilesTotal * mTileHoldTime;
    mTotalTileTime = Max(mTotalTileTime, 0.0001f);
    mInvTotalTileTime = 1.0f / mTotalTileTime;
}

void RndParticleSys::SetNumTiles(int num) {
    mNumTilesTotal = Max(num, 1);
    mTotalTileTime = mNumTilesTotal * mTileHoldTime;
    mTotalTileTime = Max(mTotalTileTime, 0.0001f);
    mInvTotalTileTime = 1.0f / mTotalTileTime;
}

void RndParticleSys::SetGrowRatio(float f) {
    if (f >= 0 && f <= mGrowRatio)
        mGrowRatio = f;
}

void RndParticleSys::SetShrinkRatio(float f) {
    if (f >= mGrowRatio && f <= 1.0f)
        mShrinkRatio = f;
}

void RndParticleSys::SetFrameDrive(bool b) {
    mFrameDrive = b;
    if (mFrameDrive) {
        mLastFrame = GetFrame();
    } else
        mDrawCount = 0;
    mPausedTime = 0;
}

void RndParticleSys::SetPauseOffscreen(bool b) {
    mPauseOffscreen = b;
    mPausedTime = 0;
}

void RndParticleSys::SetAnimatedUV(bool b) {
    if (mAnimateUVs != b) {
        SetPool(mMaxParticles, mType);
    }
    mAnimateUVs = b;
}

void RndParticleSys::SetMesh(RndMesh *mesh) {
    if (mesh) {
        SetTransParent(mesh, false);
        SetTransConstraint(RndTransformable::kConstraintParentWorld, 0, false);
        if (!mesh->GetKeepMeshData()) {
            MILO_NOTIFY(
                "keep_mesh_data should be checked for %s.  It's the mesh emitter for %s.\n",
                PathName(mesh),
                PathName(this)
            );
        }
    } else if (mMeshEmitter) {
        SetTransParent(0, false);
        SetTransConstraint(RndTransformable::kConstraintNone, 0, false);
    }
    mMeshEmitter = mesh;
}

RndParticle *RndParticleSys::AllocParticle() {
    RndParticle *p;
    if (mPreserveParticles) {
        p = mFreeParticles;
        if (!mFreeParticles)
            return nullptr;
        mFreeParticles = p->next;
    } else {
        p = gParticlePool->AllocateParticle();
        if (!p) {
            int size = ParticlePoolSize();
            MILO_NOTIFY_ONCE(
                "Can't allocate more particles for %s.\nGlobal max particle limit reached (%d).\n",
                PathName(this),
                size
            )
            return nullptr;
        }
    }
    p->prev = p;
    if (mActiveParticles) {
        mActiveParticles->prev = p;
    }
    p->next = mActiveParticles;
    mActiveParticles = p;
    mNumActive++;
    return p;
}

RndParticle *ParticleCommonPool::FreeParticle(RndParticle *p) {
    if (!p)
        return nullptr;
    else {
        RndParticle *ret = p->next;
        p->next = mPoolFreeParticles;
        p->prev = nullptr;
        mPoolFreeParticles = p;
        mNumActiveParticles--;
        return ret;
    }
}

RndParticle *RndParticleSys::FreeParticle(RndParticle *p) {
    if (!p)
        return nullptr;
    else {
        if (p == mActiveParticles) {
            mActiveParticles = p->next;
        } else {
            p->prev->next = p->next;
        }
        if (p->next) {
            p->next->prev = p->prev;
        }
        if (!p->prev) {
            MILO_FAIL("Already deallocated particle");
        }
        p->prev = nullptr;
        RndParticle *ret = nullptr;
        if (mPreserveParticles) {
            ret = p->next;
            p->next = mFreeParticles;
            mFreeParticles = p;
        } else {
            ret = gParticlePool->FreeParticle(p);
        }
        mNumActive--;
        return ret;
    }
}

void RndParticleSys::MakeLocToRel(Transform &tf) {
    if (mRelativeMotion == 1) {
        if (mMotionParent == this) {
            tf.Reset();
            return;
        }
    }
    Transpose(mRelativeXfm, tf);
    Multiply(WorldXfm(), tf, tf);
}

void RndParticleSys::SetSubSamples(int num) {
    mSubSamples = num;
    Transpose(mRelativeXfm, mSubSampleXfm);
    Multiply(WorldXfm(), mSubSampleXfm, mSubSampleXfm);
}

void RndParticleSys::UpdateRelativeXfm() {
#ifdef HX_NATIVE
    if (!mMotionParent)
        return;
#endif
    if (mRelativeMotion == 1) {
        mRelativeXfm = mMotionParent->WorldXfm();
    } else if (mRelativeMotion) {
        const Transform &worldXfm = mMotionParent->WorldXfm();
        Invert(mLastWorldXfm.m, mLastWorldXfm.m);
        Multiply(mLastWorldXfm.m, worldXfm.m, mLastWorldXfm.m);
        Hmx::Quat q28(0, 0, 0, 1);
        FastInterp(q28, Hmx::Quat(mLastWorldXfm.m), mRelativeMotion, q28);
        MakeRotMatrix(q28, mLastWorldXfm.m);
        Subtract(mRelativeXfm.v, mLastWorldXfm.v, mRelativeXfm.v);
        Multiply(mRelativeXfm, mLastWorldXfm.m, mRelativeXfm);
        Normalize(mRelativeXfm.m, mRelativeXfm.m);
        Interp(mLastWorldXfm.v, worldXfm.v, mRelativeMotion, mLastWorldXfm.v);
        Add(mRelativeXfm.v, mLastWorldXfm.v, mRelativeXfm.v);
    }
    Subtract(mMotionParent->WorldXfm().v, mLastWorldXfm.v, mMotionParentDelta);
    mLastWorldXfm = mMotionParent->WorldXfm();
}

// TODO: 69.3% match (AT_LIMIT). 2340-byte function, implemented from 0.1% stub.
//
// Remaining diff breakdown (614 instructions total):
//   - r29<->r30 register swap: 117 instructions. Target uses r30 for 'this',
//     our compiler picks r29. Unfixable compiler register allocation choice.
//   - 111 deletes: target has dead stores to stack slots 0x60/0x64 where it
//     caches intermediate pointers (addi rX, rBase, offset; stw rX, 0x60, r31).
//     Our compiler optimizes these away. Also target caches &p->pos in r25 and
//     &p->vel in r26 as dedicated pointer registers throughout the inner loop.
//   - 2 diff_ops remaining:
//     (1) idx 126: bounce WorldXfm call uses bl (call) in target vs b (branch)
//         in ours. Target reuses a shared branch point for the two WorldXfm calls.
//     (2) idx 340: attractor strength==0.015625 check uses beq (branch-if-equal
//         to special case) in target vs bne (skip special case) in ours. Target
//         also has dead code after (li 0; clrlwi. 0; beq - always-taken branch),
//         suggesting original code had a boolean variable for the condition.
//   - fmadds vs fmuls+fadds: our compiler fuses multiply-add in position update,
//     bounce reflection (fnmsubs vs fmuls+fsubs), and basic particle color/size.
//     Target uses separate instructions. Hard to prevent without volatile temps.
//   - Stack frame: target 0x1c0, ours larger. Target saves from r14 (savegprlr_14),
//     ours from r17 (3 fewer callee-saved GPRs).
//
// Potential improvements to investigate:
//   - Restructure bounce WorldXfm calls to match target's shared-branch pattern
//   - Try a bool variable for the attractor strength check to match dead code
//   - Volatile or separate-statement tricks to prevent fmadds fusion
//   - Declaration order changes to shift r14-r16 register assignment
//
// RndFancyParticle offset note: header comments are wrong by -8 bytes.
// RndParticle is 0x68 bytes (not 0x60), so RndFancyParticle fields start at 0x68.
// E.g. growFrame comment says 0x60 but actual compiled offset is 0x68,
// midcolFrame comment says 0x80 but actual is 0x88, etc.
void RndParticleSys::MoveParticles(float dt, float frameSpan) {
    START_AUTO_TIMER("psysmove");

    if (mActiveParticles == NULL || frameSpan == 0.0f)
        return;

    float dragFactor;

    float oneOverThirty = 1.0f / 30.0f;
    if (mDrag > 0.0f) {
        float powResult = std::pow(1.0f - mDrag, frameSpan * oneOverThirty);
        dragFactor = powResult;
    } else {
        dragFactor = 1.0f;
    }

    float rpmDragFactor;
    if (mRotate && mRPMDrag > 0.0f) {
        rpmDragFactor = std::pow(1.0f - mRPMDrag, frameSpan * oneOverThirty);
    } else {
        rpmDragFactor = 1.0f;
    }

    // Force direction scaled by frameSpan
    float forceX_dt = mForceDir.x * frameSpan;
    float forceZ_dt = mForceDir.z * frameSpan;
    float forceY_dt = mForceDir.y * frameSpan;

    // Individual matrix components needed for fmadds sequence in pre-computation.
    // Removing these and using mRelativeXfm.m.x.x etc. directly drops match by ~0.4%.
    float m_yx = mRelativeXfm.m.y.x;
    bool isRotate = mRotate;
    float m_yy = mRelativeXfm.m.y.y;
    bool isFancy = (mType == kFancy);
    float m_yz = mRelativeXfm.m.y.z;
    float m_xx = mRelativeXfm.m.x.x;
    float m_xz = mRelativeXfm.m.x.z;
    float m_xy = mRelativeXfm.m.x.y;
    bool isBubble = mBubble;

    // Pre-compute all 3 transformed force rows (target does this before bounce check).
    // Row 2 uses mRelativeXfm.m.z directly (not cached into locals) to match target.
    float relForceRow0 =
        (m_xx * forceX_dt + (m_xy * forceY_dt + m_xz * forceZ_dt));
    float relForceRow1 =
        m_yx * forceX_dt + m_yy * forceY_dt + m_yz * forceZ_dt;
    float relForceRow2 =
        mRelativeXfm.m.z.x * forceX_dt + mRelativeXfm.m.z.y * forceY_dt
        + mRelativeXfm.m.z.z * forceZ_dt;

    // TODO: target calls WorldXfm twice via a shared branch point (bl+b pattern).
    // Our compiler generates a different call sequence (diff_op at idx 126).
    float planeNx, planeNy, planeNz, planeD;
    if (mBounce != NULL) {
        const Transform &bxf = mBounce->WorldXfm();
        planeNy = bxf.m.z.y;
        planeNz = bxf.m.z.z;
        planeNx = bxf.m.z.x;
        const Transform &bxf2 = mBounce->WorldXfm();
        planeD = -(bxf2.v.x * planeNx + bxf2.v.z * planeNz + bxf2.v.y * planeNy);
    }

    RndParticle *p = mActiveParticles;
    int endTile = mNumTilesTotal + mStartingTile;

    if (p != NULL) {
        float sixf = 6.0f;
        float halfPi = 1.5707963705062866f;
        float epsilon = 1.1920928955078125e-07f;
        float magicStrength = 0.015625f;
        float two = 2.0f;

        do {
            bool dead;
            if (dt >= p->deathFrame || dt < p->birthFrame) {
                dead = true;
            } else {
                dead = false;
            }

            if (dead) {
                p = FreeParticle(p);
            } else {
                // UV tile animation
                if (mAnimateUVs) {
                    float tileTime = p->mTileTime + frameSpan;
                    p->mTileTime = tileTime;
                    if (p->mCurrentTileIndex < endTile && tileTime > mTileHoldTime) {
                        int newTile = p->mCurrentTileIndex + 1;
                        p->mCurrentTileIndex = newTile;
                        if (newTile >= endTile) {
                            if (mLoopUVAnim) {
                                p->mCurrentTileIndex = mStartingTile;
                            } else {
                                p->mCurrentTileIndex = endTile - 1;
                            }
                        }
                        p->mTileTime = std::fmod(tileTime, mTileHoldTime);
                    }
                }

                // Birth momentum (fancy only)
                if (isFancy && mBirthMomentum) {
                    RndFancyParticle *fp = (RndFancyParticle *)p;
                    float momentumScale = mBirthMomentumAmount * frameSpan * oneOverThirty;
                    p->pos.x += momentumScale * fp->mBirthVelocityX;
                    p->pos.z += fp->mBirthVelocityZ * momentumScale;
                    p->pos.y += fp->mBirthVelocityY * momentumScale;
                }

                // Position integration
                // TODO: target uses fmuls+fadds (separate multiply then add) here,
                // our compiler generates fmadds (fused multiply-add). This accounts
                // for ~6 instruction differences in the inner loop.
                p->pos.x += frameSpan * p->vel.x;
                p->pos.y += p->vel.y * frameSpan;
                p->pos.z += frameSpan * p->vel.z;

                // Bounce plane reflection
                if (mBounce != NULL) {
                    float dist = planeNx * p->pos.x + planeNy * p->pos.y + planeNz * p->pos.z
                        + planeD;
                    if (dist < 0.0f) {
                        float velDotN =
                            planeNy * p->vel.y + p->vel.x * planeNx + planeNz * p->vel.z;
                        if (velDotN < 0.0f) {
                            // TODO: target uses fmuls+fsubs (separate), our compiler
                            // generates fnmsubs (fused negate-multiply-subtract).
                            float reflect = velDotN * two;
                            p->vel.z -= planeNz * reflect;
                            p->vel.x -= reflect * planeNx;
                            p->vel.y -= planeNy * reflect;
                        }
                    }
                }

                // Attractors
                unsigned int numAttractors = mAttractors.size();
                for (unsigned int i = 0; i < numAttractors; i++) {
                    Attractor &a = mAttractors[i];
                    if (a.mAttractor != NULL) {
                        const Transform &axf = a.mAttractor->WorldXfm();
                        float dz = axf.v.z - p->pos.z;
                        float dy = axf.v.y - p->pos.y;
                        float strength = a.mStrength;
                        float dx = axf.v.x - p->pos.x;

                        // TODO: target uses beq (to special case) + dead code after,
                        // ours uses bne (skip special case). diff_op at idx 340.
                        // Target dead code: li r11,0; clrlwi. r11,r11,24; beq (always taken).
                        // Suggests original may have used a bool for this condition.
                        if (strength == magicStrength) {
                            dz = 0.0f;
                            auto _tmp0 = a.mAttractor.Owner();
                            RndParticleSys *ps =
                                dynamic_cast<RndParticleSys *>(_tmp0);
                            if (ps != NULL) {
                                const Transform &t1xf = a.mAttractor->WorldXfm();
                                const Transform &t2xf = ps->WorldXfm();
                                float relY = t2xf.v.y - t1xf.v.y;
                                float relX = t2xf.v.x - t1xf.v.x;
                                strength *= (relX * relX + relY * relY) + epsilon;
                            }
                        }

                        float distSq =
                            dy * dy + (dx * dx + dz * dz) + epsilon;
                        float scale = (strength * frameSpan) / distSq;
                        p->vel.z += scale * dz;
                        p->vel.x += scale * dx;
                        p->vel.y += scale * dy;
                    }
                }

                p->vel.y += relForceRow1;
                p->vel.x += relForceRow0;
                p->vel.z += relForceRow2;

                if (isFancy) {
                    p->vel.y *= dragFactor;
                    p->vel.z *= dragFactor;
                    p->vel.x *= dragFactor;

                    RndFancyParticle *fp = (RndFancyParticle *)p;

                    // Bubble oscillation effect
                    if (isBubble) {
                        float sinVal =
                            FastSin(fp->RPF * dt + fp->swingArmVel + halfPi);
                        float bubbleScale = fp->RPF * sinVal * frameSpan;
                        p->pos.x += fp->bubbleDir.z * bubbleScale;
                        p->pos.y += fp->bubbleDir.w * bubbleScale;
                        p->pos.z += fp->bubbleFreq * bubbleScale;
                    }

                    // RPM rotation and swing arm
                    if (isRotate) {
                        float rpmVel = fp->mRPMVelocity;
                        p->angle += rpmVel * frameSpan;
                        fp->mRPMVelocity = rpmVel * rpmDragFactor;
                        p->swingArm += fp->mPitchAngularVel * frameSpan;
                    }

                    // Fancy color: 2-phase Hermite-like blend (before/after midcolFrame).
                    // Blend formula: colorScale = (1-t)*t*frameSpan*6 where t is normalized
                    // time within the current phase. Phase 1 uses midcolVel, phase 2 uses colVel.
                    float colorScale;
                    float cr, cg, cb, ca;
                    if (dt < fp->midcolFrame) {
                        float t = (dt - p->birthFrame) * p->vel.w;
                        colorScale = (1.0f - t) * t * frameSpan * sixf;
                        ca = fp->midcolVel.alpha * colorScale;
                        cb = fp->midcolVel.blue * colorScale;
                        cg = fp->midcolVel.green * colorScale;
                        cr = colorScale * fp->midcolVel.red;
                    } else {
                        float t = (dt - fp->midcolFrame) * fp->bubblePhase;
                        colorScale = (1.0f - t) * t * frameSpan * sixf;
                        ca = p->colVel.alpha * colorScale;
                        cb = p->colVel.blue * colorScale;
                        cg = p->colVel.green * colorScale;
                        cr = p->colVel.red * colorScale;
                    }

                    // Clamp color channels to [0, 1] using fneg+fsel pattern
                    float newR = cr + p->col.red;
                    float newA = ca + p->col.alpha;
                    float newB = cb + p->col.blue;
                    float newG = cg + p->col.green;

                    newR = (-newR >= 0.0f) ? 0.0f : newR;
                    newA = (-newA >= 0.0f) ? 0.0f : newA;
                    newB = (-newB >= 0.0f) ? 0.0f : newB;
                    newG = (-newG >= 0.0f) ? 0.0f : newG;

                    p->col.red = (newR - 1.0f >= 0.0f) ? 1.0f : newR;
                    p->col.alpha = (newA - 1.0f >= 0.0f) ? 1.0f : newA;
                    p->col.blue = (newB - 1.0f >= 0.0f) ? 1.0f : newB;
                    p->col.green = (newG - 1.0f >= 0.0f) ? 1.0f : newG;

                    // Fancy size: 3-phase (grow / sustain / shrink)
                    float sizeVelRate, timeSince, invDuration;
                    if (dt < fp->growFrame) {
                        invDuration = fp->beginGrow;
                        timeSince = dt - p->birthFrame;
                        sizeVelRate = fp->growVel;
                    } else if (dt < fp->shrinkFrame) {
                        invDuration = fp->midGrow;
                        timeSince = dt - fp->growFrame;
                        sizeVelRate = p->sizeVel;
                    } else {
                        timeSince = dt - fp->shrinkFrame;
                        invDuration = fp->endGrow;
                        sizeVelRate = fp->shrinkVel;
                    }
                    float st = timeSince * invDuration;
                    p->size +=
                        sizeVelRate * ((1.0f - st) * st * frameSpan * sixf);
                } else {
                    // Basic particle: single-phase color/size update
                    // TODO: same fmadds vs fmuls+fadds issue as position update
                    float t = (dt - p->birthFrame) * p->pos.w;
                    float scale = (1.0f - t) * t * frameSpan * sixf;
                    p->size += p->sizeVel * scale;
                    p->col.red += p->colVel.red * scale;
                    p->col.alpha += p->colVel.alpha * scale;
                    p->col.blue += p->colVel.blue * scale;
                    p->col.green += p->colVel.green * scale;
                }
                p = p->next;
            }
        } while (p != NULL);
    }
}

void RndParticleSys::CreateParticles(float f1, float f2, const Transform &tf) {
    if (f2 <= 0 || mNumActive >= mMaxParticles)
        return;
    else {
        mEmitCount += f2 * RandomFloat(mEmitRate.x, mEmitRate.y);
        mEmitCount += CheckBursts(f2) + (float)mExplicitParts;
        mExplicitParts = 0;
        while (mEmitCount >= 1.0f && mNumActive < mMaxParticles) {
            RndParticle *p = AllocParticle();
            if (!p) {
                mEmitCount = 0;
                return;
            }
            InitParticle(f1, p, &tf, gNoPartOverride);
            mEmitCount -= 1.0f;
        }
    }
}

void RndParticleSys::RunFastForward() {
    mNeedForward = false;

    float avgEmitRate = (mEmitRate.x + mEmitRate.y) * 0.5f;
    if (avgEmitRate < 0.0001f)
        return;

    float stepSize = 1.0f / avgEmitRate;
    float duration = Min(stepSize * (float)mMaxParticles, (mLife.x + mLife.y) * 0.5f);
    stepSize = Max(1.0f, stepSize);
    float currentFrame = CalcFrame();
    Transform xfm;
    MakeLocToRel(xfm);

    float frame;
    for (frame = currentFrame - duration; frame <= currentFrame; frame += stepSize) {
        MoveParticles(frame, stepSize);
        CreateParticles(frame, stepSize, xfm);
    }
}

void RndParticleSys::UpdateParticles() {
    if (mPreserveParticles == 0) {
        return;
    }

    f32 currentFrame = CalcFrame();

    if (mLastFrame == 0.0f) {
        mLastFrame = currentFrame;
    }

    if (mNeedForward != 0) {
        RunFastForward();
        if (mFrameDrive == 0) {
            mLastFrame = currentFrame;
        }
    } else {
        f32 frameUpdate = currentFrame - mLastFrame;
        if (mFrameDrive == 0) {
            mLastFrame = currentFrame;
        }

        if (frameUpdate != 0.0f) {
            if (mPauseOffscreen != 0) {
                if (frameUpdate > 4.0f) {
                    float excess = frameUpdate - 4.0f;
                    mPausedTime += excess;
                    frameUpdate = 4.0f;
                }
                currentFrame -= mPausedTime;
            }

            MoveParticles(currentFrame, frameUpdate);

            if ((mExplicitParts != 0) || (mEmitRate.x > 0.0f) || (mEmitRate.y > 0.0f) || (mMaxBurst != 0)) {
                Transform locToRel;
                MakeLocToRel(locToRel);

                if (mSubSamples > 1) {
                    Vector3 baseVel;
                    if (!mMeshEmitter) {
                        f32 halfSample = 0.5f;
                        f32 pitchMid = LimitAng(mPitch.y - mPitch.x) * halfSample + mPitch.x;
                        f32 yawMid = LimitAng(mYaw.y - mYaw.x) * halfSample + mYaw.x;
                        f32 speedMid = (mSpeed.y - mSpeed.x) * halfSample + mSpeed.x;

                        f32 halfPi = 1.57079637f;
                        f32 cosPitch = FastSin(pitchMid + halfPi);
                        f32 negXVel = -(FastSin(yawMid) * cosPitch * speedMid);
                        f32 yVel = FastSin(yawMid + halfPi) * cosPitch * speedMid;
                        f32 sinPitch = FastSin(pitchMid);

                        baseVel.Set(
                            negXVel * frameUpdate,
                            yVel * frameUpdate,
                            sinPitch * speedMid * frameUpdate
                        );

                        Multiply(baseVel, mSubSampleXfm, baseVel);
                    } else {
                        baseVel = mSubSampleXfm.v;
                    }

                    memcpy(&mSubSampleXfm, &locToRel, sizeof(Transform));

                    int count = mSubSamples;
                    f32 stepSize = frameUpdate / (f32)mSubSamples;
                    Vector3 interpOffset;
                    if (count != 0) {
                        do {
                            CreateParticles(currentFrame, stepSize, locToRel);
                            Interp(interpOffset, baseVel, 1.0f / (f32)count, interpOffset);
                            count--;
                        } while (count != 0);
                    }
                } else {
                    CreateParticles(currentFrame, frameUpdate, locToRel);
                }
            }
        }
    }
}

void RndParticleSys::FreeAllParticles() {
    for (RndParticle *p = mActiveParticles; p != nullptr; p = FreeParticle(p))
        ;
    mEmitCount = 0;
}

void RndParticleSys::ExplicitParticles(int i1, bool b2, PartOverride &partOverride) {
    if (b2) {
        float frame = CalcFrame();
        Transform tf;
        MakeLocToRel(tf);
        for (int i = 0; i < i1 && mNumActive < mMaxParticles; i++) {
            RndParticle *p = AllocParticle();
            if (!p)
                break;
            InitParticle(frame, p, &tf, partOverride);
        }
    } else {
        mExplicitParts += i1;
    }
}

#define PI 3.1415927f

void RndParticleSys::InitParticle(float frame, RndParticle *p, const Transform *xfm, PartOverride &po) {
    p->birthFrame = frame;
    float life;
    if (po.mask & 1) {
        life = po.life;
    } else {
        life = RandomFloat(mLife.x, mLife.y);
    }
    p->deathFrame = frame + life;

    float invLife = 0.0f;
    if (p->deathFrame > p->birthFrame) {
        invLife = 1.0f / (p->deathFrame - p->birthFrame);
    }
    p->pos.w = invLife;

    RndMesh *emitter = mMeshEmitter;
    if (po.mask & 0x100) {
        emitter = po.mesh;
    }

    if (emitter && !emitter->Faces().empty()) {
        RandomPointOnMesh(emitter, (Vector3 &)p->pos, (Vector3 &)p->vel);
    } else {
        if (po.mask & 0x200) {
            p->pos.x = RandomFloat(po.box.mMin.x, po.box.mMax.x);
            p->pos.y = RandomFloat(po.box.mMin.y, po.box.mMax.y);
            p->pos.z = RandomFloat(po.box.mMin.z, po.box.mMax.z);
        } else {
            p->pos.x = RandomFloat(mBoxExtent1.x, mBoxExtent2.x);
            p->pos.y = RandomFloat(mBoxExtent1.y, mBoxExtent2.y);
            p->pos.z = RandomFloat(mBoxExtent1.z, mBoxExtent2.z);
        }

        float p_min, p_max, y_min, y_max;
        if (po.mask & 0x80) {
            p_max = po.pitch.y;
            p_min = po.pitch.x;
            y_max = po.yaw.y;
            y_min = po.yaw.x;
        } else {
            p_max = mPitch.y;
            p_min = mPitch.x;
            y_max = mYaw.y;
            y_min = mYaw.x;
        }

        float pitch = RandomFloat(p_min, p_max);
        float yaw = RandomFloat(y_min, y_max);
        float sinP = FastSin(pitch);
        float cosP = FastCos(pitch);
        float sinY = FastSin(yaw);
        float cosY = FastCos(yaw);

        p->vel.x = -sinY * cosP;
        p->vel.y = cosY * cosP;
        p->vel.z = sinP;
    }

    float speed;
    if (po.mask & 2) {
        speed = po.speed;
    } else {
        speed = RandomFloat(mSpeed.x, mSpeed.y);
    }
    p->vel.y *= speed;
    p->vel.x *= speed;
    p->vel.z *= speed;

    if (mRotate) {
        p->angle = RandomFloat(0, 6.2831855f);
        p->swingArm = RandomFloat(mStartOffset.x, mStartOffset.y);
    } else {
        p->angle = 0;
        p->swingArm = 0;
    }

    if (po.mask & 0x10) {
        p->col = po.startColor;
    } else {
        float h1, s1, l1, h2, s2, l2;
        MakeHSL(mStartColorLow, h1, s1, l1);
        MakeHSL(mStartColorHigh, h2, s2, l2);
        auto _tmp1 = RandomFloat(h1, h2);
        MakeColor(_tmp1, RandomFloat(s1, s2), RandomFloat(l1, l2), p->col);
        p->col.alpha = RandomFloat(mStartColorLow.alpha, mStartColorHigh.alpha);
    }

    if (po.mask & 4) {
        p->size = po.size;
    } else {
        p->size = RandomFloat(mStartSize.x, mStartSize.y);
    }

    if (po.mask & 8) {
        p->sizeVel = po.deltaSize;
    } else {
        p->sizeVel = RandomFloat(mDeltaSize.x, mDeltaSize.y);
    }
    if (p->sizeVel < -p->size) {
        p->sizeVel = -p->size;
    }

    if (po.mask & 0x40) {
        p->colVel = po.endColor;
    } else {
        float h1, s1, l1, h2, s2, l2;
        MakeHSL(mEndColorLow, h1, s1, l1);
        MakeHSL(mEndColorHigh, h2, s2, l2);
        MakeColor(RandomFloat(h1, h2), RandomFloat(s1, s2), RandomFloat(l1, l2), p->colVel);
        p->colVel.alpha = RandomFloat(mEndColorLow.alpha, mEndColorHigh.alpha);
    }

    if ((unsigned long)(int)mType == kFancy) {
        RndFancyParticle *fp = static_cast<RndFancyParticle *>(p);
        memcpy(&fp->mRPMVelocity, &mMotionParentDelta, 16);

        if (mBubble) {
            fp->bubbleFreq = PI / RandomFloat(mBubblePeriod.x, mBubblePeriod.y);
            fp->bubblePhase = RandomFloat(0, 6.2831855f);
            float ang = RandomFloat(0, 6.2831855f);
            float bsize = RandomFloat(mBubbleSize.x, mBubbleSize.y);
            fp->bubbleDir.x = FastCos(ang) * bsize;
            fp->bubbleDir.y = 0;
            fp->bubbleDir.z = FastSin(ang) * bsize;
            float s = FastSin(fp->bubblePhase);
            p->pos.x += fp->bubbleDir.x * s;
            p->pos.y += fp->bubbleDir.y * s;
            p->pos.z += fp->bubbleDir.z * s;
            fp->bubblePhase = -(frame * fp->bubbleFreq - fp->bubblePhase);
        }

        if (mRotate) {
            fp->RPF = RandomFloat(mRPM.x, mRPM.y) * 0.0034906587f;
            if (mRandomDirection && (RandomInt() & 0x100000)) {
                fp->RPF = -fp->RPF;
            }
            fp->swingArmVel = (RandomFloat(mEndOffset.x, mEndOffset.y) - p->swingArm) * invLife;
        } else {
            fp->RPF = 0;
            fp->swingArmVel = 0;
        }

        if (mGrowRatio == 0) {
            fp->growVel = 0;
            fp->growFrame = p->birthFrame;
        } else {
            float s = p->size;
            float b = p->birthFrame;
            fp->growFrame = (p->deathFrame - b) * mGrowRatio + b;
            float gdiff = fp->growFrame - b;
            if (gdiff != 0) {
                fp->growVel = s / gdiff;
            } else {
                fp->growVel = 0;
            }
        }

        if (mShrinkRatio == 1.0f) {
            fp->shrinkVel = 0;
            fp->shrinkFrame = p->deathFrame;
        } else {
            float s = p->size;
            float sv = p->sizeVel;
            fp->shrinkFrame = (p->deathFrame - p->birthFrame) * mShrinkRatio + p->birthFrame;
            float sdiff = fp->shrinkFrame - p->deathFrame;
            if (sdiff != 0) {
                fp->shrinkVel = (s + sv) / sdiff;
            } else {
                fp->shrinkVel = 0;
            }
        }

        if (p->birthFrame < fp->growFrame) {
            fp->beginGrow = 1.0f / (fp->growFrame - p->birthFrame);
        } else {
            fp->beginGrow = 0;
        }

        if (fp->shrinkFrame > fp->growFrame) {
            fp->midGrow = 1.0f / (fp->shrinkFrame - fp->growFrame);
        } else {
            fp->midGrow = 0;
        }

        if (p->deathFrame > fp->shrinkFrame) {
            fp->endGrow = 1.0f / (p->deathFrame - fp->shrinkFrame);
        } else {
            fp->endGrow = 0;
        }

        if (mGrowRatio != 0) {
            p->size = 0;
        }

        fp->midcolFrame = (p->deathFrame - p->birthFrame) * mMidColorRatio + p->birthFrame;
        if (po.mask & 0x20) {
            fp->midcolVel = po.midColor;
        } else {
            float h1, s1, l1, h2, s2, l2;
            MakeHSL(mMidColorLow, h1, s1, l1);
            MakeHSL(mMidColorHigh, h2, s2, l2);
            MakeColor(RandomFloat(h1, h2), RandomFloat(s1, s2), RandomFloat(l1, l2), fp->midcolVel);
            fp->midcolVel.alpha = RandomFloat(mMidColorLow.alpha, mMidColorHigh.alpha);
        }

        float btom = 0;
        if (p->birthFrame < fp->midcolFrame) {
            btom = 1.0f / (fp->midcolFrame - p->birthFrame);
        }
        p->vel.w = btom;

        float mtod = 0;
        if (fp->midcolFrame < p->deathFrame) {
            mtod = 1.0f / (p->deathFrame - fp->midcolFrame);
        }
        fp->bubbleDir.w = mtod;

        p->colVel.red -= fp->midcolVel.red;
        p->colVel.green -= fp->midcolVel.green;
        p->colVel.blue -= fp->midcolVel.blue;
        p->colVel.alpha -= fp->midcolVel.alpha;
        if (p->deathFrame != fp->midcolFrame) {
            float f = 1.0f / (p->deathFrame - fp->midcolFrame);
            p->colVel.red *= f;
            p->colVel.green *= f;
            p->colVel.blue *= f;
            p->colVel.alpha *= f;
        }

        if (fp->midcolFrame != p->birthFrame) {
            float f = 1.0f / (fp->midcolFrame - p->birthFrame);
            fp->midcolVel.red = (fp->midcolVel.red - p->col.red) * f;
            fp->midcolVel.green = (fp->midcolVel.green - p->col.green) * f;
            fp->midcolVel.blue = (fp->midcolVel.blue - p->col.blue) * f;
            fp->midcolVel.alpha = (fp->midcolVel.alpha - p->col.alpha) * f;
        }
    } else {
        p->colVel.red = (p->colVel.red - p->col.red) * invLife;
        p->colVel.green = (p->colVel.green - p->col.green) * invLife;
        p->colVel.blue = (p->colVel.blue - p->col.blue) * invLife;
        p->colVel.alpha = (p->colVel.alpha - p->col.alpha) * invLife;
    }

    p->sizeVel *= invLife;

    Transform local_tf;
    if (!xfm) {
        MakeLocToRel(local_tf);
        xfm = &local_tf;
    }

    Multiply((Vector3 &)p->pos, *xfm, (Vector3 &)p->pos);

    float vx = p->vel.x;
    float vy = p->vel.y;
    float vz = p->vel.z;
    // Evaluation order: z then y then x
    p->vel.z = vz * xfm->m.z.z + (vx * xfm->m.x.z + vy * xfm->m.y.z);
    p->vel.y = vz * xfm->m.z.y + (vx * xfm->m.x.y + vy * xfm->m.y.y);
    p->vel.x = vz * xfm->m.z.x + (vx * xfm->m.x.x + vy * xfm->m.y.x);

    if (mBubble && mType == kFancy) {
        RndFancyParticle *fp = static_cast<RndFancyParticle *>(p);
        float bx = fp->bubbleDir.x;
        float by = fp->bubbleDir.y;
        float bz = fp->bubbleDir.z;
        fp->bubbleDir.z = bz * xfm->m.z.z + (bx * xfm->m.x.z + by * xfm->m.y.z);
        fp->bubbleDir.y = bz * xfm->m.z.y + (bx * xfm->m.x.y + by * xfm->m.y.y);
        fp->bubbleDir.x = bz * xfm->m.z.x + (bx * xfm->m.x.x + by * xfm->m.y.x);
    }

    if (mRandomAnimStart) {
        p->mCurrentTileIndex = RandomInt(0, mNumTilesTotal);
    } else {
        p->mCurrentTileIndex = mStartingTile;
    }
    p->mTileTime = 0;
}

void RndParticleSys::InitParticle(RndParticle *p, const Transform *t) {
    InitParticle(CalcFrame(), p, t, gNoPartOverride);
}

void RndParticleSys::SetRelativeMotion(float motion, RndTransformable *parent) {
    mMotionParent = parent ? parent : this;
    mRelativeMotion = motion;
    mLastWorldXfm = mMotionParent->WorldXfm();
    if (motion == 1) {
        mRelativeXfm = mMotionParent->WorldXfm();
    } else {
        mRelativeXfm.Reset();
    }
    mMotionParentDelta.Zero();
}

DataNode RndParticleSys::OnSetStartColor(const DataArray *da) {
    DataArray *arr1 = da->Array(2);
    DataArray *arr2 = da->Array(3);
    SetStartColor(
        Hmx::Color(arr1->Float(0), arr1->Float(1), arr1->Float(2), arr1->Float(3)),
        Hmx::Color(arr2->Float(0), arr2->Float(1), arr2->Float(2), arr2->Float(3))
    );
    return 0;
}

DataNode RndParticleSys::OnSetStartColorInt(const DataArray *da) {
    Hmx::Color col1(da->Int(2));
    Hmx::Color col2(da->Int(3));
    col1.alpha = da->Float(4);
    col2.alpha = da->Float(5);
    SetStartColor(col1, col2);
    return 0;
}

DataNode RndParticleSys::OnSetEndColor(const DataArray *da) {
    DataArray *arr1 = da->Array(2);
    DataArray *arr2 = da->Array(3);
    SetEndColor(
        Hmx::Color(arr1->Float(0), arr1->Float(1), arr1->Float(2), arr1->Float(3)),
        Hmx::Color(arr2->Float(0), arr2->Float(1), arr2->Float(2), arr2->Float(3))
    );
    return 0;
}

DataNode RndParticleSys::OnSetEndColorInt(const DataArray *da) {
    Hmx::Color col1(da->Int(2));
    Hmx::Color col2(da->Int(3));
    col1.alpha = da->Float(4);
    col2.alpha = da->Float(5);
    SetEndColor(col1, col2);
    return 0;
}

DataNode RndParticleSys::OnSetEmitRate(const DataArray *da) {
    SetEmitRate(da->Float(2), da->Float(3));
    return 0;
}

DataNode RndParticleSys::OnAddEmitRate(const DataArray *da) {
    float add = da->Float(2);
    mEmitRate.x = Max(0.0f, mEmitRate.x + add);
    mEmitRate.y = Max(0.0f, mEmitRate.y + add);
    return !mEmitRate;
}

DataNode RndParticleSys::OnSetBurstInterval(const DataArray *da) {
    SetMaxBurst(da->Int(2));
    SetTimeBetweenBursts(da->Float(3), da->Float(4));
    return 0;
}

DataNode RndParticleSys::OnSetBurstPeak(const DataArray *da) {
    SetPeakRate(da->Float(2), da->Float(3));
    return 0;
}

DataNode RndParticleSys::OnSetBurstLength(const DataArray *da) {
    SetDuration(da->Float(2), da->Float(3));
    return 0;
}

DataNode RndParticleSys::OnSetLife(const DataArray *da) {
    SetLife(da->Float(2), da->Float(3));
    return 0;
}

DataNode RndParticleSys::OnSetSpeed(const DataArray *da) {
    SetSpeed(da->Float(2), da->Float(3));
    return 0;
}

DataNode RndParticleSys::OnSetRotate(const DataArray *da) {
    SetRotate(da->Int(2));
    SetRPM(da->Float(3), da->Float(4));
    SetRPMDrag(da->Float(4));
    return 0;
}

DataNode RndParticleSys::OnSetSwingArm(const DataArray *da) {
    SetStartOffset(da->Float(2), da->Float(3));
    SetEndOffset(da->Float(4), da->Float(5));
    return 0;
}

DataNode RndParticleSys::OnSetDrag(const DataArray *da) {
    SetDrag(da->Float(2));
    return 0;
}

DataNode RndParticleSys::OnSetAlignment(const DataArray *da) {
    SetAlignWithVelocity(da->Int(2));
    SetStretchWithVelocity(da->Int(3));
    SetConstantArea(da->Int(4));
    SetStretchScale(da->Float(5));
    return 0;
}

DataNode RndParticleSys::OnSetStartSize(const DataArray *da) {
    SetStartSize(da->Float(2), da->Float(3));
    return 0;
}

DataNode RndParticleSys::OnSetMat(const DataArray *da) {
    SetMat(da->Obj<RndMat>(2));
    return 0;
}

DataNode RndParticleSys::OnSetPos(const DataArray *da) {
    SetBoxExtent(
        Vector3(da->Float(2), da->Float(3), da->Float(4)),
        Vector3(da->Float(5), da->Float(6), da->Float(7))
    );
    return 0;
}

DataNode RndParticleSys::OnActiveParticles(const DataArray *da) {
    return mActiveParticles != nullptr;
}

DataNode RndParticleSys::OnExplicitPart(const DataArray *da) {
    ExplicitParticles(1, false, gNoPartOverride);
    return 0;
}

DataNode RndParticleSys::OnExplicitParts(const DataArray *da) {
    bool b = da->Size() >= 4 && da->Int(3);
    ExplicitParticles(da->Int(2), b, gNoPartOverride);
    return 0;
}

bool RndParticleSys::Burst::Set(float f1, float f2) {
    if (f2 > 0) {
        mPeakRate = f1;
        mHalfDuration = f2 * 0.5f;
        mRemainingDuration = f2;
        mInvHalfDuration = 1.0f / mHalfDuration;
        return true;
    } else
        return false;
}

float RndParticleSys::Burst::Emit(float f1) {
    mRemainingDuration -= f1;
    if (mRemainingDuration < 0)
        return -1;
    float ret = mRemainingDuration;
    if (ret > mHalfDuration) {
        ret = mHalfDuration * 2.0f - ret;
    }
    ret *= mInvHalfDuration;
    float ret2 = ret * ret;
    float ret3 = ret2 * ret;
    return (ret2 * 3.0f - ret3 * 2.0f) * mPeakRate * f1;
}

float RndParticleSys::CheckBursts(float f1) {
    if (f1 > 1)
        f1 = 1;
    float sum = 0;
    for (std::vector<Burst>::iterator it = mBursts.begin(); it != mBursts.end();) {
        float emit = it->Emit(f1);
        if (emit < 0)
            it = mBursts.erase(it);
        else {
            sum += emit;
            ++it;
        }
    }
    if (mBursts.size() < mMaxBurst) {
        mTimeTillBurst -= f1;
        if (mTimeTillBurst <= 0) {
            Burst burst;
            if (burst.Set(
                    RandomFloat(mBurstPeak.x, mBurstPeak.y),
                    RandomFloat(mBurstLength.x, mBurstLength.y)
                )) {
                mBursts.push_back(burst);
            }
            mTimeTillBurst = RandomFloat(mBurstInterval.x, mBurstInterval.y);
        }
    }
    return sum;
}

bool RndParticleSys::MakeWorldSphere(Sphere &s, bool b2) {
    if (b2) {
        s.Zero();
        for (RndParticle *p = mActiveParticles; p != nullptr; p = p->next) {
            Sphere s38;
            Multiply((const Vector3 &)p->pos, mRelativeXfm, s38.center);
            s38.radius = p->size * 0.5f;
            s.GrowToContain(s38);
        }
        return true;
    }
    if (mSphere.GetRadius()) {
        Multiply(mSphere, WorldXfm(), s);
        return true;
    }
    return false;
}
