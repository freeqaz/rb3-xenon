#pragma once
#include "utl/Symbol.h"

// RB3 retail (X360) uses the SAME 7-value enum as the rb3-Wii oracle, NOT the
// wider DC3 one.  Ground truth: SongParser::AudioTrackUsed compares against
// `cmpwi 5` for kAudioTypeFake (target), whereas the DC3 numbering put Fake at
// 15.  The DC3 extras (drum2/perc/guitar2/harm1/harm2/keys2/keys3/backing*/
// center/side) are LATER additions; they are kept here only so the existing
// SymbolToAudioType/SongInfoAudioTypeToSym bodies still compile, and are given
// values above the retail range so they can never collide with a retail value.
enum SongInfoAudioType {
    kAudioTypeDrum = 0,
    kAudioTypeGuitar = 1,
    kAudioTypeBass = 2,
    kAudioTypeVocals = 3,
    kAudioTypeKeys = 4,
    kAudioTypeFake = 5,
    kAudioTypeMulti = 6,
    // --- not present in RB3 retail ---
    kAudioTypeDrum2 = 7,
    kAudioTypePerc = 8,
    kAudioTypeGuitar2 = 9,
    kAudioTypeHarm1 = 10,
    kAudioTypeHarm2 = 11,
    kAudioTypeKeys2 = 12,
    kAudioTypeKeys3 = 13,
    kAudioTypeBacking = 14,
    kAudioTypeBacking2 = 15,
    kAudioTypeBacking3 = 16,
    kAudioTypeCenter = 17,
    kAudioTypeSide = 18
};

SongInfoAudioType SymbolToAudioType(Symbol);
Symbol SongInfoAudioTypeToSym(SongInfoAudioType);
