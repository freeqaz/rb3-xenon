#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    template <class T>
    class LogicalClockTmpl : public RootObject {
    public:
        LogicalClockTmpl(T t = 0) : m_tValue(t) {}
        LogicalClockTmpl(const LogicalClockTmpl &o) : m_tValue(o.m_tValue) {}

        T m_tValue; // 0x0
    };
}
