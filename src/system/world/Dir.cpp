#include "world/Dir.h"
#include "Dir.h"
#include "PhysicsManager.h"
#include "SpotlightDrawer.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/Timer.h"
#include "rndobj/BaseMaterial.h"
#include "rndobj/Cam.h"
#include "rndobj/Dir.h"
#include "rndobj/Env.h"
#include "rndobj/Graph.h"
#include "rndobj/Mat.h"
#include "rndobj/PostProc.h"
#include "rndobj/Rnd.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "synth/FxSend.h"
#include "ui/PanelDir.h"
#include "ui/UI.h"
#include "utl/BinStream.h"
#include "world/CameraManager.h"
#include "world/Spotlight.h"

WorldDir *TheWorld = nullptr;
// Internal: retail addresses it off the same base register as the rev file
// static (gWorldDirRevs + 0xc).
static ObjectDir *gOldTexDir = nullptr;
std::vector<FilePath> gOldChars;

void SetTheWorld(WorldDir *w) {
    static DataNode &n = DataVariable("world");
    n = w;
    TheWorld = w;
}

WorldDir::WorldDir()
    : mPresetOverrides(this), mBitmapOverrides(this), mMatOverrides(this),
      mHideOverrides(this), mCamShotOverrides(this), mPS3PerPixelShows(this),
      mPS3PerPixelHides(this), mCrowds(this), mHUDDir(0), mShowHUD(0), mHUD(ObjPtrInlineOwner(), this),
      mCameraManager(this), mLightPresetMgr(this), mEchoMsgs(0), mFirstPoll(0),
      mPollCamera(1), mTestLightPreset1(ObjPtrInlineOwner(), this),
      mTestLightPreset2(ObjPtrInlineOwner(), this),
      mTestAnimTime(10) {
    // Retail ctor 0x824BC930 tail: per-instance glow mat (New<RndMat> +
    // SetBlend/SetZMode/SetPreLit inlined), then zero deltas.
    mGlowMat = Hmx::Object::New<RndMat>();
    mGlowMat->SetBlend(RndMat::kBlendSrcAlpha);
    mGlowMat->SetZMode(kZModeDisable);
    mGlowMat->SetPreLit(true);
    ClearDeltas();
}

WorldDir::~WorldDir() {
#ifdef HX_NATIVE
    RELEASE(mHUDDir);
#else
    // Retail deletes the HUD dir and the ctor-created glow material, nulling neither.
    delete mHUDDir;
    delete mGlowMat;
#endif
    SpotlightDrawer::Current()->ClearLights();
    if (TheWorld == this) {
        SetTheWorld(nullptr);
    }
#ifdef WORLDDIR_DC3_TAIL
    RELEASE(mPhysicsMgr);
#endif
}

BEGIN_HANDLERS(WorldDir)
    if (mEchoMsgs && !_warn) {
        MILO_LOG("World msg: %s\n", sym);
    }
#ifdef WORLDDIR_DC3_TAIL
#endif
    HANDLE_MEMBER_PTR((&mCameraManager))
    HANDLE_MEMBER_PTR((&mLightPresetMgr))
    HANDLE_SUPERCLASS(PanelDir)
END_HANDLERS

#define SYNC_PROP_OVERRIDE(s, member, sync_func)                                         \
    {                                                                                    \
        _NEW_STATIC_SYMBOL(s)                                                            \
        if (sym == _s) {                                                                 \
            if (!(_op & (kPropSize | kPropGet)))                                         \
                sync_func(false);                                                        \
            bool synced = PropSync(member, _val, _prop, _i + 1, _op);                    \
            if (synced) {                                                                \
                if (!(_op & (kPropSize | kPropGet)))                                     \
                    sync_func(true);                                                     \
                return true;                                                             \
            } else                                                                       \
                return false;                                                            \
        }                                                                                \
    }

void WorldDir::PresetOverride::Sync(bool set) {
    if (preset)
        preset->SetHue(set ? hue.Ptr() : nullptr);
}

