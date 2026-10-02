#pragma once
#include "DOHandle.h"
#include "Platform/RootObject.h"
#include "Platform/SystemError.h"

namespace Quazal {
    class DuplicatedObject;

    class DORef : public RootObject {
    public:
        DORef();
        DORef(DOHandle);
        DORef(DuplicatedObject *);
        ~DORef();

        DOHandle GetHandle() const { return m_hReferencedDO; }

        void SetSoft();
        void Release();
        void Acquire();

        unsigned int GetReferencedHandle() const { return m_hReferencedDO.mValue; }

        // Lane-chosen names for the class-checked accessors retail instantiates
        // at the end of the TU.
        template <class T>
        bool IsA() const {
            if (!m_poReferencedDO) {
                SystemError::SignalError(0, 0, 0xA0030004, 0);
                return false;
            } else {
                T *pDO = (T *)m_poReferencedDO;
                if (!T::GetDOClass(pDO->m_dohMyself.GetDOClassID())
                         ->IsAKindOf(T::GetStaticClassID())) {
                    SystemError::SignalError(0, 0, 0xE003000C, 0);
                    return false;
                }
                return true;
            }
        }

        template <class T>
        T *Get() const {
            if (!IsA<T>())
                return 0;
            else
                return (T *)m_poReferencedDO;
        }

        void EmptyInit() {
            m_bLockRelevance = true;
            m_hReferencedDO = 0;
            m_poReferencedDO = 0;
        }

        DuplicatedObject *m_poReferencedDO; // 0x0
        DOHandle m_hReferencedDO; // 0x4
        bool m_bLockRelevance; // 0x8
    };
}