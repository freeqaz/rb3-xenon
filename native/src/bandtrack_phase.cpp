// rb3-xenon native — W16-PX: the note-highway (track) drawing path, run.
//
// rb3-render's default run calls RunBandTrackPhase() after its render cells.
// Before this lane no native target linked VocalTrack or any bandobj track
// directory, so none of this code had ever executed on the host. The phase
// drives two layers over the SHIPPED ui/track milos:
//
//   1. the bandobj directories, loaded by the real DirLoader: TrackPanelDir and
//      the four GemTrackDirs + VocalTrackDir inside it, and a standalone
//      VocalTrackDir whose presentation math (SetRange / PitchToZ) is checked
//      against itself by two independent code paths;
//   2. the band3 VocalTrack controller over that VocalTrackDir, with a real
//      NullLocalBandUser and a GameConfig whose PlayerTrackConfigList is real,
//      so VocalTrack::SetDir runs the real VocalTrack::Init (timing data from
//      the shipped config/track_graphics.dta, the markers group, the 32-marker
//      mesh pool), then marker scrolling, note-tube configuration and the tube
//      plate pool.
//
// Every gate compares engine output against a value this file derives
// independently (the file's own header table, the shipped DTA read directly,
// the formula the source states, the shipped tube style's own property map), so
// each one can fail -- and two did, on harness errors, before they passed.
//
// The VocalTrack is REAL and so is most of what its vtable reaches (VocalPlayer,
// SongDB, the scoring graph, BandUser, GameConfig, ...; VOCALTRACK_GAME_SOURCES
// in native/CMakeLists.txt). The profile/session/prefab edge is
// bandtrack_link_stubs.cpp, where every function aborts with its own name if
// reached. The phase does NOT run VocalTrack::UpdateScrolling / Restart: those
// need a live VocalPlayer, TheGame and TheSongMgr. See
// docs/decomp/W16PX_NATIVE_TRACK_DRAWING_PATH_2026-10-06.md.

#include "bandobj/GemTrackDir.h"
#include "bandobj/NoteTube.h"
#include "bandobj/TrackPanelDir.h"
#include "bandobj/BandLabel.h"
#include "bandobj/VocalTrackDir.h"
#include "bandtrack/VocalTrack.h"
#include "beatmatch/PlayerTrackConfig.h"
#include "game/BandUser.h"
#include "game/GameConfig.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "os/System.h"
#include "rndobj/Cam.h"
#include "rndobj/Dir.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include "utl/FilePath.h"
#include "track/TrackDir.h"
#include "track/TrackWidget.h"
#include "utl/HxGuid.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern DataArray *gSystemConfig;