BEGIN_CUSTOM_PROPSYNC(WorldDir::PresetOverride)
    SYNC_PROP_OVERRIDE(preset, o.preset, o.Sync)
    SYNC_PROP_OVERRIDE(hue, o.hue, o.Sync)
END_CUSTOM_PROPSYNC

#ifndef HX_NATIVE
// Retail (0x824CC038): walk the target texture's ref ring directly (next saved
// before the call) and Replace each foreign-dir ref through ObjRefOwner.
void WorldDir::BitmapOverride::Sync(bool b) {
    if (!original || !replacement)
        return;
    if (!b) {
        for (ObjRef::iterator it = replacement->Refs().begin();
             it != replacement->Refs().end();) {
            ObjRef *cur = it;
            ++it;
            ObjRefOwner *ref = RefPtrOf(cur);
            if (ref->RefOwner()->Dir() != replacement->Dir())
                ref->Replace(reinterpret_cast<ObjRef *>((RndTex *)replacement), original);
        }
    } else {
        for (ObjRef::iterator it = original->Refs().begin();
             it != original->Refs().end();) {
            ObjRef *cur = it;
            ++it;
            ObjRefOwner *ref = RefPtrOf(cur);
            if (ref->RefOwner() && ref->RefOwner()->Dir() != replacement->Dir())
                ref->Replace(reinterpret_cast<ObjRef *>((RndTex *)original), replacement);
        }
    }
}
#else
void WorldDir::BitmapOverride::Sync(bool b) {
    if (!original)
        return;
    if (!replacement)
        return;
    if (!b) {
        ObjRef localRing;
        localRing.Clear();
        ObjRef::iterator it = replacement->Refs().begin();
        while (it != replacement->Refs().end()) {
            ObjRef *ref = it;
            if (RefPtrOf(ref)->RefOwner()->Dir() != replacement->Dir()) {
                ObjRef *p = ref->SpliceToRing(&localRing);
                it = ObjRef::iterator(p);
            }
            ++it;
        }
        localRing.ReplaceList(original);
    } else {
        ObjRef localRing;
        localRing.Clear();
        ObjRef::iterator it = original->Refs().begin();
        while (it != original->Refs().end()) {
            ObjRef *ref = it;
            if (RefPtrOf(ref)->RefOwner() && RefPtrOf(ref)->RefOwner()->Dir() != replacement->Dir()) {
                ObjRef *p = ref->SpliceToRing(&localRing);
                it = ObjRef::iterator(p);
            }
            ++it;
        }
        localRing.ReplaceList(replacement);
    }
}
#endif

BEGIN_CUSTOM_PROPSYNC(WorldDir::BitmapOverride)
    SYNC_PROP_OVERRIDE(original, o.original, o.Sync)
    SYNC_PROP_OVERRIDE(replacement, o.replacement, o.Sync)
END_CUSTOM_PROPSYNC

void WorldDir::MatOverride::Sync(bool b) {
    if (!mat || !mesh)
        return;
    else if (!b) {
        if (mat2)
            mesh->SetMat(mat2);
    } else {
        mat2 = mesh->Mat();
        mesh->SetMat(mat);
    }
}

BEGIN_CUSTOM_PROPSYNC(WorldDir::MatOverride)
    SYNC_PROP_OVERRIDE(mesh, o.mesh, o.Sync)
    SYNC_PROP_OVERRIDE(mat, o.mat, o.Sync)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(WorldDir)
    SYNC_PROP_MODIFY(hud_filename, mHUDFilename, SyncHUD())
#ifdef HX_NATIVE
    // DC3-era addition; RB3-360 retail goes straight from `hud_filename` to
    // `echo_msgs`.  Arbitrated on RETAIL BYTES (lane CQ-3): the 1484 B retail
    // body enumerates 13 property-name literals and `show_hud` is not among
    // them.  Native-only so the host engine keeps the HUD toggle.
    SYNC_PROP_MODIFY(show_hud, mShowHUD, SyncHUD())
