#pragma once
#include "DORef.h"
#include "Platform/LogicalClock.h"

namespace Quazal {
    class MasterStationRef : public DORef {
    public:
        MasterStationRef();
        MasterStationRef(const MasterStationRef &);
        MasterStationRef(DOHandle, LogicalClockTmpl<unsigned char>);
        ~MasterStationRef();
        MasterStationRef &operator=(const MasterStationRef &);

        LogicalClockTmpl<unsigned char> m_lcVersion; // 0xc
    };
}