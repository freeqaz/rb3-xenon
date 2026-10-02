#pragma once

namespace Quazal {
    class DOID {
    public:
        DOID(unsigned int ui) : m_uiValue(ui) {}
        DOID(const DOID &id) : m_uiValue(id.m_uiValue) {}
        operator unsigned int() const { return m_uiValue; }
        bool IsNull() const { return (unsigned int)*this == 0; }

        unsigned int m_uiValue; // 0x0
    };
}
