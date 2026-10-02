#pragma once

namespace Quazal {
    template <class T>
    class LogicalClockTmpl {
    public:
        int Compare(const LogicalClockTmpl &) const;

        bool operator>(const LogicalClockTmpl &o) const { return Compare(o) > 0; }
        LogicalClockTmpl &operator=(const LogicalClockTmpl &o) {
            m_tValue = o.m_tValue;
            return *this;
        }

        T m_tValue; // 0x0
    };
}