#endif
    SYNC_PROP(echo_msgs, mEchoMsgs)
    SYNC_PROP_OVERRIDE(hide_overrides, mHideOverrides, SyncHides)
    SYNC_PROP(bitmap_overrides, mBitmapOverrides)
    SYNC_PROP(mat_overrides, mMatOverrides)
    SYNC_PROP(preset_overrides, mPresetOverrides)
    SYNC_PROP_OVERRIDE(camshot_overrides, mCamShotOverrides, SyncCamShots)
    SYNC_PROP(ps3_per_pixel_hides, mPS3PerPixelHides)
    SYNC_PROP(ps3_per_pixel_shows, mPS3PerPixelShows)
    SYNC_PROP(test_light_preset_1, mTestLightPreset1)
    SYNC_PROP(test_light_preset_2, mTestLightPreset2)
    SYNC_PROP(test_animation_time, mTestAnimTime)
    SYNC_PROP_MODIFY(hud, mHUD, SyncObjects())
#ifdef WORLDDIR_DC3_TAIL
    SYNC_PROP_SET(
        doppler_power, m3DSoundMgr.mDopplerPower, m3DSoundMgr.mDopplerPower = _val.Float()
    )
    SYNC_PROP_SET(
        listener,
        m3DSoundMgr.mListener.Ptr(),
        m3DSoundMgr.SetListener(_val.Obj<RndTransformable>())
    )
    SYNC_PROP(explicit_postproc, mExplicitPostProc)
#endif
    SYNC_SUPERCLASS(PanelDir)
END_PROPSYNCS

BinStream &operator<<(BinStream &bs, const WorldDir::PresetOverride &o) {
    bs << o.preset << o.hue;
    return bs;
}

BinStream &operator<<(BinStream &bs, const WorldDir::BitmapOverride &o) {
    bs << o.original << o.replacement;
    return bs;
}

// mat2 is not part of the stream: the reader below takes mesh and mat only.
BinStream &operator<<(BinStream &bs, const WorldDir::MatOverride &o) {
    bs << o.mesh << o.mat;
    return bs;
}

// Retail writes rev 0x19 with no alt rev, and the stream ends at mHUD.
BEGIN_SAVES(WorldDir)
#ifdef WORLDDIR_DC3_TAIL
    SAVE_REVS(0x1D, 1)
#else
    SAVE_REVS(0x19, 0)
#endif
    bs << mHUDFilename;
    SAVE_SUPERCLASS(PanelDir)
    bs << mHideOverrides << mBitmapOverrides << mMatOverrides << mPresetOverrides;
    bs << mCamShotOverrides << mPS3PerPixelHides << mPS3PerPixelShows;
    bs << mTestLightPreset1 << mTestLightPreset2 << mTestAnimTime;
    bs << mHUD;
#ifdef WORLDDIR_DC3_TAIL
    bs << m3DSoundMgr.mDopplerPower;
    ObjPtr<RndTransformable> listener(this, m3DSoundMgr.mListener);
    bs << listener;
    bs << mExplicitPostProc;
#endif
END_SAVES

BEGIN_COPYS(WorldDir)
    COPY_SUPERCLASS(PanelDir)
    CREATE_COPY(WorldDir)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mHUDFilename)
        SyncHides(false);
        COPY_MEMBER(mHideOverrides)
        SyncHides(true);
        SyncBitmaps(false);
        COPY_MEMBER(mBitmapOverrides)
        SyncBitmaps(true);
        SyncMats(false);
        COPY_MEMBER(mMatOverrides)
        SyncMats(true);
        SyncPresets(false);
        COPY_MEMBER(mPresetOverrides)
        SyncPresets(true);
        SyncCamShots(false);
        COPY_MEMBER(mCamShotOverrides)
        SyncCamShots(true);
        COPY_MEMBER(mPS3PerPixelHides)
        COPY_MEMBER(mPS3PerPixelShows)
        COPY_MEMBER(mTestLightPreset1)
        COPY_MEMBER(mTestLightPreset2)
        COPY_MEMBER(mTestAnimTime)
        COPY_MEMBER(mHUD)