namespace {

typedef void (*GateFn)(const char *, bool, const char *);
GateFn gGate = nullptr;
char gBuf[512];

void Gate(const char *name, bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Gate(const char *name, bool ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gBuf, sizeof(gBuf), fmt, ap);
    va_end(ap);
    gGate(name, ok, gBuf);
}

bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

// band_keep.dta carries `(track_graphics #include track_graphics.dta)` and
// `(tour #include tour.dta)`; this driver reads only the preinit half
// (main_render.cpp StandUpConfig), so each section the phase needs is spliced
// in from the same shipped file, the same way.
DataArray *SpliceSection(const char *name, const char *file) {
    DataArray *have = SystemConfig()->FindArray(name, false);
    if (have)
        return have;
    DataArray *body = DataReadFile(file, true);
    if (!body)
        return nullptr;
    DataArray *sec = new DataArray(body->Size() + 1);
    sec->Node(0) = Symbol(name);
    for (int i = 0; i < body->Size(); i++)
        sec->Node(i + 1) = body->Node(i);
    int n = gSystemConfig->Size();
    gSystemConfig->Resize(n + 1);
    gSystemConfig->Node(n) = DataNode(sec, kDataArray);
    sec->Release();
    body->Release();
    return SystemConfig()->FindArray(name, false);
}

template <class T> int CountClass(ObjectDir *dir) {
    int n = 0;
    for (ObjDirItr<T> it(dir, true); it != nullptr; ++it)
        n++;
    return n;
}

ObjectDir *LoadMilo(ObjDirPtr<ObjectDir> &ptr, const char *path) {
    FilePath fp(path);
    ptr.LoadFile(fp, false, false, kLoadFront, false);
    return ptr;
}

// ------------------------------------------------------------ W16-QC gates --
// The track factories are registered by TrackInit() (milo_object_factories.cpp),
// which registers TrackDir and then TrackWidget, as retail's App::App does. Each
// factory must build its own class through the class's own operator new.
void TrackInitChecks() {
    Hmx::Object *d = Hmx::Object::NewObject(TrackDir::StaticClassName());
    Hmx::Object *w = Hmx::Object::NewObject(TrackWidget::StaticClassName());
    TrackDir *td = dynamic_cast<TrackDir *>(d);
    TrackWidget *tw = dynamic_cast<TrackWidget *>(w);
    Gate("bt-trackinit", td && tw && td->ClassName() == TrackDir::StaticClassName()
             && tw->ClassName() == TrackWidget::StaticClassName(),
         "TrackDir factory -> [%s], TrackWidget factory -> [%s]",
         d ? d->ClassName().Str() : "(null)", w ? w->ClassName().Str() : "(null)");
    delete d;
    delete w;
}

bool NearV(const Vector3 &a, const Vector3 &b, float eps = 1e-4f) {
    return Near(a.x, b.x, eps) && Near(a.y, b.y, eps) && Near(a.z, b.z, eps);
}

// The world position a transformable must report after a local change: the
// parent's world transform applied to the new local one.
Vector3 ExpectedWorldPos(RndTransformable *t) {
    RndTransformable *p = t->TransParent();
    if (!p)
        return t->LocalXfm().v;
    Transform out;
    Multiply(t->LocalXfm(), p->WorldXfm(), out);
    return out.v;
}

// GemTrackDir::SetTrackOffset moves only the rotater's local x, to
// -f * (slot - (numTracks - 1) / 2), keeping y and z, and the change must reach
// the rotater's world transform. SetCamPos replaces the camera's local position
// and likewise must reach its world transform. Each track is put in a 5-track
// layout at its own slot; every value is restored afterwards.
void GemTrackOffsetChecks(TrackPanelDir *tp) {
    int n = 0, offOk = 0, camN = 0, camOk = 0;
    const float f = 2.5f;
    for (ObjDirItr<GemTrackDir> it(tp, true); it != nullptr; ++it) {
        GemTrackDir *g = it;
        RndGroup *rot = g->mRotater.Ptr();
        if (!rot)
            continue;
        int slot = n++;
        int oldNum = g->mNumTracks, oldSlot = g->unk488;
        Transform oldLocal = rot->LocalXfm();
        (void)rot->WorldXfm(); // clean, so a missing dirty mark would show
        g->mNumTracks = 5;
        g->unk488 = slot;
        g->SetTrackOffset(f);
        Vector3 want = oldLocal.v;
        want.x = -f * (slot - 0.5f * (5 - 1));
        if (NearV(rot->LocalXfm().v, want) && NearV(rot->WorldXfm().v, ExpectedWorldPos(rot)))
            offOk++;
        else
            printf("  %s: rotater local (%g %g %g) want (%g %g %g)\n", g->Name(),
                   rot->LocalXfm().v.x, rot->LocalXfm().v.y, rot->LocalXfm().v.z, want.x,
                   want.y, want.z);
        rot->SetLocalXfm(oldLocal);
        g->mNumTracks = oldNum;
        g->unk488 = oldSlot;

        RndCam *cam = g->Cam();
        if (cam) {
            camN++;
            Vector3 oldPos = cam->LocalXfm().v;
            (void)cam->WorldXfm();
            Vector3 p(1.25f + slot, -3.5f, 7.0f);
            g->SetCamPos(p.x, p.y, p.z);
            if (NearV(cam->LocalXfm().v, p) && NearV(cam->WorldXfm().v, ExpectedWorldPos(cam)))
                camOk++;
            cam->SetLocalPos(oldPos);
        }
    }
    Gate("bt-gemtrack-offset", n == 4 && offOk == n,
         "%d/%d GemTrackDir rotaters at x = -f*(slot-2), y/z kept, world updated", offOk, n);
    Gate("bt-gemtrack-campos", camN > 0 && camOk == camN,
         "%d/%d GemTrackDir cameras at the set position, world updated", camOk, camN);
}

// TrackPanelDir::UpdateTimeInfo in audition mode loads ui/track/time_info.milo
// as a child dir named time_info and binds the readout group and its four
// labels from THAT dir. The panel itself holds none of them, so binding them
// from the panel finds nothing.
class AuditionGameMode : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *msg, bool warn) {
        static Symbol in_mode("in_mode");
        static Symbol audition("audition");
        if (msg->Sym(1) == in_mode)
            return DataNode(msg->Size() > 2 && msg->Sym(2) == audition);
        return Hmx::Object::Handle(msg, warn);
    }
};

