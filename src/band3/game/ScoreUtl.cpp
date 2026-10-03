#include "game/ScoreUtl.h"
#include "game/GameConfig.h"
#include "game/Scoring.h"

// Retail 0x826F3AC8 (its own TU between VocalPart and HeldNote) reads the
// track type from the game config's player track list, not from the BandUser.
int GetStarsForScore(int score, const UserGuid &guid) {
    TrackType type =
        TheGameConfig->GetConfigList()->GetConfigByUserGuid(guid).GetTrackType();
    return TheScoring->GetSoloNumStars(score, type);
}
