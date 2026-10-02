#pragma once
#include "Platform/RootObject.h"
#include "ObjDup/DOHandle.h"

namespace Quazal {
    class Message : public RootObject {
    public:
        ~Message();

        char m_unk0[0x24];
        DOHandle m_hSource; // 0x24
    };
}
