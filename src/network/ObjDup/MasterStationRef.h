#pragma once
#include "DORef.h"
#include "Platform/LogicalClock.h"

namespace Quazal {
    class MasterStationRef : public DORef {
    public:
        MasterStationRef();
        MasterStationRef(const MasterStationRef &);
        ~MasterStationRef();

        LogicalClockTmpl<unsigned char> m_lcVersion; // 0xc
    };
}