// The panel's configuration object is set by the game at runtime, so a loaded
// panel has none; this one answers the only property UpdateTimeInfo reads.
class WidescreenConfig : public Hmx::Object {
public:
    virtual bool SyncProperty(DataNode &n, DataArray *prop, int i, PropOp op) {
        static Symbol aspect("aspect");
        if (op == kPropGet && i == prop->Size() - 1 && prop->Sym(i) == aspect) {
            n = Symbol("widescreen");
            return true;
        }
        return Hmx::Object::SyncProperty(n, prop, i, op);
    }
};

void TimeInfoChecks(TrackPanelDir *tp) {
    bool panelHas = tp->FindObject("time.grp", false) != nullptr;
    if (!tp->mVocalTrack) {
        Gate("bt-timeinfo", false, "panel lacks its vocal track");
        return;
    }
    WidescreenConfig *cfg = new WidescreenConfig;
    tp->mConfiguration = cfg;
    AuditionGameMode *gm = new AuditionGameMode;
    gm->SetName("gamemode", ObjectDir::Main());
    bool oldOver = tp->unk378;
    tp->unk378 = false;
    tp->UpdateTimeInfo();
    tp->unk378 = oldOver;
    RndDir *ti = dynamic_cast<RndDir *>(tp->FindObject("time_info", false));
    RndGroup *grp = tp->mTimeGrp;
    // The shipped time_info.milo has no time_section.lbl (it holds time.grp and
    // the mbt/elapsed/remaining labels), so that one binding may be null, but
    // only while the dir itself lacks the label.
    bool sectionShipped = ti && ti->FindObject("time_section.lbl", false) != nullptr;
    bool sectionOk = sectionShipped ? (tp->mTimeSection && tp->mTimeSection->Dir() == ti)
                                    : !tp->mTimeSection;
    bool labelsFromDir = ti && tp->mTimeMbt && tp->mTimeElapsed && tp->mTimeRemaining
        && tp->mTimeMbt->Dir() == ti && tp->mTimeElapsed->Dir() == ti
        && tp->mTimeRemaining->Dir() == ti && sectionOk;
    // widescreen x 10.15; z 1.5 with the vocal track showing, else 4.5.
    Vector3 wantPos(10.15f, 25.0f, tp->mVocalTrack->Showing() ? 1.5f : 4.5f);
    bool placed = grp && grp->Dir() == ti && grp->Showing()
        && NearV(grp->LocalXfm().v, wantPos);
    Gate("bt-timeinfo", !panelHas && ti && labelsFromDir && placed,
         "time_info dir %s; time.grp %s at (%.3g %.3g %.3g) (panel has its own: %s); "
         "mbt/elapsed/remaining bound from it: %s; time_section.lbl %s",
         ti ? "loaded" : "MISSING", grp ? (grp->Dir() == ti ? "from time_info" : "elsewhere") : "unbound",
         grp ? grp->LocalXfm().v.x : 0.f, grp ? grp->LocalXfm().v.y : 0.f,
         grp ? grp->LocalXfm().v.z : 0.f, panelHas ? "yes" : "no", labelsFromDir ? "yes" : "no",
         sectionShipped ? "shipped and bound" : "not in the shipped dir, binding null");
    if (grp && !placed)
        printf("  time.grp local (%g %g %g) want (%g %g %g), showing %d\n",
               grp->LocalXfm().v.x, grp->LocalXfm().v.y, grp->LocalXfm().v.z, wantPos.x,
               wantPos.y, wantPos.z, (int)grp->Showing());
    tp->mConfiguration = nullptr;
    delete cfg;
    delete gm;
}

