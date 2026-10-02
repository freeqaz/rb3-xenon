#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    template <class T>
    class LogicalClockTmpl : public RootObject {
    public:
        LogicalClockTmpl(const LogicalClockTmpl &o) : m_value(o.m_value) {}

        T m_value; // 0x0
    };
}
