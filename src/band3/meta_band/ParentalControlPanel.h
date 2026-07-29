#pragma once
#include "obj/ObjMacros.h"
#include "ui/UIPanel.h"
#include "xdk/XAPILIB.h"

class LocalBandUser;

/** RB3-360 retail "parental controls" gate panel.
 *
 * Enter() fires the console's async parental-control UI for the panel's user
 * and parks the XOVERLAPPED; Poll() drains it and raises `done`.
 *
 * NOTE (retail-vs-rb3-Wii): the Wii dev decomp of this class
 * (`../rb3/src/band3/meta_band/ParentalControlPanel.{cpp,h}`) is a STUB -- two
 * `unkNN` members and two two-line bodies. Retail has three members and real
 * bodies. It also does NOT redeclare `virtual ~ParentalControlPanel()`:
 * proved from the retail vtable at 0x820C9C54 -- PCP's primary slot 0 is
 * byte-identical to UIPanel's (0x82812CB8), so the derived dtor is not its own
 * slot (laneBL section 7's "redundant derived destructor declaration" rule,
 * here settled from ground truth rather than from a diff).
 */
class ParentalControlPanel : public UIPanel {
public:
    ParentalControlPanel();
    OBJ_CLASSNAME(ParentalControlPanel);
    OBJ_SET_TYPE(ParentalControlPanel);
    NEW_OBJ(ParentalControlPanel);

    // UIPanel
    virtual void Enter();
    virtual void Poll();

    /** The local user the parental-control UI is shown for. Nothing inside this
     * TU writes it; it is set from the panel's owner. */
    LocalBandUser *mUser; // 0x3c
    /** Set once the async UI has completed; gates the `done` message. */
    bool mDone; // 0x40
    /** The in-flight async request, owned; freed on completion. */
    XOVERLAPPED *mOverlapped; // 0x44
};
