// rb3-app native driver (lanes A1, A7, 2026-11-22). Native only.
//
// Retail main (src/Main.cpp, 0x82272E68) is `App app(argc, argv); app.Run();`.
// This driver replaces Main.cpp in rb3-app (Main.cpp stays the X360 TU,
// untouched) and does the same two calls, bounded:
//
//   1. the data dir: retail runs with game:\ at the disc root, and SystemInit
//      opens gen/main_xbox.hdr relative to it. Natively the disc root is
//      argv[1], else $RB3_ASSETS; the driver chdir()s there (rc 2 if absent).
//   2. headless: MILO_HEADLESS=1 is forced for the engine (no window, no
//      swapchain; the engine still renders offscreen, and MILO_SCREENSHOT_DIR /
//      MILO_SCREENSHOT_FRAMES capture frames) unless the caller set it.
//   3. App::App(argc, argv) with argv[0] and any arguments after the data dir.
//      Every init step prints its own gate (rb3_app_gates.cpp).
//   4. By default: print `RESULT: APP CONSTRUCTED` and _exit(0) right after
//      the constructor (the A1 milestone; RB3_APP_CONSTRUCT_ONLY=1 still asks
//      for it explicitly and wins over RB3_APP_RUN).
//      RB3_APP_RUN=1: App::Run(), the retail frame loop (App::RunWithoutDebugging,
//      `while (true)`; App.cpp's HX_NATIVE arm only drops the SEH trampoline).
//      Opt-in while the loop does not yet survive its first frame natively:
//      the splash's AnimTask outlives its anim through the ~ObjectDir cascade
//      (Phase 0 nulls refs without the owner's Replace), so frame 1 polls a
//      task whose mAnim is null.
//
// The loop has no exit, so the bound sits OUTSIDE it, at the one call every
// frame starts with: SystemPoll(false) is -Wl,--wrap'ed (native/CMakeLists.txt;
// App.cpp:76 is its only caller in src/). The wrapper counts frames, logs each
// UI screen change, and ends the process at a frame boundary:
//   RB3_APP_FRAMES=N   (default 300) after N whole frames:
//                      `RESULT: APP RAN N FRAMES screen=<name>`, rc 0.
//   RB3_APP_SECONDS=S  (default 120) wall clock since App::Run, checked at each
//                      frame start: `RESULT: APP RAN N FRAMES (wall-clock cap)`,
//                      rc 4. A frame still running S+15 s after App::Run is
//                      killed by SIGALRM: `RESULT: APP STALLED IN FRAME N`, rc 5.
// No teardown runs (_exit), as at the construct-only exit: ~App is not reached
// in retail either.
// A gate that checks state after an init step (rb3_app_gates.cpp) and finds it
// wrong prints `[FAIL]`, then `RESULT: GATE FAILED (<what>)`, rc 7.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <csignal>
#include <unistd.h>
#include "App.h"
#include "ui/UI.h"
#include "ui/UIScreen.h"
#include "ui/UIPanel.h"
#include "ui/UIComponent.h"
#include "ui/PanelDir.h"
#include "ui/UIList.h"
#include "meta_band/OvershellPanel.h"
#include "meta_band/OvershellSlot.h"
#include "meta_band/OvershellSlotState.h"
#include "tour/Tour.h"
#include "tour/TourProgress.h"
#include "meta_band/ModifierMgr.h"
#include "meta_band/MetaPerformer.h"
#include "game/Game.h"
#include "game/GemPlayer.h"
#include "bandtrack/TrackPanel.h"
#include "bandobj/TrackPanelDirBase.h"
#include "bandobj/GemTrackDir.h"
#include "beatmatch/BeatMatcher.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/SongSortMgr.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/BandProfile.h"
#include "meta_band/SavedSetlist.h"
#include "meta_band/GameplayOptions.h"
#include "game/BandUser.h"
#include "bandtrack/GemManager.h"
#include "bandtrack/NowBar.h"
#include "beatmatch/MasterAudio.h"
#include "rndobj/Rnd.h"
#include "synth/Synth.h"
#include "synth/Faders.h"
#include "synth/FxSend.h"
#include "obj/Task.h"

