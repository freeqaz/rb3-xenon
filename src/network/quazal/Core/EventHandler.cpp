// Quazal NetZ - .\Core\EventHandler.cpp
//
// Retail TU: .text 0x82AF2388..0x82AF2DC8, compiled /Od /Oi- /Ob1 /EHs-c-
// (see objects.json). It starts right after the StringConverter object's
// destructor and ends where the PerfCounter object's constructor begins. Its
// .rdata is four copies of the __FILE__ string and nothing else: no vtable and
// no EH tables, although every member function but the constructor and
// destructor holds a ScopedCS (no .pdata record has the EH bit).
//
// The handler keeps one kernel event per slot (m_pEventTable->m_phEvents) and
// the Event object that owns the slot (m_ppEvents). The classes are declared
// here with the layouts retail uses: the shared EventHandler.h types the
// handle table as a byte pointer and gives its holder no constructor, while
// retail stores HANDLEs and null-checks the holder's allocation.
//
// This TU is built /Od: its locals are laid out by a walk over the scope's
// symbol hash table, so the local NAMES below determine the stack offsets, and
// the allocations pass __FILE__/__LINE__, so #line reproduces retail's lines.

#include "Platform/CriticalSection.h"
#include "Platform/RootObject.h"
#include "Platform/ScopedCS.h"
#include "Platform/SystemError.h"

extern "C" {
typedef void *HANDLE;
void *memset(void *, int, unsigned int);
HANDLE CreateEventA(void *, int, int, const char *);
int SetEvent(HANDLE);
int ResetEvent(HANDLE);
int CloseHandle(HANDLE);
unsigned long WaitForMultipleObjects(unsigned long, const HANDLE *, int, unsigned long);
unsigned long GetLastError();
}

#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define WAIT_FAILED ((unsigned long)0xFFFFFFFF)
#define WAIT_TIMEOUT 0x102

namespace Quazal {
    // Array new/delete with an element-count header; defined in another TU.
    template <class T>
    T *qNewArray(unsigned int count, const char *file, int line);
    template <class T>
    void qDeleteArray(T *arr);

    class EventHandler;

    class Event : public RootObject {
    public:
        Event(EventHandler *, unsigned int, unsigned int);
        ~Event();
        void Set();
        void Reset();

        EventHandler *m_pHandler; // 0x0
        unsigned int m_uiStart; // 0x4
        unsigned int m_uiEnd; // 0x8
        unsigned int m_unkc; // 0xc
    };

    class EventTable : public RootObject {
    public:
        EventTable() {}

        HANDLE *m_phEvents; // 0x0
    };

    class EventHandler : public RootObject {
    public:
        EventHandler(unsigned short);
        ~EventHandler();

        Event *CreateEventObject(unsigned int, unsigned int);
        void DeleteEventObject(Event *);
        unsigned short GetEventIndex(Event *);
        void SetEvent(Event *);
        HANDLE GetEventHandle(Event *);
        void ResetEvent(Event *);
        bool IsSignaled(Event *);
        bool WaitForEvent(unsigned int, Event **) const;
        bool WaitForEventImpl(unsigned int, Event **) const;

        CriticalSection m_csEventTable; // 0x0
        EventTable *m_pEventTable; // 0x14
        Event **m_ppEvents; // 0x18
        int m_iNbEvents; // 0x1c
        unsigned short m_usMaxNbEvents; // 0x20
    };

    EventHandler::EventHandler(unsigned short usMaxNbEvents) : m_csEventTable(0x40000000) {
#line 47
        m_pEventTable = new (__FILE__, __LINE__) EventTable;
        m_usMaxNbEvents = usMaxNbEvents;
#line 50
        m_pEventTable->m_phEvents = qNewArray<HANDLE>(m_usMaxNbEvents, __FILE__, __LINE__);
        for (int i = 0; i < m_usMaxNbEvents; i++) {
            m_pEventTable->m_phEvents[i] = CreateEventA(0, 0, 0, 0);
            ::ResetEvent(m_pEventTable->m_phEvents[i]);
        }
#line 76
        m_ppEvents = qNewArray<Event *>(m_usMaxNbEvents, __FILE__, __LINE__);
        memset(m_ppEvents, 0, m_usMaxNbEvents * sizeof(Event *));
        m_iNbEvents = 0;
    }

