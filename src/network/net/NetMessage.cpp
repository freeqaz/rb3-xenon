#include "net/NetMessage.h"
#include "os/Debug.h"

NetMessageFactory TheNetMessageFactory;

unsigned char NetMessageFactory::GetNetMessageByteCode(String type) const {
    for (int i = 0; i < mFactoryList.size(); i++) {
        if (mFactoryList[i].mType == type)
            return i;
    }
    // retail keeps the by-value String copy of the failure's argument
    MILO_FAIL_RTL("No Registered NetMessage by this name %s", type);
    return 0;
}

NetMessage *NetMessageFactory::CreateNetMessage(unsigned char byteCode) {
    MILO_ASSERT(byteCode < mFactoryList.size(), 0x36);
    return mFactoryList[byteCode].mCreator();
}

// Retail evaluates neither the duplicate-type check nor the size check: the
// 92-byte body is just the pair construction and the push_back.
void NetMessageFactory::RegisterNetMessage(String type, NetMessageFunc *creator) {
    TypeCreatorPair tcPair;
    tcPair.mType = type;
    tcPair.mCreator = creator;
    mFactoryList.push_back(tcPair);
}
