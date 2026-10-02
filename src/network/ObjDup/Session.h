#pragma once
#include "Core/CallContext.h"
#include "Plugins/StationURL.h"
#include "SessionDDL.h"
#include "ObjDup/WKHandle.h"

namespace Quazal {
    class Session : public _DO_Session {
    public:
        Session();
        virtual ~Session();
        virtual void OperationBegin(DOOperation *);
        virtual void OperationEnd(DOOperation *);
        virtual void Trace(unsigned int) const;

        static bool JoinSessionImpl(CallContext *, const qList<Quazal::StationURL> &);
        static void RegisterWellKnownDOsFactory(void (*)(void));
        static Session *GetInstance();

        // Lane-chosen names: the well-known session handle at 0x82E1050C and
        // the session state read at +0x5F8.
        static DOHandle GetWKHandle() { return s_wkhSession; }
        unsigned char GetSessionState();
        static WKHandle s_wkhSession;
        static bool IsActive();
        static unsigned int GetClassID() { return s_uiDOClassID; }

        static unsigned int s_uiDOClassID;
        static DOHandle s_hSession;
    };
}