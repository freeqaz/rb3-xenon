#pragma once
#include "Platform/CriticalSection.h"
#include "Platform/MemoryManager.h"
#include "Platform/RootObject.h"

namespace Quazal {

    class Event;

    struct EventFlagTable : public RootObject {
        unsigned char *flags; // 0x0
    };

    class EventHandler : public RootObject {
    public:
        EventHandler(unsigned short);
        ~EventHandler();

        void SetEvent(Event *);
        void ResetEvent(Event *);
        Event *CreateEventObject(unsigned int, unsigned int);

        CriticalSection m_csEventTable; // 0x0
        EventFlagTable *unk14;          // 0x14
        Event **unk18;                  // 0x18
        int unk1c;                      // 0x1c
        unsigned short unk20;           // 0x20
    };
}
