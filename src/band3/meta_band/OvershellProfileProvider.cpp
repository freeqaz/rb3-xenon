#include "meta_band/OvershellProfileProvider.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "meta_band/AppLabel.h"
#include "obj/ObjMacros.h"
#include "os/User.h"

// Retail X360 TU at 0x82667FE0-0x8266849C (lane W16-OR; read from
// build/45410914/asm/PropKeys.s, which pinned it before this TU existed):
//
//   0x82667FE0  200 B  Handle            (secondary, Hmx::Object, table slot 6)
//   0x826680D0  176 B  Text              (primary table slot 1)
//   0x826681A0  204 B  Reload
//   0x82668278  124 B  ctor              (already named in the map)
//   0x82668378    8 B  `??_E` adjustor thunk (this-4)
//   0x82668388  116 B  dtor
//   0x82668450   76 B  scalar deleting dtor
//
// plus the static-Symbol guard resets and EH unwind funclets between them.
// Mat (primary slot 2) and NumData (slot 10) are overrides in retail too, but
// their bodies are ICF-folded elsewhere: Mat onto the 0x823591E8 `return 0`
// hub, NumData onto 0x82B7B280 (a 4-byte-element vector size).

BEGIN_HANDLERS(OvershellProfileProvider)
    HANDLE_EXPR(num_data, NumData())
    HANDLE_CHECK(0x3B)
END_HANDLERS

// retail 0x826680D0: the label is dynamic_cast to AppLabel BEFORE the static
// Symbol is constructed, then the user's name (User virtual base) fills the
// token.
void OvershellProfileProvider::Text(int, int data, UIListLabel *, UILabel *label) const {
    AppLabel *app = dynamic_cast<AppLabel *>(label);
    static Symbol overshell_swap_profile("overshell_swap_profile");
    app->SetTokenFmt(overshell_swap_profile, mUsers[data]->UserName());
}

RndMat *OvershellProfileProvider::Mat(int, int, UIListMesh *) const { return 0; }

int OvershellProfileProvider::NumData() const { return mUsers.size(); }

// retail 0x826681A0: refill from the user manager, then drop every user that
// is not signed in or is a guest (LocalUser virtual-base table slots 4 and 3).
void OvershellProfileProvider::Reload() {
    mUsers.clear();
    mBandUserMgr->GetLocalUsersWithAnyController(mUsers);
    for (std::vector<LocalBandUser *>::iterator it = mUsers.begin();
         it != mUsers.end();) {
        if (!(*it)->IsSignedIn() || (*it)->IsGuest()) {
            it = mUsers.erase(it);
        } else {
            ++it;
        }
    }
}

// retail 0x82668278: UIListProvider, Hmx::Object at +4, the manager at 0x2c,
// an empty vector at 0x30, then Reload().
OvershellProfileProvider::OvershellProfileProvider(BandUserMgr *mgr)
    : mBandUserMgr(mgr) {
    Reload();
}

// retail 0x82668388: only the implicit ~vector and the base destructors.
OvershellProfileProvider::~OvershellProfileProvider() {}

#ifdef HX_NATIVE
// Wii-only profile-swap entry points. Retail X360 has no bodies for them;
// they exist so the native engine links OvershellSlot's calls.
WiiProfileActResult
OvershellProfileProvider::ActOnProfile(int, LocalBandUser *, bool) {
    return kWiiProfileActResult_Done;
}
WiiProfileActResult OvershellProfileProvider::ActOnProfileConfirmed(LocalBandUser *) {
    return kWiiProfileActResult_Done;
}
void OvershellProfileProvider::SetWiiProfileListMode(WiiProfileListMode, bool) {}
OvershellProfileProvider::WiiProfileListMode
OvershellProfileProvider::GetWiiProfileListMode() {
    return (WiiProfileListMode)0;
}
int OvershellProfileProvider::GetWiiProfileCount(LocalBandUser *) const { return 0; }
const char *OvershellProfileProvider::GetWiiProfileSelectedName() const { return ""; }
#endif
