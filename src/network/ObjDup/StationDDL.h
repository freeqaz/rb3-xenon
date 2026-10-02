#pragma once
#include "RootDO.h"
#include "ObjDup/ConnectionInfoDDL.h"
#include "ObjDup/StationIdentificationDDL.h"
#include "ObjDup/StationInfoDDL.h"
#include "ObjDup/StationStateDDL.h"

namespace Quazal {
    class StationURL;
    class RMCContext;

    // The user dataset classes over the DDL-generated ones. Retail calls
    // ConnectionInfo's, StationInfo's and StationState's destructors out of
    // line from the _DO_Station destructor (0x82A7BBF0), and expands
    // StationIdentification's.
    class ConnectionInfo : public _DS_ConnectionInfo {
    public:
        ConnectionInfo();
        ~ConnectionInfo();
        const char *GetURL(int) const;
    };

    class StationIdentification : public _DS_StationIdentification {
    public:
        StationIdentification();
    };

    class StationInfo : public _DS_StationInfo {
    public:
        StationInfo();
        ~StationInfo();
        unsigned int GetMachineUniqueID() const;
        void InitMachineUniqueID();
    };

    class StationState : public _DS_StationState {
    public:
        StationState();
        ~StationState();
        void Set(unsigned short);
        void OperationBegin(DOOperation *);
        void OperationEnd(DOOperation *);
    };

    class _DO_Station : public RootDO {
    public:
        _DO_Station();
        virtual ~_DO_Station() {}
        virtual void CallOperationOnDatasets(DOOperation *, Operation::_Event);
        virtual bool IsACoreDO() const;
        virtual bool IsABootstrapDO() const;

        static void InitDOClass(unsigned int);
        static DuplicatedObject *Create(unsigned int);
        static DOHandle GetIDGenerator();
        bool SpecificUpdate(DataSet *, const Time &);
        bool SpecificRefresh(DataSet *, const Time &);
        void AddDSToDiscoveryMessage(Message *, Station *);
        bool ExtractDSFromDiscoveryMessage(Message *);
        bool ExtractADataset(Message *, unsigned char);
        bool CallSignalAsFaulty(RMCContext *, const unsigned int &);
        void DispatchSignalAsFaulty(const CallMethodOperation &);

        static unsigned int GetStaticClassID() { return s_uiClassID; }
        static unsigned int s_uiClassID;

        ConnectionInfo m_oConnectionInfo; // 0x70
        StationIdentification m_oIdentification; // 0x98
        StationInfo m_oStationInfo; // 0xa8
        StationState m_oState; // 0xb0
    };

    class _DOC_Station : public DOClassTemplate<_DO_Station, _DOC_RootDO> {
    public:
        _DOC_Station(unsigned int);
        virtual void DataSetsOperation(unsigned int);
        virtual DuplicatedObject *Create();
        virtual void Delete(DuplicatedObject *);
        virtual bool ApproveFaultRecovery(DuplicatedObject *);
        virtual bool ApproveEmigration(DuplicatedObject *, unsigned int);
        virtual void Trace(DuplicatedObject *, unsigned int);
        virtual bool IsAKindOf(unsigned int);
        virtual const char *GetClassNameString() const;
        virtual const char *GetDatasetNameString(unsigned char) const;
        virtual bool
        FormatVariableValue(const DuplicatedObject *, Variable *, Variable *, String *) const;
        virtual bool DispatchAction(DuplicatedObject *, unsigned short, Message *);
        virtual void DispatchRMCCall(const CallMethodOperation &);
        virtual bool DispatchRMCResult(RMCContext *);
        virtual void FillDupSpacesInfo(DupSpace::_Role, unsigned int *, unsigned int *);

        unsigned short m_usSignalAsFaultyID; // 0x30
    };
}
