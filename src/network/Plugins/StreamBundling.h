#pragma once
#include "Platform/RootObject.h"
#include "Platform/Time.h"

namespace Quazal {
    class StreamBundling : public RootObject {
    public:
        StreamBundling();
        virtual ~StreamBundling();

        void Flush();

        // The vfptr is padded to 8 (Time member). Retail Station::FlushBundle
        // (0x82A7D580) tests this byte before Flush().
        bool m_bEnabled; // 0x8
        Time unk10;
    };
}