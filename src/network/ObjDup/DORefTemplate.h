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

        // Inline: /Ob1 declines it, so callers call the one out-of-line copy
        // (the first TU to use it emits it) yet still reserve its frame.
        bool IsValid() const {
            if (GetDOPtr() == 0) {
                SystemError::SignalError(0, 0, 0xA0030004, 0);
                return false;
            } else {
                if (!T::GetDOClass(((T *)GetDOPtr())->m_dohMyself.GetDOClassID())
                         ->IsAKindOf(T::GetClassID())) {
                    SystemError::SignalError(0, 0, 0xE003000C, 0);
                    return false;
                }
                return true;
            }
        }
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
                return (T *)GetDOPtr();
            }
        }
    };
}