// ---------------------------------------------------------------- layer 1 --
void TrackPanelChecks(ObjDirPtr<ObjectDir> &tpPtr) {
    printf("\n=== bandtrack: ui/track/gen/trackpanel.milo_xbox ===\n");
    ObjectDir *dir = LoadMilo(tpPtr, "ui/track/gen/trackpanel.milo_xbox");
    TrackPanelDir *tp = dynamic_cast<TrackPanelDir *>(dir);
    Gate("bt-trackpanel-load", tp != nullptr, "root '%s' [%s]",
         dir ? dir->Name() : "(null)", dir ? dir->ClassName().Str() : "-");
    if (!tp)
        return;
    // The file's own header names 4 GemTrackDir + 1 VocalTrackDir (measured off
    // the header table by rb3-milo before any of these classes had a factory).
    int gems = CountClass<GemTrackDir>(tp), vox = CountClass<VocalTrackDir>(tp);
    Gate("bt-trackpanel-tracks", gems == 4 && vox == 1,
         "%d GemTrackDir (header: 4), %d VocalTrackDir (header: 1)", gems, vox);
    GemTrackOffsetChecks(tp);
    TimeInfoChecks(tp);
}

void VocalTrackDirChecks(VocalTrackDir *vd) {
    // SetRange stores the window and places the tube-range group at middle C;
    // PitchToZ maps a pitch into the same window. Two code paths, one answer.
    vd->SetRange(48.0f, 72.0f, -1, true);
    float lo = vd->mLastMin, hi = vd->mLastMax;
    bool stored = hi > lo;
    float zLo = vd->PitchToZ(lo, true), zHi = vd->PitchToZ(hi, true);
    float zMid = vd->PitchToZ(60.0f, true);
    Gate("bt-vocaldir-range", stored && Near(zLo, vd->mPitchBottomZ) && Near(zHi, vd->mPitchTopZ),
         "SetRange(48,72) -> window [%.1f, %.1f]; PitchToZ ends %.3f/%.3f vs dir bottom/top "
         "%.3f/%.3f",
         lo, hi, zLo, zHi, vd->mPitchBottomZ, vd->mPitchTopZ);
    Gate("bt-vocaldir-middle-c", Near(zMid, vd->mMiddleCZPos, 1e-3f),
         "PitchToZ(60) %.4f vs SetRange's middle-C z %.4f", zMid, vd->mMiddleCZPos);
    // Clamped PitchToZ is monotone over the window (the tube rises with pitch).
    bool mono = true;
    float prev = vd->PitchToZ(lo, true);
    for (float p = lo + 0.5f; p <= hi; p += 0.5f) {
        float z = vd->PitchToZ(p, true);
        if ((vd->mPitchTopZ > vd->mPitchBottomZ && z < prev - 1e-5f)
            || (vd->mPitchTopZ < vd->mPitchBottomZ && z > prev + 1e-5f))
            mono = false;
        prev = z;
    }
    Gate("bt-vocaldir-monotone", mono && vd->mPitchTopZ != vd->mPitchBottomZ,
         "PitchToZ monotone across [%.0f, %.0f], z %.3f..%.3f", lo, hi, vd->mPitchBottomZ,
         vd->mPitchTopZ);
}

