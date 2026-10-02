#pragma once
#include "Platform/RootObject.h"
#include <stdlib.h>

namespace Quazal {
    template <class T>
    class LogicalClockTmpl : public RootObject {
    public:
        LogicalClockTmpl(T value) : m_value(value) {}
        LogicalClockTmpl(const LogicalClockTmpl &o) : m_value(o.m_value) {}

        int Compare(const LogicalClockTmpl &o) const {
            int iThis = m_value;
            int iOther = o.m_value;
            if (abs(iThis - iOther) < 0x80) {
                return iThis - iOther;
            } else if (iThis < iOther) {
                return iThis + 0x100 - iOther;
            } else {
                return iThis - (iOther + 0x100);
            }
        }
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
