#pragma once
#include "ObjDup/DORef.h"

namespace Quazal {
    template <class T>
    class DORefTemplate : public DORef {
    public:
        DORefTemplate(DOHandle h) : DORef(h) {}

        bool IsValid();
        T *Get() {
            if (!IsValid())
                return NULL;
            else
                return (T *)m_poReferencedDO;
        }
    };
}
