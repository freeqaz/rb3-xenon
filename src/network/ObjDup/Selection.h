#pragma once
#include "DOHandle.h"
#include "Platform/RootObject.h"
#include "Platform/qStd.h"

namespace Quazal {
    class DuplicatedObject;

    class Selection : public RootObject {
    public:
        Selection(unsigned char);
        virtual ~Selection();

        void SetFlags(unsigned char);
        bool Add(DuplicatedObject *);
        bool Remove(const DuplicatedObject *);

        qMap<DOHandle, class DuplicatedObject *> m_map; // 0x4
        unsigned char m_byFlags; // 0x20;
    };
}