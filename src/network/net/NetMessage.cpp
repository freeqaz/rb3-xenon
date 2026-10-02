#include "net/NetMessage.h"
#include "os/Debug.h"

// Retail TU: .text 0x823F0A00-0x823F0F84 (between the Xbox session code and
// RockCentral), plus TheNetMessageFactory's initializer/atexit pair.

NetMessageFactory TheNetMessageFactory;

unsigned char NetMessageFactory::GetNetMessageByteCode(String type) const {
    for (int i = 0; i < mFactoryList.size(); i++) {
        if (mFactoryList[i].mType == type)
            return i;
    }
    // Retail keeps a by-value copy of `type` on this path (0x823F0AE8) and
    // nothing else: the stripped varargs form.
    MILO_FAIL_RTL("No Registered NetMessage by this name %s", type);
    return 0;
}

NetMessage *NetMessageFactory::CreateNetMessage(unsigned char byteCode) {
    MILO_ASSERT(byteCode < mFactoryList.size(), 0x36);
    return mFactoryList[byteCode].mCreator();
}

void NetMessageFactory::RegisterNetMessage(String type, NetMessageFunc *creator) {
    // Retail (0x823F0ED8, 92 bytes) has no duplicate-name scan before the
    // push_back: no String comparison survives in the body.
    TypeCreatorPair tcPair;
    tcPair.mType = type;
    tcPair.mCreator = creator;
    mFactoryList.push_back(tcPair);
    MILO_ASSERT(mFactoryList.size() < 256, 0x23);
}
