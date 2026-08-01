#include "bandobj/TrackPanelDirBase.h"
#include "bandobj/BandCrowdMeter.h"
#include "bandobj/BandTrack.h"
#include "bandobj/GemTrackDir.h"
#include "bandobj/TrackPanelInterface.h"
#include "decomp.h"
#include "obj/DataFunc.h"
#include "obj/Msg.h"
#include "obj/Task.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include "utl/Loader.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"
#include "utl/TimeConversion.h"
#include <cmath>

INIT_REVS(TrackPanelDirBase);

// Base TrackPanelDirBase stores mGemTracks as ObjVector<ObjPtr<RndDir> > (to keep
// layout without pulling GemTrackDir.h into the widely-included header).
// GemTrackDir IS-A TrackDir IS-A RndDir, so a stored RndDir* is a GemTrackDir.
static inline GemTrackDir *AsGemTrack(RndDir *d) { return static_cast<GemTrackDir *>(d); }

// TrackPanelDirBase::TrackPanelDirBase() -- mApplauseMeter.
//
// Retail's ctor (Ghidra decompile of va 0x82358c20) builds mApplauseMeter with
// THREE inline field stores (mOwner@+4, mObject@+8 = null, vtable@+0, in that
// order) and no AddRef, while mConfiguration one member earlier keeps a real
// `bl` to the out-of-line ObjPtr<Hmx::Object> ctor. So retail's /Ob2 inline
// decision for the ObjPtr ctor is PER-INSTANTIATION here, and this explicit
// specialization is how a TU expresses that without touching obj/Object.h:
// ObjPtr<RndDir>'s two-arg ctor gets a visible inline body (identical to the
// primary template's in obj/ObjPtr_p.h -- with ptr = 0 the AddRef branch folds
// away, leaving exactly the three stores), while ObjPtr<Hmx::Object> still
// instantiates the out-of-line template body and keeps its `bl`.
//
// NOTE the body must be NON-EMPTY to match. Measured in this TU (lane NCCC
// f183): binding an owner-only ObjPtr ctor whose body is `{}` puts the vtable
// store FIRST in the inlined sequence -- MSVC folds the empty ctor's vptr init
// into the enclosing ctor's vptr-init group, so the scheduler hoists the
// `lis/addi` of ??_7ObjPtr@VRndDir@@@@6B@ to the top of the block and the
// owner-load chain starts four slots late (18 mismatches, 95.0%). With a body
// present the vptr store stays last, as retail has it (3 mismatches, 99.4%).
// The body's CONTENT is irrelevant -- a body reading no members measured
// identically -- so this is about the ctor being non-empty, not about AddRef.
template <>
inline ObjPtr<RndDir>::ObjPtr(Hmx::Object *owner, RndDir *ptr)
    : ObjRefConcrete<RndDir>(owner, ptr) {
    if (mObject)
        mObject->AddRef(this);
}

bool gShowHUD = true;

DataNode ToggleHUD(DataArray *da) {
    gShowHUD = gShowHUD == 0;
    return gShowHUD;
}

TrackPanelDirBase::TrackPanelDirBase()
    : mViewTimeEasy(0), mViewTimeExpert(0), mNetTrackAlpha(0), mPulseOffset(0),
      mConfiguration(this, 0), mConfigurableObjects(this, kObjListNoNull), mTracks(this),
      mGemTracks(this), unk224(0), mTrackPanel(0), mApplauseMeter(this, 0),
      mBandLogoRival(0), mBandLogo(0), mPerformanceMode(0), mDoubleSpeedActive(0),
      mIndependentTrackSpeeds(0) {
    // Retail's ctor (Ghidra decompile of va 0x82358c20) never calls
    // DataRegisterFunc("toggle_hud", ToggleHUD) -- unlike the rb3-Wii dev
    // decomp, which still has it (source `../rb3` TrackPanelDirBase.cpp:35).
    // ToggleHUD/gShowHUD stay defined above; only this dev-only registration
    // call was stripped for the Xbox 360 retail build.
    if (SystemConfig()->FindArray("track_graphics", false)) {
        if (SystemConfig("track_graphics")->FindArray("pulse_offset", false)) {
            mPulseOffset = SystemConfig("track_graphics")->FindFloat("pulse_offset");
        }
    }
}

SAVE_OBJ(TrackPanelDirBase, 0x3F)

float TrackPanelDirBase::GetPulseAnimStartDelay(bool b) const {
    float beat =
        MsToBeat(TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f + 16.70000076293945f);
    if (b)
        beat = beat + mPulseOffset;
    return std::floor(beat) + 1.0f - beat;
}

void TrackPanelDirBase::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(0, 0);
    bs.PushRev(packRevs(gAltRev, gRev), this);
    PanelDir::PreLoad(bs);
}

DECOMP_FORCEACTIVE(
    TrackPanelDirBase,
    "non-empty list passed to TrackPanelDirBase::GetConfigList, will be cleared"
)

