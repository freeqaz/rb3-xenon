#pragma once
#include "DOHandle.h"
#include "Platform/RootObject.h"
#include "Platform/qStd.h"

namespace Quazal {
    class Selection : public qMap<DOHandle, class DuplicatedObject *> {
    public:
        Selection(unsigned char);
        virtual ~Selection();

        void SetFlags(unsigned char);
        void Add(DOHandle);
        void Add(class DuplicatedObject *);
        bool Remove(DOHandle);
        const_iterator find(DOHandle h) const {
            return qMap<DOHandle, class DuplicatedObject *>::find(h);
        }

        unsigned char m_byFlags; // 0x20;
    };
}