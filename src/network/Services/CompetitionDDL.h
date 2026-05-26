#pragma once
#include "Services/GatheringDDL.h"
#include "Platform/qStd.h"

namespace Quazal {
    class _DDL_Competition : public _DDL_Gathering {
    public:
        _DDL_Competition() {}
        virtual ~_DDL_Competition() {}
        virtual Gathering *Clone() const;
        virtual String GetGatheringType() const;
        virtual bool IsA(const String &) const;
        virtual bool IsAKindOf(const String &) const;
        virtual void StreamIn(Message *) const;
        virtual void StreamOut(Message *);

        static void Add(Message *, const _DDL_Competition &);
        static void Extract(Message *, _DDL_Competition *);

        String m_str28;                                    // 0x28
        // qList at 0x2c (intrusive list, two pointers = 8 bytes)
        unsigned int m_listHead;                           // 0x2c
        unsigned int m_listTail;                           // 0x30
    };
}