// ---------------------------------------------------------------- layer 2 --
void VocalTrackChecks(VocalTrackDir *vd, DataArray *tg) {
    printf("\n=== bandtrack: VocalTrack controller over the loaded VocalTrackDir ===\n");
    // A real local user on the vocal track, and the GameConfig answer
    // VocalTrack::Init asks for (GameConfig::GetTrackNum is the real body).
    // User::SetUserGuid asserts !IsLocal(): a local user owns the guid its ctor
    // generated, so the config list is keyed on that one. SetTrackType would
    // also push the change to TheNetSession (BandUser::UpdateData); there is no
    // session here, so the field is set directly -- the track reads only it.
    NullLocalBandUser *user = BandUser::NewNullLocalBandUser();
    UserGuid guid = user->GetUserGuid();
    user->mTrackType = kTrackVocals;
    // AddConfig's 4th argument is the SLOT; the track number is assigned later,
    // by Process() over the song's track layout (PlayerTrackConfigList.cpp
    // ProcessConfig -> TrackNumOfType). Vocals sit at index 4 of this layout.
    PlayerTrackConfigList *list = new PlayerTrackConfigList(1);
    list->AddConfig(guid, kTrackVocals, kDifficultyExpert, 0, false);
    std::vector<TrackType> layout;
    layout.push_back(kTrackDrum);
    layout.push_back(kTrackGuitar);
    layout.push_back(kTrackKeys);
    layout.push_back(kTrackBass);
    layout.push_back(kTrackVocals);
    const int kTrackNum = 4;
    list->Process(layout);
    GameConfig *cfg = (GameConfig *)calloc(1, sizeof(GameConfig));
    cfg->mPlayerTrackConfigList = list;
    TheGameConfig = cfg;

    VocalTrack *vt = new VocalTrack(user);
    vt->SetDir(vd); // -> VocalTrack::Init

    Gate("bt-vocaltrack-tracknum", vt->mTrackConfig.TrackNum() == kTrackNum,
         "Init set track %d via GameConfig::GetTrackNum (vocals at layout index %d)",
         vt->mTrackConfig.TrackNum(), kTrackNum);

    DataArray *svp = tg->FindArray("static_vocal_parameters");
    float wantOverlap = tg->FindFloat("lyric_overlap_ms");
    float wantShift = svp->FindArray("lyric_shift_ms")->Float(1);
    float wantQuick = svp->FindArray("lyric_shift_ms")->Float(2);
    float wantZone = svp->FindFloat("static_deploy_x_size");
    Gate("bt-vocaltrack-timing",
         Near(vt->mLyricOverlapWindowMs, wantOverlap) && Near(vt->mLyricShiftMs, wantShift)
             && Near(vt->mLyricShiftQuickMs, wantQuick)
             && Near(vt->mStaticDeployZoneXSize, wantZone),
         "overlap %.0f/%.0f shift %.0f/%.0f quick %.0f/%.0f deploy-x %.2f/%.2f (track vs "
         "track_graphics.dta)",
         vt->mLyricOverlapWindowMs, wantOverlap, vt->mLyricShiftMs, wantShift,
         vt->mLyricShiftQuickMs, wantQuick, vt->mStaticDeployZoneXSize, wantZone);

    RndGroup *markers = vt->unk1c8;
    Gate("bt-vocaltrack-marker-pool",
         markers && vt->mMeshPool.size() == 32 && vt->unk1a0.empty() && vt->unk19c == 32,
         "markers.grp %s, pool %d (Init creates 32 then ClearMarkers returns them), live %d",
         markers ? "found" : "MISSING", (int)vt->mMeshPool.size(), (int)vt->unk1a0.size());

    // Beat markers: x = width * t / window, shown only inside [from, to],
    // returned to the pool once scrolled past.
    if (markers) {
        float t[3] = { 750.0f, 1500.0f, 2250.0f };
        RndMesh *m[3];
        for (int i = 0; i < 3; i++)
            m[i] = vt->CreateMarker("beat_marker.mesh", t[i], false);
        float wantX = vt->unk78 * (t[1] / vt->unk74);
        bool placed = Near(m[1]->LocalXfm().v.x, wantX, 1e-3f) && markers->HasObject(m[1]);
        Gate("bt-vocaltrack-marker-place", placed && vt->mMeshPool.size() == 29,
             "t=%.0f -> x %.3f (want %.3f = %.1f*t/%.0f), pool %d", t[1],
             m[1]->LocalXfm().v.x, wantX, vt->unk78, vt->unk74, (int)vt->mMeshPool.size());
        vt->UpdateMarkerVisibility(1000.0f, 2000.0f);
        Gate("bt-vocaltrack-marker-visibility",
             !m[0]->Showing() && m[1]->Showing() && !m[2]->Showing(),
             "window [1000,2000]: shown %d%d%d (want 010)", m[0]->Showing(), m[1]->Showing(),
             m[2]->Showing());
        vt->InvalidateMarkers(1600.0f);
        Gate("bt-vocaltrack-marker-invalidate",
             vt->unk1a0.size() == 1 && vt->mMeshPool.size() == 31 && !markers->HasObject(m[0])
                 && markers->HasObject(m[2]),
             "after t=1600: live %d (want 1), pool %d (want 31)", (int)vt->unk1a0.size(),
             (int)vt->mMeshPool.size());
        vt->ClearMarkers();
    }

    // Note tube: each style/part picks the directory's material and group.
    NoteTube *nt = vt->mNoteTube;
    RndMat *wantBack[3] = { vd->mLeadBackMat, vd->mHarm1BackMat, vd->mHarm2BackMat };
    RndMat *wantFront[3] = { vd->mLeadFrontMat, vd->mHarm1FrontMat, vd->mHarm2FrontMat };
    RndMat *wantPhon[3] = { vd->mLeadPhonemeMat, vd->mHarm1PhonemeMat, vd->mHarm2PhonemeMat };
    // A null directory material is only acceptable where the SHIPPED tube style
    // itself says null: VocalTrackDir resolves back/front/phoneme from the
    // style's (lead|harmony_1|harmony_2)_(back|front|phoneme) properties, so
    // each null is checked against that property. Deploy materials are
    // directory members, never style-driven, so a null one is always a defect.
    int bad = 0, unresolved = 0;
    char dataNull[160] = "", missing[160] = "";
    static const char *kPrefix[3] = { "lead", "harmony_1", "harmony_2" };
    auto append = [](char *buf, size_t cap, const char *what, int part) {
        size_t l = strlen(buf);
        snprintf(buf + l, cap - l, " %s_%s", kPrefix[part], what);
    };
    auto need = [&](RndMat *m, const char *what, int part) {
        if (m) return;
        bool styleNull = false;
        if (strcmp(what, "deploy") != 0 && vd->mTubeStyle) {
            char key[64];
            snprintf(key, sizeof(key), "%s_%s", kPrefix[part], what);
            const DataNode *n = vd->mTubeStyle->Property(Symbol(key), false);
            styleNull = n && n->Type() == kDataObject && !n->GetObj();
        }
        if (styleNull) {
            append(dataNull, sizeof(dataNull), what, part);
        } else {
            unresolved++;
            append(missing, sizeof(missing), what, part);
        }
    };
    for (int part = 0; part < 3; part++) {
        vt->ConfigNoteTube(true, 8, part, false, 1.0f);
        if (nt->mBackMat != wantBack[part] || nt->mFrontMat != wantFront[part]) bad++;
        need(wantBack[part], "back", part);
        need(wantFront[part], "front", part);
        vt->ConfigNoteTube(false, 8, part, false, 1.0f);
        if (nt->mBackMat != wantPhon[part] || nt->mFrontMat != nullptr) bad++;
        need(wantPhon[part], "phoneme", part);
        vt->ConfigNoteTube(false, 8, part, true, 1.0f);
        RndMat *dep = part ? vd->mHarmDeployMat : vd->mLeadDeployMat;
        if (nt->mBackMat != dep || nt->mFrontMat != nullptr) bad++;
        need(dep, "deploy", part);
    }
    Gate("bt-vocaltrack-notetube", bad == 0 && unresolved == 0,
         "9 style/part configurations, %d mismatched, %d unresolved%s; null in the shipped "
         "tube style '%s':%s",
         bad, unresolved, missing, vd->mTubeStyle ? vd->mTubeStyle->Name() : "-",
         dataNull[0] ? dataNull : " none");

    // Tube plates: a pitched lead tube takes the first unbaked plate of the
    // part-0 front/back pools; nothing baked yet, so the pools do not grow.
    vt->ConfigNoteTube(true, 8, 0, false, 1.0f);
    size_t frontBefore = vt->mFrontTubePlates[0].size();
    vt->HookupTubePlates(nt);
    Gate("bt-vocaltrack-plates",
         nt->mFrontPlate == vt->mFrontTubePlates[0].front()
             && nt->mBackPlate == vt->mBackTubePlates[0].front()
             && vt->mFrontTubePlates[0].size() == frontBefore && frontBefore == 4,
         "lead front/back plates hooked to pool heads, pool %d (InitPlatePool: 4)",
         (int)vt->mFrontTubePlates[0].size());

    delete vt; // ClearLyrics / ClearMarkers / ClearAllTubePlates / mesh pool
    TheGameConfig = nullptr;
    printf("  VocalTrack destroyed cleanly\n");
}

} // namespace

