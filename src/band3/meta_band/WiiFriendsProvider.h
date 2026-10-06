#pragma once

// The Wii friends-list provider.  Retail X360 has no instance of it: every use
// is either compiled only under HX_NATIVE (meta_band/MetaPanel.cpp) or sits
// behind a branch retail never takes (meta_band/MusicLibrary.cpp).  It is
// declared once here because those two TUs used to declare it separately, one
// with a 4-byte filler member and one empty -- the same class with two layouts
// (tools/layout_odr.py).
class WiiFriendsProvider {
public:
    void Init();
    void Poll();
    bool IsPossessiveSuffixNeeded(const char *);
    const char *GetPossessiveSuffix(const char *);
};
extern WiiFriendsProvider TheWiiFriendsProvider;
