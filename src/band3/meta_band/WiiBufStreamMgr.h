#pragma once

class WiiBufStreamMgr {
public:
    static int GetWiiProfileMgrStartOffset();
    static int GetProfileStartOffset(int profileIndex);
    static int GetSongStatusStartOffset(int profileIndex);
};
