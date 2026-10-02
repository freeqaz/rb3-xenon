#pragma once
#include "DORef.h"
#include "ObjDup/LogicalClock.h"

namespace Quazal {
    class MasterStationRef : public DORef {
    public:
        MasterStationRef();
        MasterStationRef(DOHandle, LogicalClockTmpl<unsigned char>);
        ~MasterStationRef();

        LogicalClockTmpl<unsigned char> m_lcVersion; // 0xc
    };
}