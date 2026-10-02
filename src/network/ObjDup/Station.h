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
        // Lane-chosen names: retail wrappers around Acquire/ReleaseReferenceToMaster.
        void AcquireStationReference();
        void ReleaseStationReference();

        static Station *GetLocalInstance();
        static DOHandle GetLocalStation();
        void SendMessage(Message *, bool);

        static Station *DynamicCast(DuplicatedObject *pDO) {
            if (pDO && pDO->IsAKindOf(_DO_Station::s_uiClassID))
                return (Station *)pDO;
            else
                return NULL;
        }
    };
}