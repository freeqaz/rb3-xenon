#include "meta_band/WiiBufStreamMgr.h"
#include "meta_band/ProfileMgr.h"
#include "os/Debug.h"

int WiiBufStreamMgr::GetWiiProfileMgrStartOffset() {
    return TheProfileMgr.GetGlobalOptionsSize() + 8;
}

int WiiBufStreamMgr::GetProfileStartOffset(int profileIndex) {
    MILO_ASSERT(profileIndex >= 0, 0x38);
    return profileIndex * 0x140000 + 0x40000;
}

int WiiBufStreamMgr::GetSongStatusStartOffset(int profileIndex) {
    MILO_ASSERT(profileIndex >= 0, 0x4b);
    int base = GetProfileStartOffset(4);
    return base + profileIndex * 0x180000;
}