void rb3_app_check_ui_sink(); // rb3_app_gates.cpp
void rb3_app_check_tex_movie(); // rb3_app_gates.cpp
void rb3_app_check_net_cache_offline(); // rb3_app_gates.cpp
void rb3_app_check_tex_gpu_cleanup(); // rb3_app_gates.cpp
void rb3_app_check_clip_driver_prune(); // rb3_app_gates.cpp
void rb3_app_check_music_library_exit(); // rb3_app_gates.cpp
void rb3_app_check_object_copy(); // rb3_app_gates.cpp
void rb3_app_check_pre_clear_unregister(); // rb3_app_gates.cpp
void rb3_app_check_offline_game_start(); // rb3_app_gates.cpp
void rb3_app_check_trans_parent_replace(); // rb3_app_gates.cpp
void rb3_app_check_sphere_base_replace(); // rb3_app_gates.cpp
void rb3_app_check_owner_ptr_replace(); // rb3_app_gates.cpp
void rb3_app_check_bones_typeprops_replace(); // rb3_app_gates.cpp
void rb3_app_check_reserved_vocal_slot(); // rb3_app_gates.cpp
void rb3_app_check_spotlight_disk(); // rb3_app_gates.cpp
void rb3_app_check_glyph_atlas_rgb(); // rb3_app_gates.cpp
void rb3_app_check_album_art_path(); // rb3_app_gates.cpp
void rb3_app_check_lyric_line_color_kept(); // rb3_app_gates.cpp
void rb3_app_check_environ_lr_fade(); // rb3_app_gates.cpp
void rb3_app_check_world_end_depth_clear(); // rb3_app_gates.cpp
void rb3_app_check_music_library_on_exit(); // rb3_app_gates.cpp
void rb3_app_check_clip_group_replace(); // rb3_app_gates.cpp
void rb3_app_check_clip_transition_drop(); // rb3_app_gates.cpp
void rb3_app_check_drum_venue_lookup(); // rb3_app_gates.cpp
void rb3_app_check_replace_refs_shifted_entry(); // rb3_app_gates.cpp
void rb3_app_check_bones_ref_dying_compare(); // rb3_app_gates.cpp
void rb3_app_check_typeprops_remove_one(); // rb3_app_gates.cpp
void rb3_app_check_poll_sorter_search_id(); // rb3_app_gates.cpp
void rb3_app_check_cube_tex_gpu_cleanup(); // rb3_app_gates.cpp
void rb3_app_check_session_settings_unranked(); // rb3_app_gates.cpp
void rb3_app_check_compress_queue_drain(); // rb3_app_gates.cpp
void rb3_app_check_campaign_screen_push(); // rb3_app_gates.cpp
void rb3_app_check_nonull_list_replace_drain(); // rb3_app_gates.cpp
void rb3_app_check_bone_merge_repoint(); // rb3_app_gates.cpp
void rb3_app_check_credits_auto_scroll(); // rb3_app_gates.cpp
void rb3_app_check_postproc_noise_map(); // rb3_app_gates.cpp
void rb3_app_check_retail_post_chain(); // rb3_app_gates.cpp
void rb3_app_check_retail_post_noise(); // rb3_app_gates.cpp
void rb3_app_check_gamma_space_shading(); // rb3_app_gates.cpp
void rb3_app_check_retail_light_model(); // rb3_app_gates.cpp
void rb3_app_check_unnamed_mesh_not_text(); // rb3_app_gates.cpp
void rb3_app_check_cam_screen_y_down(); // rb3_app_gates.cpp
void rb3_app_check_crowd_force_3d(); // rb3_app_gates.cpp
void SetGameMarksTextGlyphMeshes(bool marks); // milo-native-engine platform/MeshGpuCache.h
void SetGameGlyphAtlasesKeepRGB(bool keepRGB); // milo-native-engine platform/MeshGpuCache.h

extern "C" void rb3_app_gate(const char *step);
void rb3_app_determinism_init(); // rb3_app_determinism.cpp (RB3_APP_DETERMINISTIC)
void rb3_app_determinism_frame(int frame); // rb3_app_determinism.cpp

