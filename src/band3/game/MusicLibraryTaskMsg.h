#pragma once
#include "meta_band/MusicLibrary.h"
#include "network/net/NetMessage.h"

// One definition for the three TUs that touch this message: game/Game.cpp
// registers it, game/NetGameMsgs.cpp defines its methods, tour/Tour.cpp sends
// it.  Each used to declare its own copy, two with a user-declared virtual
// destructor and one without -- the same class defined two ways
// (tools/layout_odr.py reports the differing vftable adjustor lists).  The
// form kept is tour/Tour.cpp's: retail's ??1MusicLibraryTaskMsg (in the Tour
// unit) does not re-store the derived vptr, which is what the IMPLICIT
// destructor compiles to and a user-declared `{}` one does not.
class MusicLibraryTaskMsg : public NetMessage {
public:
    MusicLibraryTaskMsg() {}
    MusicLibraryTaskMsg(MusicLibrary::MusicLibraryTask &);
    virtual void Save(BinStream &) const;
    virtual void Load(BinStream &);
    virtual void Dispatch();
    NETMSG_BYTECODE(MusicLibraryTaskMsg);
    NETMSG_NAME(MusicLibraryTaskMsg);
    NETMSG_NEWNETMSG(MusicLibraryTaskMsg);

    MusicLibrary::MusicLibraryTask mTask; // 0x4
};