void TrackPanelDirBase::PostLoad(BinStream &bs) {
    PanelDir::PostLoad(bs);
    int revs = bs.PopRev(this);
    gRev = getHmxRev(revs);
    gAltRev = getAltRev(revs);
    if (!IsProxy()) {
        bs >> mViewTimeEasy;
        bs >> mViewTimeExpert;
        bs >> mNetTrackAlpha;
        bs >> mConfigurableObjects;
    }
}

BEGIN_COPYS(TrackPanelDirBase)
    COPY_SUPERCLASS(PanelDir)
    CREATE_COPY(TrackPanelDirBase)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mViewTimeEasy)
        COPY_MEMBER(mViewTimeExpert)
        COPY_MEMBER(mNetTrackAlpha)
        COPY_MEMBER(mConfigurableObjects)
    END_COPYING_MEMBERS
END_COPYS

void TrackPanelDirBase::SetConfiguration(Hmx::Object *o, bool b) {
    if (o) {
        static Message apply("apply", b);
        apply[0] = b;
        mConfiguration = o;
        o->Handle(apply, true);
    }
    if (!mPerformanceMode)
        SetShowing(gShowHUD);
}

void TrackPanelDirBase::ReapplyConfiguration(bool b) {
    if (mConfiguration) {
        static Message apply("apply", b);
        apply[0] = b;
        mConfiguration->Handle(apply, true);
        if (!mPerformanceMode)
            SetShowing(gShowHUD);
    }
}

bool TrackPanelDirBase::ModifierActive(Symbol s) {
    Hmx::Object *gamemodeobj = FindObject("gamemode", true);
    if (gamemodeobj) {
        if (gamemodeobj->Property("always_show_hud", true)->Int() == 0) {
            if (gamemodeobj->Property("is_practice", true)->Int() != 0)
                return false;
            else {
                Hmx::Object *modmgr = FindObject("modifier_mgr", true);
                if (modmgr) {
                    static Message active("is_modifier_active", "");
                    active[0] = s;
                    int ret = modmgr->Handle(active, true).Int();
                    if (ret != 0)
                        return true;
                }
            }
        }
    }
    return false;
}

void TrackPanelDirBase::Enter() {
    PanelDir::Enter();
    if (LOADMGR_EDITMODE) {
        if (Find<EventTrigger>("reset_all.trig", false)) {
            ConfigureTracks(true);
            Reset();
        }
    }
}

float GetTrackViewTime(const Symbol &s1, Symbol s2) {
    DataArray *cfg = SystemConfig("objects", "view_times", s1);
    return cfg->FindFloat(s2);
}

void TrackPanelDirBase::UpdateTrackSpeed() {
    if (!mTrackPanel || !mTrackPanel->ShouldUpdateScrollSpeed())
        return;
    else {
        mDoubleSpeedActive = ModifierActive(mod_doublespeed);
        mIndependentTrackSpeeds = ModifierActive(mod_independent_track_speeds);
        float f1 = mDoubleSpeedActive ? 1.5f : 1.0f;
        if (mIndependentTrackSpeeds) {
            for (int i = 0; i < mGemTracks.size(); i++) {
                GemTrackDir *tdir = AsGemTrack(mGemTracks[i]);
                TrackInstrument inst = tdir->GetInstrument();
                Symbol diffsym = tdir->GetPlayerDifficultySym();
                bool ok = tdir->InUse();
                if (ok) ok = (inst >= kInstGuitar);
                if (ok) ok = (diffsym != gNullStr);
                if (ok) {
                    Symbol instsym = tdir->GetInstrumentSymbol();
                    float viewtime = GetTrackViewTime(instsym, diffsym);
                    tdir->SetScrollSpeed(viewtime / f1);
                }
            }
        } else {
            float f15 = 0;
            float f13 = f15;
            float f11 = f15;
            float f14 = f15;
            for (int i = 0; i < mGemTracks.size(); i++) {
                GemTrackDir *tdir = AsGemTrack(mGemTracks[i]);
                TrackInstrument inst = tdir->GetInstrument();
                Symbol diffsym = tdir->GetPlayerDifficultySym();
                bool ok = tdir->InUse();
                if (ok) ok = (inst >= kInstGuitar);
                if (ok) ok = (diffsym != gNullStr);
                if (ok) {
                    Symbol instsym = tdir->GetInstrumentSymbol();
                    float viewtime = GetTrackViewTime(instsym, diffsym);
                    f15 += 1.0f;
                    f11 += viewtime;
                    if (!tdir->HasNetPlayer()) {
                        f13 += 1.0f;
                        f14 += viewtime;
                    }
                }
            }
            if (f15 > 0) {
                float speed;
                if (f13 == 0) {
                    speed = f11 / f15;
                } else {
                    speed = f14 / f13;
                }
                speed /= f1;
                for (int i = 0; i < mGemTracks.size(); i++) {
                    AsGemTrack(mGemTracks[i])->SetScrollSpeed(speed);
                }
            }
        }
    }
}

