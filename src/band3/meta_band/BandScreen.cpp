#include "meta_band/BandScreen.h"
#include "meta_band/BandUI.h"
#include "meta_band/InterstitialMgr.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "ui/UI.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"

bool BandScreen::Entering() const { return UIScreen::Entering() || TheBandUI.WipingIn(); }

bool BandScreen::Exiting() const { return UIScreen::Exiting() || TheBandUI.WipingOut(); }

bool BandScreen::CheckIsLoaded() { return UIScreen::CheckIsLoaded() != 0; }

bool BandScreen::IsLoaded() const { return UIScreen::IsLoaded() != 0; }

void BandScreen::LoadInterstitials() {
    TheBandUI.mInterstitialMgr->GetInterstitialsFromScreen(this, mExtraPanels);
    FOREACH (it, mExtraPanels) {
        UIPanel *cur = *it;
        cur->CheckLoad();
        cur->CheckIsLoaded();
    }
}

void BandScreen::LoadPanels() {
    UIScreen::LoadPanels();
    if (TheUI->GetTransitionState() == UIManager::kTransitionTo && TheUI->PushDepth() == 0) {
        MILO_ASSERT(TheUI->TransitionScreen() == this, 0x45);
        LoadInterstitials();
    }
}

void BandScreen::UnloadInterstitials() {
    FOREACH_REVERSE(it, mExtraPanels) { (*it)->CheckUnload(); }
}

BEGIN_HANDLERS(BandScreen)
    HANDLE_SUPERCLASS(UIScreen)
    HANDLE_CHECK(0x88)
END_HANDLERS

void BandScreen::Enter(UIScreen *s) {
    UIScreen::Enter(s);
    // 0x82642798: the message is a function-local static built on first use.
    static Message block_wipe_in_msg("block_wipe_in");
    const DataNode &handled = HandleType(block_wipe_in_msg);
    if (handled.Type() == kDataUnhandled || !handled.Int()) {
        TheBandUI.WipeInIfNecessary();
    }
}

void BandScreen::Exit(UIScreen *s) {
    UIScreen::Exit(s);
    TheBandUI.WipeOutIfNecessary();
    UnloadInterstitials();
}
