#pragma once
#include "StationDDL.h"
#include "ObjDup/DistanceComputationCache.h"
#include "Platform/CriticalSection.h"
#include "Platform/Result.h"

namespace Quazal {
    class EndPoint;
    class Message;
    class StationURL;
    template <class T>
    class qList;
    template <class T>
    class PseudoGlobalVariable;

    // Bookkeeping of the duplication operations a station is updating
    // (0x1c bytes in retail; its ctor/dtor are out of line).
    class UpdateContextMap : public RootObject {
    public:
        UpdateContextMap();
        ~UpdateContextMap();
        unsigned char m_pad[0x1c];
    };

    // The messages a station bundles for one send (0x40 bytes).
    class MessageBundle : public RootObject {
    public:
        MessageBundle();
        ~MessageBundle();
        bool MustFlushBefore(Message *, unsigned int) const;
        void Add(Message *, unsigned int);
        bool MustFlushNow() const;
        qResult Send(EndPoint *);
        bool IsEmpty() const;
        unsigned char m_pad[0x40];
    };

    // A flag set once under its own lock (the station's fault flag).
    class ProtectedFlag {
    public:
        ProtectedFlag() : m_iValue(0), m_csLock(0x40000000) {}
        int TestAndSet();

        int m_iValue; // 0x0
        CriticalSection m_csLock; // 0x4
        unsigned int m_uiUnk18; // 0x18
    };

    class Station : public _DO_Station {
    public:
        enum _State {
        };

        Station();
        virtual ~Station();
        virtual bool ApproveFaultRecovery();
        virtual bool ApproveEmigration(unsigned int);
        virtual bool ValidOperation(DOOperation *);
        virtual void OperationEnd(DOOperation *);
        virtual void Trace(unsigned int) const;
        virtual void TestInvariants();

        // Retail source order (0x82A7B860..0x82A7DE08).
        void ReleaseOwnReference();
        void AcquireOwnReference();
        void SetState(_State);
        void SetAtEOS();
        void ClearAtEOS();
        bool AtEOS();
        void LeaveSessionNormally();
        void MarkAsDisconnected();
        bool IsAPeer();
        bool IsLocal();
        bool DiscoversGlobalObject(DuplicatedObject *);
        bool IsConnected() const;
        bool IsFaulty() const;
        EndPoint *ReleaseConnection();
        void SetConnection(EndPoint *);
        EndPoint *GetEndPoint();
        void ReleaseSystemReferences();
        unsigned int GetInputLatency();
        unsigned int GetOutputLatency();
        static void SetLocalStation(DOHandle);
        static DOHandle GetLocalStation();
        static Station *GetLocalInstance();
        void InitIdentification(StationIdentification *);
        static DOHandle ConvertIDToDOHandle(unsigned int);
        unsigned int GetStationID() const;
        static unsigned int ConvertDOHandleToID(DOHandle);
        bool TestAndSetFaultFlag();
        unsigned int GetProcessType() const;
        bool Send(Message *, unsigned int);
        qResult SendImpl(Message *, unsigned int);
        qResult SendLocalMessage(Message *);
        qResult SendRemoteMessage(Message *, unsigned int);
        void InitLocalStationInfo();
        unsigned int GetMachineUniqueID() const;
        const char *GetStationURL(int);
        void GetStationURLs(qList<StationURL> *);
        qResult FlushBundle(bool);
        static void FlushAllBundles();
        bool SignalFault(bool);
        void SignalAsFaulty(unsigned int);
        static void InitiateFaultProcessingForStation(DOHandle, unsigned int);

        static bool IsLocal(unsigned int ui) { return ui == GetLocalStation().mValue; }
        unsigned short GetState() const { return m_oState.m_ui16State; }

        static PseudoGlobalVariable<DOHandle> s_hLocalStation;
        static unsigned int s_uiDOClassID;
        static unsigned int GetClassID() { return s_uiDOClassID; }

        static Station *DynamicCast(DuplicatedObject *pDO) {
            if (pDO && pDO->IsAKindOf(_DO_Station::s_uiClassID))
                return (Station *)pDO;
            else
                return NULL;
        }

        Station *m_pThis; // 0xb4
        EndPoint *m_pEndPoint; // 0xb8
        UpdateContextMap m_oUpdateContextMap; // 0xbc
        DistanceComputationCache m_oDistanceCache; // 0xd8
        bool m_bAtEOS; // 0xf0
        ProtectedFlag m_oFaultFlag; // 0xf4
        MessageBundle m_oReliableBundle; // 0x110
        MessageBundle m_oUnreliableBundle; // 0x150
        unsigned int m_uiBundleCount; // 0x190
    };
}
