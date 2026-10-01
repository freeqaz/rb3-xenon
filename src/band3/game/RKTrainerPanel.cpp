#include "game/RKTrainerPanel.h"
#include "game/Defines.h"
#include "game/ProTrainerPanel.h"
#include "meta_band/BandProfile.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"

void RKTrainerPanel::SetSongSectionComplete(
    BandProfile *profile, int songID, Difficulty diff, int section
) {
    for (int i = 0; i <= diff; i++) {
        profile->SetProKeyboardSongLessonSectionComplete(
            songID, (Difficulty)i, GetCurrSection()
        );
        if (AllSectionsFinished()) {
            profile->SetProKeyboardSongLessonComplete(songID, (Difficulty)i);
        }
    }
}

bool RKTrainerPanel::IsSongSectionComplete(
    BandProfile *profile, int songID, Difficulty diff, int section
) {
    return profile->IsProKeyboardSongLessonSectionComplete(songID, diff, section);
}

BEGIN_HANDLERS(RKTrainerPanel)
    HANDLE_SUPERCLASS(ProTrainerPanel)
    HANDLE_CHECK(0x2F)
END_HANDLERS

RKTrainerPanel::RKTrainerPanel() {}

RKTrainerPanel::~RKTrainerPanel() {}
