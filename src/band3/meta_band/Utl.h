#pragma once
#include "game/Defines.h"

class BandUser;
class MetaPerformer;

void UtlInit();
bool IsLeaderLocal();
const char *GetFontCharFromInstrument(Symbol, int);
const char *GetFontCharFromTrackType(TrackType, int);
const char *GetFontCharForProDrums(int);
const char *GetFontCharForHarmonyMics(int, int);
const char *GetFontCharFromScoreType(ScoreType, int);
const char *GetFontCharFromControllerType(ControllerType, int);
const char *GetUserFontChar(BandUser *, MetaPerformer *, int);
bool IsVignette(class UIPanel *);
bool AllowedToAccessContent(int);