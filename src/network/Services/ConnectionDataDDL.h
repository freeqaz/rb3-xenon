#pragma once

#include "Platform/RootObject.h"
#include "Plugins/StationURL.h"

namespace Quazal {
    class Message;

    class _DDL_ConnectionData : public RootObject {
    public:
        _DDL_ConnectionData() {}
        virtual ~_DDL_ConnectionData() {}

        static void Extract(class Message *, class _DDL_ConnectionData *);

        StationURL m_urlStation;        // 0x4 (size 0x58 — based on offset arithmetic)
        unsigned int m_uiConnectionID;  // 0x5c
    };
}
