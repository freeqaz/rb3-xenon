#pragma once

#include "Services/GatheringDDL.h"

namespace Quazal {
    class Gathering;
    class GameSession;
    class Message;

    class _DDL_GameSession : public _DDL_Gathering {
    public:
        _DDL_GameSession() {}
        virtual ~_DDL_GameSession() {}
        virtual Gathering *Clone() const;
        virtual String GetGatheringType() const;
        virtual bool IsA(const String &) const;
        virtual bool IsAKindOf(const String &) const;
        virtual void StreamIn(Message *) const;
        virtual void StreamOut(Message *);
    };
}
