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

        bool IsCreated() const { return m_bCreated; }

        bool unk4; // 0x4
        bool m_bCreated; // 0x5
        int unk8; // 0x8
    };
}
