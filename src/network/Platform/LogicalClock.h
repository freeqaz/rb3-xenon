#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    template <class T>
    class LogicalClockTmpl : public RootObject {
    public:
        LogicalClockTmpl(T t = 0) : m_tValue(t) {}
        LogicalClockTmpl(const LogicalClockTmpl &o) : m_tValue(o.m_tValue) {}

        int Compare(const LogicalClockTmpl &) const;

        bool operator>(const LogicalClockTmpl &o) const { return Compare(o) > 0; }
        LogicalClockTmpl &operator=(const LogicalClockTmpl &o) {
            m_tValue = o.m_tValue;
            return *this;
        }

        T m_tValue; // 0x0
    };
}