#ifdef WORLDDIR_DC3_TAIL
        COPY_MEMBER(m3DSoundMgr.mDopplerPower)
        m3DSoundMgr.SetListener(c->m3DSoundMgr.mListener);
        SyncHUD();
        COPY_MEMBER(mExplicitPostProc)
#else
        SyncHUD();
#endif
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0x1D, 1)

// Retail 0x824D15C0: the revs live in a file static (alt at +0, rev at +4),
// there is no minimum-rev assert, and the rev is pushed BEFORE
// PanelDir::PreLoad runs.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gWorldDirRevs;

void WorldDir::PreLoad(BinStream &bs) {
    int revs;
    bs >> revs;
    gWorldDirRevs.rev = getHmxRev(revs);
    gWorldDirRevs.altRev = getAltRev(revs);
    if (gWorldDirRevs.rev > 0 && gWorldDirRevs.rev < 5) {
        ObjPtr<RndCam> cam(ObjPtrInlineOwner(), this);
        bs >> cam;
    }
    if (gWorldDirRevs.rev > 1 && gWorldDirRevs.rev < 0x15) {
        int x, y;
        bs >> x >> y;
    }
    if (gWorldDirRevs.rev > 9) {
        bs >> mHUDFilename;
    }
    if (gWorldDirRevs.rev < 9) {
        if (gWorldDirRevs.rev > 7) {
            OldLoadProxies(bs, 0);
        } else if (gWorldDirRevs.rev > 2) {
            bs >> gOldChars;
        }
    }
    bs.PushRev(packRevs(gWorldDirRevs.altRev, gWorldDirRevs.rev), this);
    PanelDir::PreLoad(bs);
}

BinStream &operator>>(BinStream &bs, WorldDir::PresetOverride &o) {
    bs >> o.preset >> o.hue;
    return bs;
}

BinStreamRev &operator>>(BinStreamRev &d, WorldDir::PresetOverride &o) {
    d.stream >> o;
    return d;
}

BinStream &operator>>(BinStream &bs, WorldDir::BitmapOverride &c) {
    bs >> c.original;
    if (gOldTexDir) {
        FilePath bitmap;
        bs >> bitmap;
        if (!bitmap.empty()) {
            const char *chr = strrchr(bitmap.c_str(), 0x2F);
            if (!chr) {
                chr = bitmap.c_str();
            } else {
                chr = chr + 1;
            }
            c.replacement = gOldTexDir->Find<RndTex>(chr, false);
            if (!c.replacement) {
                MILO_WARN(
                    "Loading %s synchronously, please resave %s",
                    chr,
                    gOldTexDir->Loader()->LoaderFile()
                );
                c.replacement = gOldTexDir->New<RndTex>(chr);
                c.replacement->SetBitmap(bitmap);
            } else {
                MILO_ASSERT(c.replacement->File() == bitmap, 0x163);
            }
        } else
            c.replacement.ReleaseObjConcrete(); // retail open-codes the null release
    } else
        bs >> c.replacement;
    return bs;
}

BinStreamRev &operator>>(BinStreamRev &d, WorldDir::BitmapOverride &o) {
    d.stream >> o;
    return d;
}

BinStream &operator>>(BinStream &bs, WorldDir::MatOverride &o) {
    bs >> o.mesh >> o.mat;
    return bs;
}

BinStreamRev &operator>>(BinStreamRev &d, WorldDir::MatOverride &o) {
    d.stream >> o;
    return d;
}

