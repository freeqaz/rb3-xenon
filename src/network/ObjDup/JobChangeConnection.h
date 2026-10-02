#pragma once
#include "Core/StepSequenceJob.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/DORef.h"

namespace Quazal {
    class DOOperation;
    class Station;

    // The job StationManager queues per station to connect or disconnect it.
    // Virtual order from the job vtables (slot 11 GetType, 12
    // GetTargetConnectionState, 13 Trace).
    class JobChangeConnection : public StepSequenceJob {
    public:
        virtual int GetType() const = 0;
        virtual int GetTargetConnectionState() const = 0;
        virtual void Trace(unsigned int);

        bool m_bCancelRequested; // 0x60
        DORef m_refStation; // 0x64
    };

    class JobConnectStation : public JobChangeConnection {
    public:
        JobConnectStation(DOHandle);
        virtual int GetType() const;
        virtual int GetTargetConnectionState() const;
        void QueueOperation(DOOperation *);

        unsigned char m_pad70[0x160];
    };

    class JobDisconnectStation : public JobChangeConnection {
    public:
        JobDisconnectStation(Station *);
        virtual int GetType() const;
        virtual int GetTargetConnectionState() const;

        unsigned char m_pad70[0x10];
    };

}
