#include "App.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "os/Debug.h"

// Only the debug-modal hook is written so far; the rest of this TU (App::App,
// App::~App, App::Run, ...) is still unwritten.

Debug::ModalCallbackFunc *gRealCallback;

// Installed through Debug::SetModalCallback, so it has no direct callers.
// The logging branch of notify level 0 compiles to nothing in retail.
void AppDebugModal(bool &b, char *msg, bool b2) {
    if (!b) {
        static DataNode &notify_level = DataVariable("notify_level");
        int notif_lvl = notify_level.Int();
        if (notif_lvl == 2) {
            gRealCallback(b, msg, b2);
            return;
        } else if (notif_lvl == 1) {
            Hmx::Object *disp = ObjectDir::Main()->Find<Hmx::Object>("cheat_display", false);
            if (disp) {
                static Message show("show_prio", 0, 0);
                show[0] = msg;
                show[1] = DataNode(200);
                disp->Handle(show, false);
            }
        }
    } else
        gRealCallback(b, msg, b2);
}