// Retail 0x824D08F0: PanelDir::PostLoad runs first, then the rev is popped into
// the same file static PreLoad fills, and every field comes from the raw
// stream. Retail stops after SyncHUD(); the FxSend, doppler, listener and
// alt-rev reads are newer-engine revisions.
void WorldDir::PostLoad(BinStream &bs) {
    PanelDir::PostLoad(bs);
    int revs = bs.PopRev(this);
    gWorldDirRevs.rev = getHmxRev(revs);
    gWorldDirRevs.altRev = getAltRev(revs);
    if (gWorldDirRevs.rev > 4 && gWorldDirRevs.rev < 6) {
        ObjPtr<RndCam> cam(ObjPtrInlineOwner(), this);
        bs >> cam;
        SetCam(cam);
    }
    if (gWorldDirRevs.rev < 8) {
        for (int i = 0; i < gOldChars.size(); i++) {
            Symbol dirClass = DirLoader::GetDirClass(gOldChars[i].c_str());
            RndDir *p = dynamic_cast<RndDir *>(Hmx::Object::NewObject(dirClass));
            MILO_ASSERT(p, 0x1AA);
            p->SetProxyFile(gOldChars[i], false);
            char buf[0x80];
            bs.ReadString(buf, 0x80);
            p->SetName(buf, this);
            p->RndTransformable::Load(bs);
            bool showing;
            bs >> showing;
            float fff;
            bs >> fff;
            if (p) {
                p->SetShowing(showing);
                p->SetOrder(fff);
            }
            bs.ReadString(buf, 0x80);
            if (p && *buf != '\0') {
                p->SetEnv(Find<RndEnviron>(buf, true));
            }
        }
        gOldChars.clear();
    }
    if (gWorldDirRevs.rev < 0x19) {
        if (gWorldDirRevs.rev > 0xA) {
            Transform tf;
            bs >> tf;
        } else if (gWorldDirRevs.rev > 6 && mCam) {
            mCam->RndTransformable::Load(bs);
        }
    }
    if (gWorldDirRevs.rev > 0xB) {
        SyncHides(false);
        bs >> mHideOverrides;
        SyncHides(true);
        SyncBitmaps(false);
        gOldTexDir = gWorldDirRevs.rev > 0xC ? nullptr : Dir();
        bs >> mBitmapOverrides;
        SyncBitmaps(true);
    }
    if (gWorldDirRevs.rev > 0xD) {
        SyncMats(false);
        bs >> mMatOverrides;
        SyncMats(true);
    }
    if (gWorldDirRevs.rev > 0xE) {
        SyncPresets(false);
        bs >> mPresetOverrides;
        SyncPresets(true);
    }
    if (gWorldDirRevs.rev > 0xF) {
        SyncCamShots(false);
        mCamShotOverrides.Load(bs, false);
        SyncCamShots(true);
    }
    if (gWorldDirRevs.rev > 0x10 && gWorldDirRevs.rev != 0x17) {
        bs >> mPS3PerPixelHides >> mPS3PerPixelShows;
    }
    if (gWorldDirRevs.rev > 0x11 && gWorldDirRevs.rev < 0x16) {
        Symbol s;
        bs >> s;
    }
    if (gWorldDirRevs.rev > 0x12) {
        bs >> mTestLightPreset1 >> mTestLightPreset2 >> mTestAnimTime;
    }
    if (gWorldDirRevs.rev > 0x13) {
        bs >> mHUD;
    }
    SyncHUD();
#ifdef WORLDDIR_DC3_TAIL
    if (gWorldDirRevs.rev == 0x1A) {
        ObjPtr<FxSend> send(this);
        bs >> send;
    }
    if (gWorldDirRevs.rev >= 0x1C) {
        float x;
        bs >> x;
        m3DSoundMgr.mDopplerPower = x;
    }
    if (gWorldDirRevs.rev >= 0x1D) {
        ObjPtr<RndTransformable> trans(this);
        bs >> trans;
        m3DSoundMgr.SetListener(trans);
    }
    if (gWorldDirRevs.altRev > 0) {
        bs >> mExplicitPostProc;
    }
#endif
}

// class PhysicsManager * (__cdecl* CreatePhysicsManager)(class RndDir *)