namespace {
int gFramesStarted = 0; // SystemPoll calls since App::Run (frame N starts at call N)
int gFrameCap = 300;
double gSecondsCap = 120.0;
bool gInRun = false;
timespec gRunStart;
const UIScreen *gLastScreen = nullptr;
bool gLastTransition = false;

double SecondsSinceRun() {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - gRunStart.tv_sec) + (now.tv_nsec - gRunStart.tv_nsec) * 1e-9;
}

const char *ScreenName(const UIScreen *s) {
    if (!s) return "(none)";
    const char *n = s->Name();
    return n && *n ? n : "(unnamed)";
}

void Finish(int framesDone, const char *why, int rc) {
    UIScreen *cur = TheUI ? TheUI->CurrentScreen() : nullptr;
    UIScreen *next = TheUI ? TheUI->TransitionScreen() : nullptr;
    printf("RESULT: APP RAN %d FRAMES screen=%s transition=%s%s (%.1f s)\n", framesDone,
           ScreenName(cur), TheUI && TheUI->InTransition() ? ScreenName(next) : "none", why,
           SecondsSinceRun());
    fflush(stdout);
    fflush(stderr);
    _exit(rc);
}

void OnStall(int) {
    // async-signal-safe: write() only
    char buf[128];
    int n = snprintf(buf, sizeof buf, "RESULT: APP STALLED IN FRAME %d\n", gFramesStarted);
    if (n > 0) (void)!write(1, buf, (size_t)n);
    _exit(5);
}

// The selected entry of a focused UIList, for the focus log ("" for any other component).
const char *ListSel(UIComponent *c, char *buf, size_t n) {
    UIList *l = dynamic_cast<UIList *>(c);
    buf[0] = 0;
    if (l && l->NumProviderData() > 0)
        snprintf(buf, n, " sel=%d:%s", l->SelectedPos(), l->SelectedSym(false).Str());
    return buf;
}

// RB3_APP_FOCUS_LOG=1: one `rb3-app: frame N focus=<panel>/<component>` line whenever the
// current screen's focused panel or that panel's focused component changes, so a menu walk can
// see where its d-pad presses land (the det hash does not cover focus). Read-only: FocusPanel
// and UIPanel::FocusComponent only read members.
void LogFocus(UIScreen *cur) {
    static const bool on = [] {
        const char *v = getenv("RB3_APP_FOCUS_LOG");
        return v && v[0] && strcmp(v, "0") != 0;
    }();
    if (!on) return;
    static const void *lastPanel = nullptr, *lastComp = nullptr;
    UIPanel *panel = cur ? cur->FocusPanel() : nullptr;
    UIComponent *comp = panel ? panel->FocusComponent() : nullptr;
    char sel[96];
    ListSel(comp, sel, sizeof sel);
    static char lastSel[96];
    if (panel != lastPanel || comp != lastComp || strcmp(sel, lastSel)) {
        lastPanel = panel;
        lastComp = comp;
        strcpy(lastSel, sel);
        printf("rb3-app: frame %d focus=%s/%s%s\n", gFramesStarted,
               panel && panel->Name() ? panel->Name() : "(none)",
               comp && comp->Name() ? comp->Name() : "(none)", sel);
        fflush(stdout);
    }
    // The overshell's slots (an overlay, not a screen): state id, view and focused component.
    OvershellPanel *os = ObjectDir::Main()
        ? ObjectDir::Main()->Find<OvershellPanel>("overshell", false) : nullptr;
    for (int i = 0; i < 4; i++) {
        static int lastId[4] = {-2, -2, -2, -2};
        static const void *lastOsComp[4];
        static const char *lastView[4];
        static char lastOsSel[4][96];
        char osel[96];
        OvershellSlot *slot = os ? os->GetSlot(i) : nullptr;
        int id = slot && slot->mState ? (int)slot->mState->GetStateID() : -1;
        const char *view = slot ? slot->GetCurrentView().Str() : nullptr;
        PanelDir *pd = slot ? dynamic_cast<PanelDir *>(slot->DataDir()) : nullptr;
        UIComponent *oc = pd ? pd->FocusComponent() : nullptr;
        ListSel(oc, osel, sizeof osel);
        if (id == lastId[i] && oc == lastOsComp[i] && view == lastView[i]
            && !strcmp(osel, lastOsSel[i]))
            continue;
        strcpy(lastOsSel[i], osel);
        lastId[i] = id;
        lastOsComp[i] = oc;
        lastView[i] = view;
        printf("rb3-app: frame %d overshell%d state=%d view=%s focus=%s%s\n", gFramesStarted, i,
               id, view && *view ? view : "(none)", oc && oc->Name() ? oc->Name() : "(none)", osel);
        fflush(stdout);
    }
}

