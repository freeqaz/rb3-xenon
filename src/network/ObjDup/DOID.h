#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class DOID : public RootObject {
    public:
        DOID(unsigned int ui = 0) : m_uiValue(ui) {}
        DOID(const DOID &o) : m_uiValue(o.m_uiValue) {}
        operator unsigned int() const { return m_uiValue; }
        bool IsNull() const { return (unsigned int)*this == 0; }

        unsigned int m_uiValue; // 0x0
    };
}
