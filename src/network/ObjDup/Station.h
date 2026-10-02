#pragma once
#include "StationDDL.h"

namespace Quazal {

    class Station : public _DO_Station {
    public:
        Station();
        virtual ~Station();
        virtual bool ApproveFaultRecovery();
        virtual bool ApproveEmigration(unsigned int);
        virtual bool ValidOperation(DOOperation *);
        virtual void OperationEnd(DOOperation *);
        virtual void Trace(unsigned int) const;
        virtual void TestInvariants();

        int GetStationID() const;

        static Station *GetLocalInstance();
        static DOHandle GetLocalStationHandle();
        static unsigned int GetStationIDFromHandle(DOHandle);
        void OnStationDOReleased();
        void SendMessage(Message *, unsigned int);
        bool IsAPeer();
        bool IsConnected();
        unsigned int GetProcessType() const;
        unsigned short GetState() const { return m_usState; }

        static unsigned int s_uiDOClassID;
        static unsigned int GetClassID() { return s_uiDOClassID; }

        unsigned char unk70[0x40]; // 0x70
        unsigned short m_usState; // 0xb0
    };
}
