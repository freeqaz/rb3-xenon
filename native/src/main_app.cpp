// rb3-app native driver (lane A1, 2026-11-22). Native only.
//
// Retail main (src/Main.cpp, 0x82272E68) is `App app(argc, argv); app.Run();`.
// A1's milestone is App::App alone, headless, with no frame loop, so this
// driver replaces Main.cpp in rb3-app (Main.cpp stays the X360 TU, untouched):
//
//   1. the data dir: retail runs with game:\ at the disc root, and SystemInit
//      opens gen/main_xbox.hdr relative to it. Natively the disc root is
//      argv[1], else $RB3_ASSETS; the driver chdir()s there (rc 2 if absent).
//   2. headless: MILO_HEADLESS=1 is forced for the engine (no window, no
//      swapchain) unless the caller already set it.
//   3. App::App(argc, argv) with argv[0] and any arguments after the data dir.
//   4. a gate line once the constructor returns, then _exit(0): App::Run and
//      ~App are A2's (the frame loop and teardown are not this milestone).
//      Every init step prints its own gate (rb3_app_gates.cpp).
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include "App.h"

extern "C" void rb3_app_gate(const char *step);

int main(int argc, char **argv) {
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
    (void)app;
    rb3_app_gate("App::App.complete");
    printf("RESULT: APP CONSTRUCTED\n");
    fflush(stdout);
    fflush(stderr);
    _exit(0);
}
