#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class StationURL : public RootObject {
    public:
        StationURL();
        StationURL(const StationURL &);
        virtual ~StationURL();

    private:
        char m_data[0x54]; // total size 0x58
    };
}