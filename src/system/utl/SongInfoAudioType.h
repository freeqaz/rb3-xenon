#pragma once
#include "utl/Symbol.h"

enum SongInfoAudioType {
    kAudioTypeDrum = 0,
    kAudioTypeDrum2 = 1,
    kAudioTypePerc = 2,
    kAudioTypeBass = 3,
    kAudioTypeGuitar = 4,
    kAudioTypeGuitar2 = 5,
    kAudioTypeVocals = 6,
    kAudioTypeHarm1 = 7,
    kAudioTypeHarm2 = 8,
    kAudioTypeKeys = 9,
    kAudioTypeKeys2 = 10,
    kAudioTypeKeys3 = 11,
    kAudioTypeBacking = 12,
    kAudioTypeBacking2 = 13,
    kAudioTypeBacking3 = 14,
    kAudioTypeFake = 15, // this is a guess
    kAudioTypeMulti = 16,
    kAudioTypeCenter = 17,
    kAudioTypeSide = 18
};

SongInfoAudioType SymbolToAudioType(Symbol);
Symbol SongInfoAudioTypeToSym(SongInfoAudioType);