// RB3_APP_FOCUS_LOG=1 also logs the tour state on change: `rb3-app: frame N tour=<desc>
// on=<0|1> gigs=<completed>/<total> cur=<gig> stars=<total> won=<last quest won>`, so a
// tour walk can check progression across gigs and boots. Read-only accessors only.
void LogTour() {
    static const bool on = [] {
        const char *v = getenv("RB3_APP_FOCUS_LOG");
        return v && v[0] && strcmp(v, "0") != 0;
    }();
    if (!on || !TheTour) return;
    TourProgress *p = TheTour->GetTourProgress();
    char buf[256];
    if (!p)
        snprintf(buf, sizeof buf, "tour=(no progress)");
    else {
        Symbol desc = p->GetTourDesc();
        bool hasDesc = desc != "" && TheTour->GetTourDesc(desc);
        snprintf(buf, sizeof buf, "tour=%s on=%d gigs=%d/%d cur=%d stars=%d won=%d",
                 desc.Str(), p->IsOnTour(), p->GetNumCompletedGigs(),
                 hasDesc ? p->GetNumTotalGigs() : -1, p->GetCurrentGigNum(), p->GetNumStars(),
                 (int)p->GetWonQuest());
    }
    static char last[256];
    if (strcmp(buf, last)) {
        strcpy(last, buf);
        printf("rb3-app: frame %d %s\n", gFramesStarted, buf);
        fflush(stdout);
    }
}

// RB3_APP_SONGSEL_LOG=1 (lane songselwalk): one `rb3-app: frame N songsel sort=<short name>
// n=<rows> hl=<ix>:<node type>:<token> making=<0|1> setlist=<song,...> filter=<task filter>`
// line whenever any of those change while the music library is on screen, and one `songsel list
// <sort> <n>: T:token ...` dump of the current sort's whole row list whenever the sort, its row
// count or the filter changes, so a walk can check sort order, filters and setlist contents
// (the focus log's list rows carry no DataSymbol here). Read-only: every call only reads.
void LogSongSel() {
    static const bool on = [] {
        const char *v = getenv("RB3_APP_SONGSEL_LOG");
        return v && v[0] && strcmp(v, "0") != 0;
    }();
    if (!on || !TheMusicLibrary || !TheSongSortMgr || !TheMusicLibrary->unk40) return;
    NodeSort *sort = TheMusicLibrary->GetCurrentSort();
    if (!sort) return;
    static const char kType[] = "-CHUSFLT";
    String filter;
    TheMusicLibrary->mTask.GetSongFilterAsString(filter);
    int n = (int)sort->mList.size();
    int hl = TheMusicLibrary->mCurrentHighlightIndex;
    SortNode *node = hl >= 0 && hl < n ? sort->mList[hl] : nullptr;
    char set[512];
    int k = 0;
    set[0] = 0;
    const std::vector<int> &sl = TheMusicLibrary->mSetlist;
    for (size_t i = 0; i < sl.size() && k < (int)sizeof set - 64; i++)
        k += snprintf(set + k, sizeof set - k, "%s%s", i ? "," : "",
                      TheSongMgr.GetShortNameFromSongID(sl[i], false).Str());
    // The primary profile's saved setlists: title (song count).
    char saved[512];
    int m = snprintf(saved, sizeof saved, "-");
    if (BandProfile *prof = TheProfileMgr.GetPrimaryProfile()) {
        const std::vector<LocalSavedSetlist *> &ss = prof->GetSavedSetlists();
        m = snprintf(saved, sizeof saved, "%d", (int)ss.size());
        for (size_t i = 0; i < ss.size() && m < (int)sizeof saved - 96; i++)
            m += snprintf(saved + m, sizeof saved - m, "%s\"%s\"(%d)", i ? "," : ":",
                          ss[i]->GetTitle(), (int)ss[i]->mSongs.size());
    }
    char buf[1536];
    snprintf(buf, sizeof buf,
             "songsel sort=%s n=%d hl=%d:%c:%s making=%d setlist=%s filter=%s saved=%s",
             sort->GetName().Str(), n, hl, node ? kType[node->GetType() & 7] : '-',
             node ? node->GetToken().Str() : "-", TheMusicLibrary->GetMakingSetlist(false), set,
             filter.c_str(), saved);
    static char last[1536];
    static std::string lastList;
    if (strcmp(buf, last)) {
        strcpy(last, buf);
        printf("rb3-app: frame %d %s\n", gFramesStarted, buf);
        char key[256];
        snprintf(key, sizeof key, "%s|%d|%s", sort->GetName().Str(), n, filter.c_str());
        if (lastList != key) {
            lastList = key;
            printf("rb3-app: frame %d songsel list %s %d:", gFramesStarted, sort->GetName().Str(), n);
            for (int i = 0; i < n; i++)
                printf(" %c:%s", kType[sort->mList[i]->GetType() & 7],
                       sort->mList[i]->GetToken().Str());
            printf("\n");
        }
        fflush(stdout);
    }
}

