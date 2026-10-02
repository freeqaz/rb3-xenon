#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    template <class T>
    class LogicalClockTmpl : public RootObject {
    public:
        LogicalClockTmpl(unsigned int ui = 0) : m_tValue(ui) {}
        LogicalClockTmpl(const LogicalClockTmpl &o) : m_tValue(o.m_tValue) {}

        T m_tValue; // 0x0
    };
}