    EventHandler::~EventHandler() {
        for (int i = 0; i < m_usMaxNbEvents; i++) {
            if (m_pEventTable->m_phEvents[i] != INVALID_HANDLE_VALUE) {
                CloseHandle(m_pEventTable->m_phEvents[i]);
            }
        }
        qDeleteArray(m_ppEvents);
        qDeleteArray(m_pEventTable->m_phEvents);
        delete m_pEventTable;
    }

    Event *EventHandler::CreateEventObject(unsigned int uiStart, unsigned int uiEnd) {
        ScopedCS oCS(m_csEventTable);
        int i;
        for (i = 0; i < m_usMaxNbEvents; i++) {
            if (m_ppEvents[i] == 0) {
                break;
            }
        }
#line 110
        m_ppEvents[i] = new (__FILE__, __LINE__) Event(this, uiStart, uiEnd);
        m_iNbEvents++;
        ResetEvent(m_ppEvents[i]);
        Event *pEvent = m_ppEvents[i];
        return pEvent;
    }

    void EventHandler::DeleteEventObject(Event *pEvent) {
        ScopedCS oCS(m_csEventTable);
        ResetEvent(pEvent);
        m_ppEvents[GetEventIndex(pEvent)] = 0;
        delete pEvent;
    }

    unsigned short EventHandler::GetEventIndex(Event *pEvent) {
        ScopedCS oCS(m_csEventTable);
        unsigned short i;
        for (i = 0; i < m_usMaxNbEvents && m_ppEvents[i] != pEvent; i++) {
        }
        if (i == m_usMaxNbEvents) {
            return 0;
        } else {
            return i;
        }
    }

    void EventHandler::SetEvent(Event *pEvent) {
        ::SetEvent(m_pEventTable->m_phEvents[GetEventIndex(pEvent)]);
    }

    HANDLE EventHandler::GetEventHandle(Event *pEvent) {
        return m_pEventTable->m_phEvents[GetEventIndex(pEvent)];
    }

    void EventHandler::ResetEvent(Event *pEvent) {
        ::ResetEvent(m_pEventTable->m_phEvents[GetEventIndex(pEvent)]);
    }

    bool EventHandler::IsSignaled(Event *pEvent) {
        return WaitForMultipleObjects(
                   1, &m_pEventTable->m_phEvents[GetEventIndex(pEvent)], 0, 0
               )
            == 0;
    }

    bool EventHandler::WaitForEvent(unsigned int uiTimeout, Event **ppEvent) const {
        return WaitForEventImpl(uiTimeout, ppEvent);
    }

    bool EventHandler::WaitForEventImpl(unsigned int uiTimeout, Event **ppEvent) const {
        bool bContinue = true;
        while (bContinue) {
            unsigned short uiIndex = 0xFFFF;
            unsigned long ulResult = WaitForMultipleObjects(
                m_usMaxNbEvents, m_pEventTable->m_phEvents, 0, uiTimeout
            );
            switch (ulResult) {
            case WAIT_FAILED:
                SystemError::SignalError(0, 0, 0xE0000002, GetLastError());
                return false;
                break;
            case WAIT_TIMEOUT:
                SystemError::SignalError(0, 0, 0xE000000C, 0);
                return false;
            }
            {
                ScopedCS oCS(const_cast<CriticalSection &>(m_csEventTable));
                uiIndex = (unsigned short)ulResult;
                if (m_ppEvents[uiIndex] != 0) {
                    m_ppEvents[uiIndex]->Reset();
                    *ppEvent = m_ppEvents[uiIndex];
                    return true;
                }
            }
        }
        return false;
    }
}
