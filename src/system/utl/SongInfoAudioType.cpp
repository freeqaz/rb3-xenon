#include "utl/SongInfoAudioType.h"
#include "SongInfoAudioType.h"
#include "os/Debug.h"
#include "utl/Symbol.h"

SongInfoAudioType SymbolToAudioType(Symbol s) {
    static Symbol drum("drum");
    static Symbol drum2("drum2");
    static Symbol perc("perc");
    static Symbol bass("bass");
    static Symbol guitar("guitar");
    static Symbol guitar2("guitar2");
    static Symbol vocals("vocals");
    static Symbol harm1("harm1");
    static Symbol harm2("harm2");
    static Symbol keys("keys");
    static Symbol keys2("keys2");
    static Symbol keys3("keys3");
    static Symbol backing("backing");
    static Symbol backing2("backing2");
    static Symbol backing3("backing3");
    static Symbol multi("multi");
    static Symbol center("center");
    static Symbol side("side");

    if (s == drum)
        return kAudioTypeDrum;
    else if (s == drum2)
        return kAudioTypeDrum2;
    else if (s == perc)
        return kAudioTypePerc;
    else if (s == bass)
        return kAudioTypeBass;
    else if (s == guitar)
        return kAudioTypeGuitar;
    else if (s == guitar2)
        return kAudioTypeGuitar2;
    else if (s == vocals)
        return kAudioTypeVocals;
    else if (s == harm1)
        return kAudioTypeHarm1;
    else if (s == harm2)
        return kAudioTypeHarm2;
    else if (s == keys)
        return kAudioTypeKeys;
    else if (s == keys2)
        return kAudioTypeKeys2;
    else if (s == keys3)
        return kAudioTypeKeys3;
    else if (s == backing)
        return kAudioTypeBacking;
    else if (s == backing2)
        return kAudioTypeBacking2;
    else if (s == backing3)
        return kAudioTypeBacking3;
    else if (s == multi)
        return kAudioTypeMulti;
    else if (s == center)
        return kAudioTypeCenter;
    else if (s == side)
        return kAudioTypeSide;
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
