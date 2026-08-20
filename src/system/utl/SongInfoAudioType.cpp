#include "utl/SongInfoAudioType.h"
#include "SongInfoAudioType.h"
#include "os/Debug.h"
#include "utl/Symbol.h"

// Retail RB3 tests exactly six symbols, in this order: drum, guitar, bass, vocals,
// keys, multi.  Ground truth is the retail body at 0x827D4260 (420 bytes), whose only
// .rdata string references are 0x82010E1C "drum", 0x82013FA8 "guitar", 0x82013FA0
// "bass", 0x8201DB6C "vocals", 0x8201DB64 "keys", 0x820F3064 "multi" -- six
// Symbol(const char*) constructions and no others.  The wider drum2/perc/guitar2/
// harm*/keys2/keys3/backing*/center/side chain that used to be here is DC3-shaped and
// has no counterpart in this binary; see the note in SongInfoAudioType.h.
SongInfoAudioType SymbolToAudioType(Symbol s) {
    static Symbol drum("drum");
    static Symbol guitar("guitar");
    static Symbol bass("bass");
    static Symbol vocals("vocals");
    static Symbol keys("keys");
    static Symbol multi("multi");

    if (s == drum)
        return kAudioTypeDrum;
    else if (s == guitar)
        return kAudioTypeGuitar;
    else if (s == bass)
        return kAudioTypeBass;
    else if (s == keys)
        return kAudioTypeKeys;
    else if (s == vocals)
        return kAudioTypeVocals;
    else if (s == multi)
        return kAudioTypeMulti;
    else {
        MILO_FAIL("No instrument for %s\n", s);
        return kAudioTypeDrum;
    }
}

Symbol SongInfoAudioTypeToSym(SongInfoAudioType t) {
    switch (t) {
    case kAudioTypeDrum:
        return "drum";
    case kAudioTypeDrum2:
        return "drum2";
    case kAudioTypePerc:
        return "perc";
    case kAudioTypeBass:
        return "bass";
    case kAudioTypeGuitar:
        return "guitar";
    case kAudioTypeGuitar2:
        return "guitar2";
    case kAudioTypeVocals:
        return "vocals";
    case kAudioTypeHarm1:
        return "harm1";
    case kAudioTypeHarm2:
        return "harm2";
    case kAudioTypeKeys:
        return "keys";
    case kAudioTypeKeys2:
        return "keys2";
    case kAudioTypeKeys3:
        return "keys3";
    case kAudioTypeBacking:
        return "backing";
    case kAudioTypeBacking2:
        return "backing2";
    case kAudioTypeBacking3:
        return "backing3";
    case kAudioTypeMulti:
        return "multi";
    case kAudioTypeCenter:
        return "center";
    case kAudioTypeSide:
        return "side";
    default:
        MILO_FAIL("Unknown SongInfoAudioType %d.\n", t);
        return gNullStr;
    }
}
