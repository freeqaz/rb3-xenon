#pragma once
#include "Services/Data.h"
#include "Platform/String.h"

namespace Quazal {
    class NintendoToken;

    class _DDL_NintendoToken : public Data {
    public:
        _DDL_NintendoToken() {}
        virtual ~_DDL_NintendoToken() {}
        virtual Data *Clone() const;
        virtual String GetDataType() const;
        virtual bool IsA(const String &) const;
        virtual bool IsAKindOf(const String &) const;
        virtual void StreamIn(Message *) const;
        virtual void StreamOut(Message *);

        String m_strToken; // 0x4
    };
}