// RB3_APP_MOD_LOG=1: one `rb3-app: frame N mods ...` line whenever the modifier state or any
// value a modifier sets changes, so a walk can check each modifier's effect (lane modwalk):
// active= the modifiers ModifierMgr::IsModifierActive reports; nofail= MetaPerformer::
// IsNoFailActive; fills= Game::mDrumFillsMod; hud= TrackPanelDirBase mPerformanceMode (nohud),
// mDoubleSpeedActive, mIndependentTrackSpeeds; view= each in-use gem track's instrument and
// ViewTimeSeconds; p= each GemPlayer's miss-sound flag (unk408) and BeatMatcher fill logic.
// Read-only: every call below only reads members (IsModifierActive(Modifier*) writes only when
// a modifier is locked, and IsModifierUnlocked is always true in retail).
void LogModifiers() {
    static const bool on = [] {
        const char *v = getenv("RB3_APP_MOD_LOG");
        return v && v[0] && strcmp(v, "0") != 0;
    }();
    if (!on || !TheModifierMgr) return;
    char buf[1024];
    int n = snprintf(buf, sizeof buf, "active=");
    for (Modifier *m : TheModifierMgr->mModifiers)
        if (TheModifierMgr->IsModifierActive(m))
            n += snprintf(buf + n, sizeof buf - n, "%s,", m->mData->Sym(0).Str());
    if (MetaPerformer::Current())
        n += snprintf(buf + n, sizeof buf - n, " nofail=%d", MetaPerformer::Current()->IsNoFailActive());
    if (TheGame) {
        n += snprintf(buf + n, sizeof buf - n, " fills=%d p=", TheGame->DrumFillsMod());
        for (Player *pl : TheGame->mAllActivePlayers) {
            GemPlayer *gp = dynamic_cast<GemPlayer *>(pl);
            if (gp && n < (int)sizeof buf - 64)
                n += snprintf(buf + n, sizeof buf - n, "miss%d/fill%d,", gp->unk408,
                              gp->mMatcher ? (int)gp->mMatcher->mFillLogic : -1);
        }
    }
    TrackPanelDirBase *tpd = TheTrackPanel ? TheTrackPanel->mTrackPanelDir : nullptr;
    if (tpd) {
        n += snprintf(buf + n, sizeof buf - n, " hud=perf%d/dbl%d/indep%d view=",
                      tpd->mPerformanceMode, tpd->mDoubleSpeedActive, tpd->mIndependentTrackSpeeds);
        for (int i = 0; i < (int)tpd->mGemTracks.size() && n < (int)sizeof buf - 64; i++) {
            GemTrackDir *g = tpd->mGemTracks[i];
            if (g && g->InUse())
                n += snprintf(buf + n, sizeof buf - n, "%s:%.3f,", g->GetInstrumentSymbol().Str(),
                              g->ViewTimeSeconds());
        }
    }
    static char last[1024];
    if (strcmp(buf, last)) {
        strcpy(last, buf);
        printf("rb3-app: frame %d mods %s\n", gFramesStarted, buf);
        fflush(stdout);
    }
}