void TrackPanelDirBase::SetShowing(bool b) {
    Find<RndGroup>("draw_order.grp", true)->SetShowing(b);
}

void TrackPanelDirBase::UpdateJoinInProgress(bool b1, bool b2) {
    GetCrowdMeter()->UpdateJoinInProgress(b1, b2);
}

void TrackPanelDirBase::FailedJoinInProgress() {
    GetCrowdMeter()->FailedJoinInProgress();
}

void TrackPanelDirBase::ToggleSurface() {
    for (int i = 0; i < mGemTracks.size(); i++) {
        RndMesh *d = AsGemTrack(mGemTracks[i])->mSurfaceMesh;
        d->SetShowing(!d->Showing());
    }
}

void TrackPanelDirBase::ToggleNowbar() {
    for (int i = 0; i < mGemTracks.size(); i++) {
        RndGroup *grp = AsGemTrack(mGemTracks[i])->Find<RndGroup>("now_bar.grp", true);
        grp->SetShowing(!grp->Showing());
    }
}

void TrackPanelDirBase::SetPlayerLocal(BandTrack *track) {
    track->SetPlayerLocal(mNetTrackAlpha);
}

void TrackPanelDirBase::CodaSuccess() {
    MILO_NOTIFY_ONCE("calling non-h2h coda success in h2h mode");
}

bool TrackPanelDirBase::ReservedVocalPlayerSlot(int i) {
    if (mTrackPanel)
        return mTrackPanel->SlotReservedForVocals(i);
    else
        return i == 2;
}

BandTrack *TrackPanelDirBase::GetBandTrackInSlot(int slot) {
    MILO_ASSERT_RANGE(slot, 0, mTracks.size(), 0x180);
    return mTracks[slot];
}

BEGIN_HANDLERS(TrackPanelDirBase)
    HANDLE_EXPR(gem_tracks_size, (int)mGemTracks.size())
    HANDLE_EXPR(
        get_gem_track,
        _msg->Int(2) < mGemTracks.size() ? mGemTracks[_msg->Int(2)].Ptr()
                                         : (RndDir *)0
    )
    HANDLE_ACTION(configure_tracks, ConfigureTracks(1))
    HANDLE_ACTION(
        set_configuration,
        SetConfiguration(_msg->Obj<Hmx::Object>(2), _msg->Size() > 3 ? _msg->Int(3) : 1)
    )
    HANDLE_ACTION(enter, Enter())
    HANDLE_ACTION(reset, Reset())
    HANDLE_ACTION(set_multiplier, SetMultiplier(_msg->Int(2), false))
    HANDLE_ACTION(play_intro, PlayIntro())
    HANDLE_ACTION(hide_score, HideScore())
    HANDLE_ACTION(game_over, GameOver())
    HANDLE_ACTION(coda, Coda())
    HANDLE_ACTION(set_showing, SetShowing(_msg->Int(2)))
    HANDLE_EXPR(showing, Showing())
    HANDLE_ACTION(toggle_surface, ToggleSurface())
    HANDLE_ACTION(toggle_nowbar, ToggleNowbar())
    HANDLE(foreach_configurable_object, DataForEachConfigObj)
    HANDLE_SUPERCLASS(PanelDir)
    HANDLE_CHECK(0x1A7)
END_HANDLERS

void TrackPanelDirBase::PlayIntro() {}
void TrackPanelDirBase::HideScore() {}
void TrackPanelDirBase::GameOver() {}

BEGIN_PROPSYNCS(TrackPanelDirBase)
    static Symbol view_time_easy("view_time_easy");
    SYNC_PROP(view_time_easy, mViewTimeEasy)
    static Symbol view_time_expert("view_time_expert");
    SYNC_PROP(view_time_expert, mViewTimeExpert)
    static Symbol net_track_alpha("net_track_alpha");
    SYNC_PROP(net_track_alpha, mNetTrackAlpha)
    static Symbol configuration("configuration");
    SYNC_PROP(configuration, mConfiguration)
    static Symbol configurable_objects("configurable_objects");
    SYNC_PROP(configurable_objects, mConfigurableObjects)
    SYNC_SUPERCLASS(PanelDir)
END_PROPSYNCS

DataNode TrackPanelDirBase::DataForEachConfigObj(DataArray *da) {
    DataNode *var = da->Var(2);
    DataNode dvar(*var);
    for (ObjPtrList<RndTransformable>::iterator it = mConfigurableObjects.begin();
         it != mConfigurableObjects.end();
         ++it) {
        *var = *it;
        for (int i = 3; i < da->Size(); i++) {
            da->Command(i)->Execute();
        }
    }
    *var = dvar;
    return 0;
}
