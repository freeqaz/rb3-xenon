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
