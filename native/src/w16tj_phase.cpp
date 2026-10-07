// rb3-xenon native -- W16-TJ (link probe skeleton; replaced below).
#include "bandtrack/GemManager.h"
#include "bandtrack/TrackConfig.h"
#include "meta_band/CustomizePanel.h"

typedef void (*GateFn)(const char *, bool, const char *);

int RunW16TJPhase(GateFn gate) {
    static volatile int never = 0;
    if (never) {
        CustomizePanel *p = new CustomizePanel;
        p->Handle(nullptr, true);
        TrackConfig *tc = new TrackConfig(nullptr);
        GemManager *gm = new GemManager(*tc, nullptr);
        gm->SetupGems(0);
    }
    (void)gate;
    return 0;
}
