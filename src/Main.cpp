#include "App.h"

// main (0x82272E68) constructs the App, calls App::Run (0x822703D0) and
// destroys it (App::~App, 0x82270000). The `bcl 20,0,X` residual that lane
// DS-4/B recorded at 0x82272E90 was RB3 Deluxe's in-place "debugger check"
// patch, which sent this call straight to App::RunWithoutDebugging
// (0x82270080) and so skipped App::Run's exception-filter entry. The clean TU5
// target (W16-PT) has the ordinary `bl App::Run`.
int main(int argc, char **argv) {
    App app(argc, argv);
    app.Run();
}