// RB3_APP_OPTIONS_LOG=1 (lane optwalk): one `rb3-app: frame N opts ...` line whenever a global
// option, its pushed effect, a profile's gameplay options or an in-game timing input changes, so
// a walk can check calibration, volumes, overscan, dolby, bass boost and lefty flip end to end:
// alag/vlag= ProfileMgr GetExcessAudioLag/GetExcessVideoLag, sync/s2t= the raw saved fields,
// vol= bg/fg/fx/crowd/cue/chat slider indices, fade= the dB the synth faders hold
// (background_music_level, crowd_level, sfx_level, instrument_level) and bass_boost.send's wet
// gain, shrink= TheRnd's safe-area flag, pK= profile K's lefty/vocal style/vocal volume, and in
// game av= TaskMgr AV offset (ms), game s2t= Game::GetSongToTaskMgrMs, p= per GemPlayer
// BeatMatcher sync offset / track lefty / smasher-0 world x. Every 600 frames in game it also
// prints `opts clock song-audio=<ms>`. Read-only.
void LogOptions() {
    static const bool on = [] {
        const char *v = getenv("RB3_APP_OPTIONS_LOG");
        return v && v[0] && strcmp(v, "0") != 0;
    }();
    if (!on || !TheSynth) return;
    ProfileMgr &pm = TheProfileMgr;
    char buf[1536];
    auto fade = [](const char *name) -> float {
        Fader *f = TheSynth->Find<Fader>(name, false);
        return f ? f->GetVal() : -999.0f;
    };
    FxSend *bass = TheSynth->Find<FxSend>("bass_boost.send", false);
    int n = snprintf(buf, sizeof buf,
                     "alag=%g vlag=%g sync=%g s2t=%g vol=%d/%d/%d/%d/%d/%d bass=%d dolby=%d "
                     "overscan=%d shrink=%d fade=%g/%g/%g/%g bassw=%g gstate=%d gdirty=%d mic=%d/%d/%d",
                     pm.GetExcessAudioLag(), pm.GetExcessVideoLag(), pm.GetSyncOffsetRaw(),
                     pm.GetSongToTaskMgrMsRaw(), pm.GetBackgroundVolume(),
                     pm.GetForegroundVolume(), pm.GetFxVolume(), pm.GetCrowdVolume(),
                     pm.GetVocalCueVolume(), pm.GetVoiceChatVolume(), pm.GetBassBoost(),
                     pm.GetDolby(), pm.GetOverscan(), TheRnd.ShrinkToSafeArea(),
                     fade("background_music_level.fade"), fade("crowd_level.fade"),
                     fade("sfx_level.fade"), fade("instrument_level.fade"),
                     bass ? bass->mWetGain : -999.0f, (int)pm.mGlobalOptionsSaveState,
                     (int)pm.mGlobalOptionsDirty, pm.GetMicVol(0), pm.GetMicVol(1), pm.GetMicVol(2));
    for (int i = 0; i < 4 && n < (int)sizeof buf - 64; i++) {
        BandProfile *p = pm.GetProfileFromPad(i);
        if (p && p->HasValidSaveData()) {
            GameplayOptions *o = p->GetGameplayOptions();
            n += snprintf(buf + n, sizeof buf - n, " p%d=lefty%d/vs%d/vv%d/unsaved%d/st%d", i,
                          o->GetLefty(), (int)o->GetVocalStyle(), o->mVocalVolume,
                          (int)p->IsUnsaved(), (int)p->GetSaveState());
            if (LocalBandUser *u = p->GetAssociatedLocalBandUser())
                n += snprintf(buf + n, sizeof buf - n, "/as%d", (int)pm.IsAutosaveEnabled(u));
        }
    }
    if (TheGame) {
        n += snprintf(buf + n, sizeof buf - n, " av=%.1f gs2t=%g p=", (TheTaskMgr.Seconds(TaskMgr::kRealTime) - TheTaskMgr.Seconds(TaskMgr::kDelayedTime)) * 1000.0f,
                      TheGame->GetSongToTaskMgrMs());
        for (Player *pl : TheGame->mAllActivePlayers) {
            GemPlayer *gp = dynamic_cast<GemPlayer *>(pl);
            if (!gp || n > (int)sizeof buf - 96) continue;
            float sx = -999.0f;
            GemManager *gm = gp->mTrack ? gp->mTrack->mGemManager : nullptr;
            if (gm && gm->mNowBar && gm->mNowBar->FindSmasher(0))
                sx = gm->mNowBar->FindSmasher(0)->Dir()->WorldXfm().v.x;
            n += snprintf(buf + n, sizeof buf - n, "sync%g/lefty%d/sx%.2f,",
                          gp->mMatcher ? gp->mMatcher->mSyncOffset : -999.0f,
                          gp->mTrack ? (int)gp->mTrack->Lefty() : -1, sx);
        }
    }
    static char last[1536];
    if (strcmp(buf, last)) {
        strcpy(last, buf);
        printf("rb3-app: frame %d opts %s\n", gFramesStarted, buf);
        fflush(stdout);
    }
    if (TheGame && TheGame->GetBeatMaster() && TheGame->GetBeatMaster()->GetAudio()
        && gFramesStarted % 600 == 0) {
        float audio = TheGame->GetBeatMaster()->GetAudio()->GetTime();
        printf("rb3-app: frame %d opts clock song=%.1f audio=%.1f song-audio=%.1f\n",
               gFramesStarted, TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f, audio,
               TheTaskMgr.Seconds(TaskMgr::kRealTime) * 1000.0f - audio);
        fflush(stdout);
    }
}

