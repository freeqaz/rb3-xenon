#pragma once
#include "Services/CompetitionDDL.h"

namespace Quazal {
    class _DDL_Tournament : public _DDL_Competition {
    public:
        _DDL_Tournament() {}
        virtual ~_DDL_Tournament() {}
        virtual Gathering *Clone() const;
        virtual String GetGatheringType() const;
        virtual bool IsA(const String &) const;
        virtual bool IsAKindOf(const String &) const;
        virtual void StreamIn(Message *) const;
        virtual void StreamOut(Message *);
    };
}
