#include "meta_band/SongSetlistProvider.h"
#include "meta_band/AppLabel.h"
#include "meta_band/MusicLibrary.h"
#include "obj/Dir.h"
#include "os/Debug.h"
#include "ui/UI.h"
#include "ui/UIListLabel.h"
#include "ui/UIScreen.h"
#include "utl/Locale.h"
#include "utl/Symbol.h"

// ---------------------------------------------------------------------------
// RB3-360 retail: .text 0x825BC6F8..0x825BC8FC, three COMDATs in source order:
//   0x825BC6F8 (272 B) SetlistProvider::Text
//   0x825BC808 ( 32 B) ??__F_choosing  (atexit funclet for Text's local static)
//   0x825BC828 (212 B) SetlistProvider::NumData
//
// Divergences from the rb3-Wii DEV oracle
// (../rb3/src/band3/meta_band/SongSetlistProvider.cpp):
//  * `choosing` is a FUNCTION-LOCAL `static Symbol`, not the centralized global
//    from utl/Symbols2.h — retail emits the guard-bit test + inline Symbol ctor
//    at 0x825BC774 (guard word 0x82DFF5F8 bit 0, Symbol storage 0x82DFF5F4) and
//    the matching 32-byte ??__F funclet at 0x825BC808 that clears that same bit.
//  * `TheUI` is a POINTER in RB3-360 (`extern UIManager *TheUI`), so the Wii
//    `TheUI.` member syntax becomes `TheUI->`; TransitionScreen()/mTransitionState
//    inline to lwz 0x30 / lwz 0x10 off the loaded pointer (0x825BC8B4/0x825BC8C0).
//  * MILO_ASSERT is (void)(cond) in retail — the dynamic_cast side effect stays,
//    the failure branch does not exist.
// ---------------------------------------------------------------------------

void SetlistProvider::Text(int, int data, UIListLabel *slot, UILabel *label) const {
    if (slot->Matches("song")) {
        AppLabel *appLabel = dynamic_cast<AppLabel *>(label);
        MILO_ASSERT(appLabel, 0x1D);
        int song = TheMusicLibrary->SongAtSetlistIndex(data);
        if (song == 0) {
            if (TheMusicLibrary->SetlistSize() == data) {
                static Symbol choosing("choosing");
                appLabel->SetSongNameWithNumber(
                    song, data + 1, Localize(choosing, nullptr)
                );
            } else
                appLabel->SetSongNameWithNumber(song, data + 1, gNullStr);
        } else {
            appLabel->SetSongNameWithNumber(song, data + 1, nullptr);
        }
    } else {
        label->SetTextToken(gNullStr);
    }
}

int SetlistProvider::NumData() const {
    if (TheMusicLibrary->GetForcedSetlist()
        && TheMusicLibrary->GetMaxSetlistSize() != 0) {
        return TheMusicLibrary->GetMaxSetlistSize();
    } else {
        UIScreen *songSelectScreen =
            ObjectDir::Main()->Find<UIScreen>("song_select_screen", true);
        UIScreen *diffScreen =
            ObjectDir::Main()->Find<UIScreen>("part_difficulty_screen", true);
        if (TheUI->BottomScreen() == songSelectScreen
            && (TheUI->TransitionScreen() != diffScreen
                || TheUI->GetTransitionState() == UIManager::kTransitionFrom)
            && !TheMusicLibrary->SetlistIsFull()) {
            return TheMusicLibrary->SetlistSize() + 1;
        } else
            return TheMusicLibrary->SetlistSize();
    }
}