int EnvInt(const char *name, int dflt) {
    const char *v = getenv(name);
    return v && *v ? atoi(v) : dflt;
}
} // namespace

extern "C" void __real__Z10SystemPollb(bool);
extern "C" void __wrap__Z10SystemPollb(bool b) { // ring: native-real
    if (gInRun) {
        int framesDone = gFramesStarted; // every earlier frame ran to its DrawRegular
        if (framesDone >= gFrameCap) Finish(framesDone, "", 0);
        if (SecondsSinceRun() >= gSecondsCap) Finish(framesDone, " (wall-clock cap)", 4);
        ++gFramesStarted;
        rb3_app_determinism_frame(gFramesStarted);
        UIScreen *cur = TheUI ? TheUI->CurrentScreen() : nullptr;
        bool tr = TheUI && TheUI->InTransition();
        if (cur != gLastScreen || tr != gLastTransition || framesDone == 0) {
            printf("rb3-app: frame %d screen=%s transition=%s (%.2f s)\n", gFramesStarted,
                   ScreenName(cur), tr ? ScreenName(TheUI->TransitionScreen()) : "none",
                   SecondsSinceRun());
            fflush(stdout);
            gLastScreen = cur;
            gLastTransition = tr;
        }
        LogFocus(cur);
        LogTour();
        LogModifiers();
        LogSongSel();
        LogOptions();
    }
    __real__Z10SystemPollb(b);
}

