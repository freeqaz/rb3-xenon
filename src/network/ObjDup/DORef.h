#pragma once
#include "DOHandle.h"
#include "Platform/RootObject.h"

namespace Quazal {
    class DuplicatedObject;

    class DORef : public RootObject {
    public:
        DORef();
        DORef(DOHandle);
        DORef(DuplicatedObject *);
        ~DORef();

        DuplicatedObject *GetDOPtr() const { return m_poReferencedDO; }
        DOHandle GetHandle() const { return m_hReferencedDO; }

        DORef &operator=(const DOHandle &);
        void SetSoft();
        void Release();
        void Acquire();

        void EmptyInit() {
            m_bLockRelevance = true;
            m_hReferencedDO = 0;
            m_poReferencedDO = 0;
        }

        DuplicatedObject *m_poReferencedDO; // 0x0
        DOHandle m_hReferencedDO; // 0x4
        bool m_bLockRelevance; // 0x8
    };

    // A DORef that checks the referenced object's DO class.
    template <class T>
    class DORefTemplate : public DORef {
    public:
        DORefTemplate(DOHandle h) : DORef(h) {}
        ~DORefTemplate() {}

        bool IsValid() const;
        T *operator->() const {
            if (!IsValid()) {
                return 0;
            } else {
                return (T *)m_poReferencedDO;
            }
        }
    };
}
