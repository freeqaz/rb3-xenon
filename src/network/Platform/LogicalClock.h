#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    template <class T>
    class LogicalClockTmpl : public RootObject {
    public:
        LogicalClockTmpl(T value = 0) : m_value(value) {}
        LogicalClockTmpl(const LogicalClockTmpl &o) : m_value(o.m_value) {}

        // Out of line in retail (0x82A76A08 for T = unsigned char).
        int Compare(const LogicalClockTmpl &) const;

        bool operator==(const LogicalClockTmpl &o) const { return m_value == o.m_value; }
        bool operator!=(const LogicalClockTmpl &o) const { return !(*this == o); }
        bool operator>(const LogicalClockTmpl &o) const { return Compare(o) > 0; }
        bool operator>=(const LogicalClockTmpl &o) const { return *this == o || *this > o; }
        LogicalClockTmpl &operator=(const LogicalClockTmpl &o) {
            m_value = o.m_value;
            return *this;
        }

        T m_value; // 0x0
    };
}
