#pragma once
#include "ObjDup/DOHandle.h"

namespace Quazal {
    class WKHandle : public DOHandle {
    public:
        WKHandle();
        ~WKHandle();
        void Init(unsigned int);
        void Cleanup();
        bool AtLeastOneCreated();
        bool AllInDOS();
        static bool IsAWKHandle(DOHandle);

        int unk4; // 0x4
        int unk8; // 0x8
    };
}
