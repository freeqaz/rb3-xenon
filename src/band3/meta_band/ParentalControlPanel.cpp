#include "meta_band/ParentalControlPanel.h"
#include "game/BandUser.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/PlatformMgr.h"
#include "os/User.h"
#include "ui/UIPanel.h"
#include "utl/Symbol.h"
#include "xdk/XAPILIB.h"
#include <string.h>

/** Retail `0x82B544E8` -- the async console UI shim this panel drives.
 *
 * It has exactly ONE caller in the whole retail image (this Enter()), carries no
 * string of its own, and is not in any oracle, so its real name is not
 * recoverable from the material available to this lane. Only its SHAPE is
 * load-bearing for the match: `(dwUserIndex, XOVERLAPPED *) -> DWORD`, returning
 * ERROR_IO_PENDING while the UI is up. */
DWORD ShowParentalControlUI(int padNum, XOVERLAPPED *pOverlapped);

ParentalControlPanel::ParentalControlPanel() : mUser(0), mDone(0), mOverlapped(0) {}

void ParentalControlPanel::Enter() {
    UIPanel::Enter();
    mOverlapped = new XOVERLAPPED();
    memset(mOverlapped, 0, sizeof(XOVERLAPPED));
    // Named local, not an inline sub-expression: retail evaluates the argument
    // list LEFT-to-RIGHT here, so `mOverlapped` is re-loaded from the object
    // AFTER the GetPadNum() vcall (`lwz r4, 0x44(r31)`). Written inline, MSVC
    // evaluates right-to-left and caches mOverlapped in r30 across the call.
    int padNum = mUser->GetPadNum();
    if (ShowParentalControlUI(padNum, mOverlapped) != ERROR_IO_PENDING) {
        mDone = true;
        delete mOverlapped;
        mOverlapped = 0;
    }
}

void ParentalControlPanel::Poll() {
    UIPanel::Poll();
    if (mOverlapped) {
        unsigned int status = XGetOverlappedResult(mOverlapped, 0, 0);
        if (status == ERROR_IO_INCOMPLETE)
            return;
        mDone = true;
        if (status == ERROR_SUCCESS) {
            // Retail stores a bool into ThePlatformMgr (0x82CC9D1C) at +0x3d.
            // Our PlatformMgr -- ported from the NEWER dc3 tree -- has no member
            // there: +0x3c starts `mOverlapped` (XOVERLAPPED). Addressed
            // positionally rather than by perturbing a 196-byte binary-wide
            // singleton for one store. See the lane report.
            *(reinterpret_cast<bool *>(&ThePlatformMgr) + 0x3d) = true;
        }
        delete mOverlapped;
        mOverlapped = 0;
    }
    if (mDone) {
        static Message done_msg(Symbol("done"));
        HandleType(done_msg);
    }
}