void WorldDir::SyncObjects() {
    PanelDir::SyncObjects();
    mCameraManager.SyncObjects();
    mLightPresetMgr.SyncObjects();
#ifdef WORLDDIR_DC3_TAIL
    m3DSoundMgr.SyncObjects();
#endif
    mCrowds.clear();
    for (ObjDirItr<WorldCrowd> it(this, true); it != nullptr; ++it) {
        mCrowds.push_back(it);
    }
#ifdef WORLDDIR_DC3_TAIL
    if (!mPhysicsMgr) {
        if (CreatePhysicsManager) {
            mPhysicsMgr = CreatePhysicsManager(this);
            if (mNeedPhysicsEnter) {
                mPhysicsMgr->Enter();
            }
        }
    }
    mPhysicsMgr->SyncObjects(false);
#endif
    if (mHUD) {
        VectorRemove(mDraws, mHUD);
    }
}

void WorldDir::SetCrowds(ObjVector<CamShotCrowd> &crowds) {
    FOREACH (it, mCrowds) {
        WorldCrowd *curCrowd = *it;
        CamShotCrowd *cit = crowds.begin();
        for (; cit != crowds.end(); cit++) {
            if (curCrowd == cit->mCrowd) {
                break;
            }
        }
        if (cit != crowds.end()) {
            curCrowd->SetShowing(true);
            curCrowd->mCrowdRotate = cit->mCrowdRotate;
        } else {
            curCrowd->SetShowing(false);
        }
    }
}

void WorldDir::DrawShowing() {
    START_AUTO_TIMER("world_draw");
    if (TheWorld) {
        MILO_ASSERT(TheWorld != this, 0x25c);
        if (Showing())
            RndDir::DrawShowing();
    } else {
        SetTheWorld(this);

        CamShot *shot = nullptr;
        shot = mCameraManager.MiloCamera();
        if (!shot)
            shot = mCameraManager.CurrentShot();
        if (shot)
            shot = shot->CurrentShot();

        RndCam *savedCam = CamOverride();
        if (!savedCam) {
            savedCam = RndCam::Current();
        } else {
            savedCam->Select();
        }

        RndEnviron *env = GetEnv() ? GetEnv() : TheUI->GetEnv();
        env->Select(nullptr);

        if (TheRnd.ProcCmds() & kProcessWorld) {
            if (!shot || shot->mDrawOverrides.empty()) {
                RndDir::DrawShowing();
            } else {
                FOREACH (it, shot->mDrawOverrides) {
                    (*it)->DrawShowing();
                }
            }

            if (shot) {
                Spotlight *spot = shot->mGlowSpot;
                // Retail does not test mGlowMat (the ctor always creates it).
                if (spot && spot->Showing() && spot->Intensity() > 0) {
                    Hmx::Rect rect(0, 0, TheRnd.Width(), TheRnd.Height());
                    Hmx::Color color(spot->Color());
                    color.alpha = 0.25f;
                    TheRnd.DrawRect(rect, color, mGlowMat, nullptr, nullptr);
                }
            }
        }

        TheRnd.CopyWorldCam(TheWorld->Cam());
#ifdef WORLDDIR_DC3_TAIL
        if (mExplicitPostProc)
#endif
        {
            TheRnd.EndWorld();
        }

        if (shot) {
            savedCam->Select();
            env->Select(nullptr);
            FOREACH (it, shot->mPostProcOverrides) {
                (*it)->DrawShowing();
            }
        }

#ifdef HX_NATIVE
        // Not in retail's DrawShowing (0x824CD068): no debug-graph camera here.
        RndGraph::SetCamera(RndCam::Current());
#endif

        if (mHUDDir)
            mHUDDir->DrawShowing();
        if (mHUD && mHUD->Showing()) {
            START_AUTO_TIMER("hud_draw");
            mHUD->DrawShowing();
        }

        if ((TheRnd.ProcCmds() & kProcessPost) && SpotlightDrawer::Current()) {
            SpotlightDrawer::Current()->DeSelect();
        }

        SetTheWorld(nullptr);
    }
}

