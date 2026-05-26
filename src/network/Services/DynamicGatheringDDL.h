#pragma once
#include "Services/Gathering.h"
#include "Plugins/Buffer.h"

namespace Quazal {
    class DynamicGathering;

    class _DDL_DynamicGathering : public Gathering {
    public:
        _DDL_DynamicGathering() : m_Buffer(0x400) {}
        virtual ~_DDL_DynamicGathering() {}
        virtual Gathering *Clone() const;
        virtual String GetGatheringType() const;
        virtual bool IsA(const String &) const;
        virtual bool IsAKindOf(const String &) const;
        virtual void StreamIn(Message *) const;
        virtual void StreamOut(Message *);

        Buffer m_Buffer; // 0x28
    };
}