int main(int argc, char **argv) {
    rb3_app_determinism_init();
    const char *data = argc > 1 ? argv[1] : getenv("RB3_ASSETS");
    if (!data || chdir(data) != 0) {
        fprintf(stderr, "rb3-app: no data dir (argv[1] or $RB3_ASSETS): %s\n",
                data ? data : "(unset)");
        return 2;
    }
    setenv("MILO_HEADLESS", "1", 0);
    printf("rb3-app: data %s\n", data);
    rb3_app_gate("main.chdir");
    // argv for App: the program name plus everything after the data dir.
    int appArgc = argc > 1 ? argc - 1 : argc;
    char **appArgv = argc > 1 ? argv + 1 : argv;
    appArgv[0] = argv[0];
    // A49: rb3-app marks its RndText glyph pages (rndobj/Text.cpp, HX_NATIVE), so
    // the engine draws only those as text; an unnamed mesh that is not a page
    // (WorldCrowd's impostor cards, spotlight beams, ribbons, gem tails) shades
    // with its own material instead of as an alpha atlas.
    SetGameMarksTextGlyphMeshes(true);
    // A56: and every glyph page samples its atlas RGB. Retail RndText draws a
    // page with the font's own material (Text.cpp SetMat(font->GetMat())) and
    // the standard shader samples the texel's RGBA like any texture's; the
    // engine's text default routes atlas alpha into colour, which drew the
    // coloured button and icon glyphs (Y/A/RB, star, mic) as white discs.
    SetGameGlyphAtlasesKeepRGB(true);
    App *app = new App(appArgc, appArgv);
    rb3_app_gate("App::App.complete");
    const char *co = getenv("RB3_APP_CONSTRUCT_ONLY");
    const char *run = getenv("RB3_APP_RUN");
    bool wantRun = run && *run && strcmp(run, "0") != 0;
    if ((co && *co && strcmp(co, "0") != 0) || !wantRun) {
        rb3_app_check_ui_sink(); // A16 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_tex_movie(); // A17 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_net_cache_offline(); // A19 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_tex_gpu_cleanup(); // A20 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_clip_driver_prune(); // A20 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_music_library_exit(); // A21 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_object_copy(); // A21 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_pre_clear_unregister(); // A22 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_offline_game_start(); // A23 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_trans_parent_replace(); // A24 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_sphere_base_replace(); // A24 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_owner_ptr_replace(); // A26 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_bones_typeprops_replace(); // A27 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_reserved_vocal_slot(); // A25 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_spotlight_disk(); // A25 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_music_library_on_exit(); // A28 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_clip_group_replace(); // A28 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_clip_transition_drop(); // A29 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_drum_venue_lookup(); // A30 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_replace_refs_shifted_entry(); // A31 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_bones_ref_dying_compare(); // A32 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_typeprops_remove_one(); // A33 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_poll_sorter_search_id(); // A34 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_cube_tex_gpu_cleanup(); // A36 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_session_settings_unranked(); // A36 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_compress_queue_drain(); // A35 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_campaign_screen_push(); // A38 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_nonull_list_replace_drain(); // A37 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_bone_merge_repoint(); // A41 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_credits_auto_scroll(); // credits lane (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_postproc_noise_map(); // A49 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_retail_post_chain(); // A55 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_retail_post_noise(); // A58 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_gamma_space_shading(); // A60 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_retail_light_model(); // A62 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_unnamed_mesh_not_text(); // A49 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_cam_screen_y_down(); // A51 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_crowd_force_3d(); // A53 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_glyph_atlas_rgb(); // A56 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_lyric_line_color_kept(); // A57 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_environ_lr_fade(); // A59 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_world_end_depth_clear(); // A64 (rb3_app_gates.cpp): exits rc 7 on a failure
        rb3_app_check_album_art_path(); // A56 (rb3_app_gates.cpp): exits rc 7 on a failure
        printf("RESULT: APP CONSTRUCTED\n");
        fflush(stdout);
        fflush(stderr);
        _exit(0);
    }
    gFrameCap = EnvInt("RB3_APP_FRAMES", 300);
    gSecondsCap = EnvInt("RB3_APP_SECONDS", 120);
    printf("rb3-app: App::App constructed; App::Run (cap %d frames, %.0f s)\n", gFrameCap,
           gSecondsCap);
    fflush(stdout);
    signal(SIGALRM, OnStall);
    alarm((unsigned)gSecondsCap + 15);
    clock_gettime(CLOCK_MONOTONIC, &gRunStart);
    gInRun = true;
    app->Run();
    // App::Run never returns in retail; the SystemPoll wrapper exits.
    printf("RESULT: APP RUN RETURNED after %d frames\n", gFramesStarted);
    fflush(stdout);
    _exit(6);
}