void WorldDir::Poll() {
    START_AUTO_TIMER("world_poll");
    if (TheWorld) {
        MILO_ASSERT(TheWorld != this, 0xC3);
        RndDir::Poll();
    } else {
        SetTheWorld(this);
        float deltas[4];
        AccumulateDeltas(deltas);
        bool b = mFirstPoll || (TheRnd.ProcCmds() != kProcessWorld);
        mFirstPoll = false;
        if (b) {
            for (int i = 0; i < 4; i++) {
                TheTaskMgr.SetDeltaTime((TaskUnits)i, mDeltaSincePoll[i]);
            }
            static Message select_camera("select_camera");
            HandleType(select_camera);
            if (mPollCamera)
                mCameraManager.PrePoll();
#ifdef WORLDDIR_DC3_TAIL
            m3DSoundMgr.Poll();
#endif
            mLightPresetMgr.Poll();
            RndDir::Poll();
            if (mPollCamera)
                mCameraManager.Poll();
#ifdef WORLDDIR_DC3_TAIL
            {
                START_AUTO_TIMER("phys_mgr_poll");
                if (mPhysicsMgr) {
                    mPhysicsMgr->Poll();
                }
            }
#endif
            RestoreDeltas(deltas);
        }
        SetTheWorld(nullptr);
    }
}

void WorldDir::Enter() {
    if (!TheWorld) {
        SetTheWorld(this);
        static DataNode &n = DataVariable("world.last_entered");
        n = this;
    }
    mLightPresetMgr.Enter();
    mCameraManager.Enter();
#ifdef WORLDDIR_DC3_TAIL
    if (mPhysicsMgr) {
        mPhysicsMgr->Enter();
    } else {
        mNeedPhysicsEnter = true;
    }
#endif
    PanelDir::Enter();
    ClearDeltas();
    mFirstPoll = true;
    TheRnd.SetProcAndLock(false);
    TheRnd.ResetProcCounter();
    if (TheWorld == this) {
        SetTheWorld(nullptr);
    }
}

void WorldDir::SyncBitmaps(bool b) {
    FOREACH (it, mBitmapOverrides) {
        it->Sync(b);
    }
}

void WorldDir::SyncMats(bool b) {
    FOREACH (it, mMatOverrides) {
        it->Sync(b);
    }
}

void WorldDir::SyncPresets(bool b) {
    FOREACH (it, mPresetOverrides) {
        it->Sync(b);
    }
}

void WorldDir::ClearDeltas() {
    for (int i = 0; i < 4; i++)
        mDeltaSincePoll[i] = 0;
}

void WorldDir::Init() {
    // Retail Init (0x824BDCB0) = register factory + SetTheWorld(0) only;
    // the glow mat is a per-instance member created in the ctor.
    REGISTER_OBJ_FACTORY(WorldDir)
    SetTheWorld(nullptr);
}

void WorldDir::AccumulateDeltas(float *deltas) {
    for (int i = 0; i < 4; i++) {
        deltas[i] = TheTaskMgr.DeltaTime((TaskUnits)i);
        mDeltaSincePoll[i] += deltas[i];
    }
}

void WorldDir::RestoreDeltas(float *f) {
    for (int i = 0; i < 4; i++) {
        mDeltaSincePoll[i] = 0;
        TheTaskMgr.SetDeltaTime((TaskUnits)i, f[i]);
    }
}

void WorldDir::SyncHUD() {
    RELEASE(mHUDDir);
    if (mShowHUD && !mHUDFilename.empty()) {
        mHUDDir = dynamic_cast<RndDir *>(
            DirLoader::LoadObjects(mHUDFilename, nullptr, nullptr)
        );
        if (mHUDDir)
            mHUDDir->Enter();
    }
}

void WorldDir::SyncHides(bool b) {
    FOREACH (it, mHideOverrides) {
        (*it)->SetShowing(!b);
    }
}

void WorldDir::SyncCamShots(bool b) {
    FOREACH (it, mCamShotOverrides) {
        (*it)->Disable(b, 1);
    }
}

#ifdef WORLDDIR_DC3_TAIL
DataNode WorldDir::OnGetPhysicsManager(const DataArray *) { return mPhysicsMgr; }
#endif