// Returns the number of gates it ran. All verdicts go through `gate`.
int RunBandTrackPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== bandtrack phase (W16-PX): track + vocal highway ===\n");
    DataArray *tg = SpliceSection("track_graphics", "config/track_graphics.dta");
    Gate("bt-track-graphics", tg && tg->FindArray("static_vocal_parameters", false),
         "config/track_graphics.dta spliced as (track_graphics ...), %d entries",
         tg ? tg->Size() - 1 : -1);
    if (!tg)
        return 1;
    // BandUser's ctor asks DefaultDifficulty() -> (tour (default_difficulty N)).
    DataArray *tour = SpliceSection("tour", "config/tour.dta");
    DataArray *dd = tour ? tour->FindArray("default_difficulty", false) : nullptr;
    Gate("bt-tour-config", dd != nullptr, "config/tour.dta spliced as (tour ...), "
         "default_difficulty %d", dd ? dd->Int(1) : -1);
    if (!dd)
        return 1;

    TrackInitChecks();
    ObjDirPtr<ObjectDir> tpPtr;
    TrackPanelChecks(tpPtr);

    printf("\n=== bandtrack: ui/track/gen/vocals.milo_xbox ===\n");
    ObjDirPtr<ObjectDir> vPtr;
    ObjectDir *vdir = LoadMilo(vPtr, "ui/track/gen/vocals.milo_xbox");
    VocalTrackDir *vd = dynamic_cast<VocalTrackDir *>(vdir);
    Gate("bt-vocaldir-load", vd != nullptr, "root '%s' [%s]", vdir ? vdir->Name() : "(null)",
         vdir ? vdir->ClassName().Str() : "-");
    if (!vd)
        return 1;
    VocalTrackDirChecks(vd);
    VocalTrackChecks(vd, tg);
    return 0;
}
