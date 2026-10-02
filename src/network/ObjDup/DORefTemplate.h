#pragma once
#include "ObjDup/DORef.h"

namespace Quazal {
    // A DORef that checks the referenced object's DO class. Lane-chosen name;
    // IsValid is instantiated out of line at the end of the TU (0x82A76640
    // for Station, 0x82A76568 for DuplicatedObject).
    template <class T>
    class DORefTemplate : public DORef {
    public:
        DORefTemplate(DOHandle h) : DORef(h) {}
        ~DORefTemplate() {}

        bool IsValid() const;
        T *Get() const {
            if (!IsValid())
                return NULL;
            else
                return (T *)m_poReferencedDO;
        }
        T *operator->() const {
            if (!IsValid()) {
                return 0;
            } else {
                return (T *)m_poReferencedDO;
            }
        }
    };
}
