// Quazal NetZ - .\StationDDL.cpp
//
// The DDL-generated code for the Station duplicated object. The retail TU is
// 0x82A82830..0x82A847B8: the _DOC_Station class object, the _DO_Station
// dataset dispatchers, and the update protocols, filter and map lookup this
// TU instantiates. It is built /Od /Ob1 with EH and RTTI off (no unwind
// records, and no object locator in front of any of its vtables).
//
// The DO has four datasets, numbered as the DDL declares them:
//   1 ConnectionInfo        at 0x70
//   2 StationIdentification at 0x98
//   3 StationInfo           at 0xA8
//   4 StationState          at 0xB0
// Each dataset is driven by the UpdatePolicy its DO class registered for that
// index. The surrounding NetZ classes are declared here only as far as this TU
// uses them; their members are defined in other TUs.

#include "Platform/qStd.h"

namespace Quazal {

    class Message;
    class Station;
    class DuplicatedObject;
    class CallMethodOperation;

    class String : public RootObject {
    public:
        String();
        String(const char *);
        String(const String &);
        ~String();
        static bool IsEqual(const char *, const char *);

        char *m_szContent;
    };

    class Time {
    public:
        unsigned long long m_ullValue;
    };

    class DOID {
    public:
        DOID(unsigned int ui = 0) : m_uiValue(ui) {}
        DOID(const DOID &o) : m_uiValue(o.m_uiValue) {}

        unsigned int m_uiValue;
    };

    class DOHandle {
    public:
        DOHandle(unsigned int val = 0) : mValue(val) {}
        DOHandle(const DOHandle &h) : mValue(h.mValue) {}

        unsigned int GetDOClassID() const { return (mValue & 0xFFC00000) >> 22; }
        unsigned int GetID() const {
            unsigned int uiID = mValue & 0x3FFFFF;
            return uiID;
        }
        void SetDOClassID(unsigned int);

        unsigned int mValue;
    };

    class SystemError {
    public:
        static void SignalError(const char *, unsigned int, unsigned int, unsigned int);
    };

    class ByteStream : public RootObject {
    public:
        void Append(const void *, unsigned int, bool);
        void Extract(void *, unsigned int, bool);
        ByteStream &operator>>(bool &);

        template <class T>
        ByteStream &operator<<(const T &t) {
            Append(&t, sizeof(T), true);
            return *this;
        }
        template <class T>
        ByteStream &operator>>(T &t) {
            Extract(&t, sizeof(T), true);
            return *this;
        }
    };

    ByteStream &operator>>(ByteStream &, DOHandle &);

    class Message : public ByteStream {};

    class qResult {
    public:
        qResult(const int &);
        qResult &operator=(const qResult &);

        int m_iCode;
        int m_iLine;
        const char *m_szFile;
    };

    class Variable {
    public:
        char m_pad0[8];
        const char *m_szName; // 0x8
    };

    class MethodIDGenerator {
    public:
        static unsigned short AssignID(String);
        static unsigned short GetID(String);
    };

    class RMCContext {
    public:
        bool PrepareCallMessage(DOHandle, unsigned short);
        Message *GetCallMessage();
        bool PerformCallAndWait();
        Message *GetResponseMessage();

        unsigned short GetMethodID() const { return m_usMethodID; }

        char m_pad0[0xA0];
        unsigned short m_usMethodID; // 0xA0
    };

    class _Type_string {
    public:
        static void Extract(Message *, String *);
    };

    // ---------------------------------------------------------------------
    // Datasets

    class DataSet : public RootObject {
    public:
        DataSet();
        bool Refresh(const Time &);
    };

    class Operation {
    public:
        enum _Event {
            Begin = 0,
            End = 1
        };
    };

    class DOOperation {
    public:
        virtual ~DOOperation();
        virtual unsigned short GetSignal() const;
        virtual int GetType() const = 0;
        virtual const char *GetClassNameString() const = 0;
        virtual void ForceImplOperationCommonMethodsMacro() = 0;
        virtual void TraceImpl(Operation::_Event, unsigned int) const = 0;
        virtual DOHandle GetImplicitStationConnection() const;
        virtual DOOperation *Clone() const;
        virtual bool CallsBackOnDataSet() = 0;
        virtual bool CallsBackOnDataSet(unsigned char) = 0;
    };

    class Variable;

    class _DS_ConnectionInfo : public DataSet {
    public:
        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);
        void ExtractFrom(Message *pMsg) {
            *pMsg >> m_bURLInitialized;
            _Type_string::Extract(pMsg, &m_strStationURL1);
            _Type_string::Extract(pMsg, &m_strStationURL2);
            _Type_string::Extract(pMsg, &m_strStationURL3);
            _Type_string::Extract(pMsg, &m_strStationURL4);
            _Type_string::Extract(pMsg, &m_strStationURL5);
            pMsg->Extract(&m_uiInputBandwidth, 4, true);
            pMsg->Extract(&m_uiInputLatency, 4, true);
            pMsg->Extract(&m_uiOutputBandwidth, 4, true);
            pMsg->Extract(&m_uiOutputLatency, 4, true);
        }

        bool m_bURLInitialized; // 0x0
        String m_strStationURL1; // 0x4
        String m_strStationURL2; // 0x8
        String m_strStationURL3; // 0xc
        String m_strStationURL4; // 0x10
        String m_strStationURL5; // 0x14
        unsigned int m_uiInputBandwidth; // 0x18
        unsigned int m_uiInputLatency; // 0x1c
        unsigned int m_uiOutputBandwidth; // 0x20
        unsigned int m_uiOutputLatency; // 0x24
    };

    class ConnectionInfo : public _DS_ConnectionInfo {
    public:
        ConnectionInfo();
    };

    class _DS_StationIdentification : public DataSet {
    public:
        _DS_StationIdentification() {
            m_uiProcessType = 0;
            m_uiProductVersion = 0;
        }
        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);
        void ExtractFrom(Message *pMsg) {
            _Type_string::Extract(pMsg, &m_strIdentificationToken);
            _Type_string::Extract(pMsg, &m_strProcessName);
            pMsg->Extract(&m_uiProcessType, 4, true);
            pMsg->Extract(&m_uiProductVersion, 4, true);
        }

        String m_strIdentificationToken; // 0x0
        String m_strProcessName; // 0x4
        unsigned int m_uiProcessType; // 0x8
        unsigned int m_uiProductVersion; // 0xc
    };

    class StationIdentification : public _DS_StationIdentification {};

    class _DS_StationInfo : public DataSet {
    public:
        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);
        void ExtractFrom(Message *pMsg) {
            *pMsg >> m_hObserver;
            pMsg->Extract(&m_uiMachineUID, 4, true);
        }

        DOHandle m_hObserver; // 0x0
        unsigned int m_uiMachineUID; // 0x4
    };

    class StationInfo : public _DS_StationInfo {
    public:
        StationInfo();
    };

    class _DS_StationState : public DataSet {
    public:
        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);
        void ExtractFrom(Message *pMsg) { pMsg->Extract(&m_ui16State, 2, true); }

        unsigned short m_ui16State; // 0x0
    };

    class StationState : public _DS_StationState {
    public:
        StationState();
        void OperationBegin(DOOperation *);
        void OperationEnd(DOOperation *);

        unsigned int m_uiPad4; // 0x4
    };

    // ---------------------------------------------------------------------
    // Update policies

    class UpdateProtocol : public RootObject {
    public:
        UpdateProtocol() {}
        virtual ~UpdateProtocol() {}
        virtual bool UsesSpecialInnerUpdateLoop() { return false; }
        virtual unsigned int SpecialInnerUpdateLoop(DuplicatedObject *, void *, unsigned char, Time) {
            unsigned int uiNbUpdates = 0;
            return uiNbUpdates;
        }
        virtual unsigned int GetCommunicationFlags(DuplicatedObject *, void *, unsigned char) = 0;
        virtual void AddToMessage(DuplicatedObject *, void *, unsigned char, Time, Message *, bool) = 0;
        virtual void ExtractFromMessage(DuplicatedObject *, void *, unsigned char, Message *, bool) = 0;
    };

    template <class T>
    class BasicUpdateProtocol : public UpdateProtocol {
    public:
        static bool UsesReliableUpdates() { return true; }
        virtual unsigned int GetCommunicationFlags(DuplicatedObject *, void *, unsigned char) {
            if (UsesReliableUpdates())
                return 1;
            else
                return 0;
        }
        virtual void AddToMessage(DuplicatedObject *, void *pData, unsigned char, Time t, Message *pMsg, bool b) {
            static_cast<T *>(pData)->AddSourceTo(pMsg, t, b);
        }
        virtual void ExtractFromMessage(DuplicatedObject *, void *pData, unsigned char, Message *pMsg, bool) {
            static_cast<T *>(pData)->ExtractFrom(pMsg);
        }
    };

    class GlobalUpdateFilter : public RootObject {
    public:
        virtual ~GlobalUpdateFilter() {}
        virtual bool UpdateRequired(DuplicatedObject *, void *, Time) = 0;
        virtual void UpdateSent(DuplicatedObject *, void *, Time, unsigned int) {}
    };

    class ConstFilter : public GlobalUpdateFilter {
    public:
        virtual ~ConstFilter() {}
        virtual bool UpdateRequired(DuplicatedObject *, void *, Time) {
            bool bRequired = false;
            return bRequired;
        }
    };

    class UpdatePolicy {
    public:
        void Update(DuplicatedObject *, void *, unsigned char, Time);
        void AddToDiscoveryMessage(DuplicatedObject *, void *, unsigned char, Station *, Message *);
        void ExtractFromDiscoveryMessage(DuplicatedObject *, void *, unsigned char, Message *);
        void ExtractFromUpdateMessage(DuplicatedObject *, void *, unsigned char, Message *);
        void AddFilter(GlobalUpdateFilter *);
        void RegisterProtocol(UpdateProtocol *);
    };

    // ---------------------------------------------------------------------
    // Duplicated objects

    class DOClass;

    class DuplicatedObject : public RootObject {
    public:
        static void *operator new(size_t, const char *, unsigned int);

        virtual ~DuplicatedObject();
        virtual bool HasGlobalDOProperty() const;
        virtual bool HasForcedNonGlobalProperty() const;
        virtual bool ApproveFaultRecovery();
        virtual bool ApproveEmigration(unsigned int);
        virtual void InitDO();
        virtual float ComputeDistance(DuplicatedObject *);
        virtual void CallOperationOnDatasets(DOOperation *, Operation::_Event);
        virtual bool ValidOperation(DOOperation *);
        virtual void OperationBegin(DOOperation *);
        virtual void OperationEnd(DOOperation *);
        virtual float GetWeight();
        virtual void Trace(unsigned int) const;
        virtual void CallOperationEndOnAdapters(DOOperation *);
        virtual void TestInvariants();
        virtual bool IsACoreDO() const = 0;
        virtual bool IsABootstrapDO() const;
        virtual void UpdateCellStats(int, int, int);

        static DOClass *GetDOClass(unsigned int);
        static DuplicatedObject *Create(unsigned int, unsigned int);
        DOClass *GetDOClass() const { return GetDOClass(m_dohMyself.GetDOClassID()); }
        void SetDOClassID(unsigned int ui) { m_dohMyself.SetDOClassID(ui); }

        DOHandle GetHandle() const {
            DOID oID = m_dohMyself.GetID();
            if (oID.m_uiValue == 0) {
                SystemError::SignalError(0, 0, 0xE000000E, 0);
                return DOHandle(0);
            } else {
                return m_dohMyself;
            }
        }

        char m_pad4[0x44];
        DOHandle m_dohMyself; // 0x48
        char m_pad4c[0x24];
    };

    class _DO_RootDO : public DuplicatedObject {
    public:
        virtual void CallOperationOnDatasets(DOOperation *, Operation::_Event);
        bool SpecificUpdate(DataSet *, const Time &);
        bool SpecificRefresh(DataSet *, const Time &);
        void AddDSToDiscoveryMessage(Message *, Station *);
        void ExtractDSFromDiscoveryMessage(Message *);
        bool ExtractADataset(Message *, unsigned char);
    };

    class RootDO : public _DO_RootDO {
    public:
        RootDO();
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
        void ExtractDSFromDiscoveryMessage(Message *);
        bool ExtractADataset(Message *, unsigned char);
        bool CallSignalAsFaulty(RMCContext *, const unsigned int &);
        void DispatchSignalAsFaulty(const CallMethodOperation &);

        static unsigned int GetStaticClassID() { return s_uiClassID; }
        static unsigned int s_uiClassID;

        ConnectionInfo m_dsConnectionInfo; // 0x70
        _DS_StationIdentification m_dsIdentification; // 0x98
        StationInfo m_dsInfo; // 0xa8
        StationState m_dsStationState; // 0xb0
    };

    class Station : public _DO_Station {
    public:
        Station();
        void SignalAsFaulty(unsigned int);

        char m_padb8[0xE0];
    };

    // ---------------------------------------------------------------------
    // DO classes

    class DupSpace {
    public:
        enum _Role {
        };
    };

    class CallMethodOperation {
    public:
        Message *GetCallMessage() const;
        Message *PrepareSuccessMessage() const;
        void SetReturnValue(qResult oResult) const { m_oResult = oResult; }
        unsigned short GetMethodID() const { return m_usMethodID; }
        DuplicatedObject *GetTargetObject() const { return m_poTarget; }

        char m_pad0[0x14];
        DuplicatedObject *m_poTarget; // 0x14
        char m_pad18[0x1A];
        unsigned short m_usMethodID; // 0x32
        char m_pad34[0xC];
        mutable qResult m_oResult; // 0x40
    };

    class Adapter;

    class DOClass : public RootObject {
    public:
        DOClass(unsigned int);
        virtual ~DOClass();
        virtual unsigned int GetCategory();
        virtual void DataSetsOperation(unsigned int);
        virtual DuplicatedObject *Create() = 0;
        virtual void Delete(DuplicatedObject *) = 0;
        virtual bool ApproveFaultRecovery(DuplicatedObject *) = 0;
        virtual bool ApproveEmigration(DuplicatedObject *, unsigned int) = 0;
        virtual void Trace(DuplicatedObject *, unsigned int) = 0;
        virtual Adapter *CreateAdapter(DuplicatedObject *, unsigned int);
        virtual void DeleteAdapter(DuplicatedObject *, unsigned int, Adapter *);
        virtual void SpecificAddDSToDiscoveryMessage(DuplicatedObject *, Station *, Message *) = 0;
        virtual void SpecificExtractDSFromDiscoveryMessage(DuplicatedObject *, Message *) = 0;
        virtual bool SpecificExtractADataset(DuplicatedObject *, Message *, unsigned char) = 0;
        virtual bool SpecificUpdate(DuplicatedObject *, DataSet *, const Time &) = 0;
        virtual bool SpecificRefresh(DuplicatedObject *, DataSet *, const Time &) = 0;
        virtual bool IsAKindOf(unsigned int);
        virtual const char *GetClassNameString() const = 0;
        virtual const char *GetDatasetNameString(unsigned char) const;
        virtual bool FormatVariableValue(const DuplicatedObject *, Variable *, Variable *, String *) const;
        virtual bool DispatchAction(DuplicatedObject *, unsigned short, Message *);
        virtual void DispatchRMCCall(const CallMethodOperation &);
        virtual bool DispatchRMCResult(RMCContext *);
        virtual bool ValidCastTowards(unsigned int);
        virtual void FillDupSpacesInfo(DupSpace::_Role, unsigned int *, unsigned int *);

        void CreateUpdatePolicy(unsigned char);
        UpdatePolicy *GetUpdatePolicy(unsigned char ucIndex) {
            return m_mapUpdatePolicies.find(ucIndex)->second;
        }
        void CompleteInitialisation();
        DOHandle GetIDGenerator() const;

        char m_pad4[8];
        qMap<unsigned char, UpdatePolicy *> m_mapUpdatePolicies; // 0xc
    };

    template <class DOType, class Parent>
    class DOClassTemplate : public Parent {
    public:
        DOClassTemplate(unsigned int uiClassID) : Parent(uiClassID) {}
        virtual ~DOClassTemplate() {}
        virtual void SpecificAddDSToDiscoveryMessage(DuplicatedObject *pDO, Station *pStation, Message *pMsg) {
            static_cast<DOType *>(pDO)->AddDSToDiscoveryMessage(pMsg, pStation);
        }
        virtual void SpecificExtractDSFromDiscoveryMessage(DuplicatedObject *pDO, Message *pMsg) {
            static_cast<DOType *>(pDO)->ExtractDSFromDiscoveryMessage(pMsg);
        }
        virtual bool SpecificExtractADataset(DuplicatedObject *pDO, Message *pMsg, unsigned char ucIndex) {
            return static_cast<DOType *>(pDO)->ExtractADataset(pMsg, ucIndex);
        }
        virtual bool SpecificUpdate(DuplicatedObject *pDO, DataSet *pDataSet, const Time &oTime) {
            return static_cast<DOType *>(pDO)->SpecificUpdate(pDataSet, oTime);
        }
        virtual bool SpecificRefresh(DuplicatedObject *pDO, DataSet *pDataSet, const Time &oTime) {
            return static_cast<DOType *>(pDO)->SpecificRefresh(pDataSet, oTime);
        }
    };

    class _DOC_RootDO : public DOClassTemplate<_DO_RootDO, DOClass> {
    public:
        _DOC_RootDO(unsigned int);
        virtual void DataSetsOperation(unsigned int);
        virtual DuplicatedObject *Create();
        virtual bool IsAKindOf(unsigned int);
        virtual const char *GetClassNameString() const;
        virtual const char *GetDatasetNameString(unsigned char) const;
        virtual bool FormatVariableValue(const DuplicatedObject *, Variable *, Variable *, String *) const;
        virtual bool DispatchAction(DuplicatedObject *, unsigned short, Message *);
        virtual void DispatchRMCCall(const CallMethodOperation &);
        virtual bool DispatchRMCResult(RMCContext *);
        virtual void FillDupSpacesInfo(DupSpace::_Role, unsigned int *, unsigned int *);

        unsigned short m_usMethodIDs[3]; // 0x28
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
        virtual bool FormatVariableValue(const DuplicatedObject *, Variable *, Variable *, String *) const;
        virtual bool DispatchAction(DuplicatedObject *, unsigned short, Message *);
        virtual void DispatchRMCCall(const CallMethodOperation &);
        virtual bool DispatchRMCResult(RMCContext *);
        virtual void FillDupSpacesInfo(DupSpace::_Role, unsigned int *, unsigned int *);

        unsigned short m_usSignalAsFaultyID; // 0x30
    };

    // ---------------------------------------------------------------------
    // _DOC_Station

    DuplicatedObject *_DOC_Station::Create() { return new (__FILE__, 0xB) Station; }

    void _DOC_Station::Delete(DuplicatedObject *pDO) { delete pDO; }

    const char *_DOC_Station::GetClassNameString() const { return "Station"; }

    _DOC_Station::_DOC_Station(unsigned int uiClassID)
        : DOClassTemplate<_DO_Station, _DOC_RootDO>(uiClassID) {
        m_usSignalAsFaultyID = MethodIDGenerator::AssignID(String("SignalAsFaulty"));
    }

    bool _DOC_Station::ApproveFaultRecovery(DuplicatedObject *pDO) {
        return static_cast<Station *>(pDO)->ApproveFaultRecovery();
    }

    bool _DOC_Station::ApproveEmigration(DuplicatedObject *pDO, unsigned int uiStation) {
        return static_cast<Station *>(pDO)->ApproveEmigration(uiStation);
    }

    void _DOC_Station::Trace(DuplicatedObject *pDO, unsigned int uiFlags) {
        static_cast<Station *>(pDO)->Trace(uiFlags);
    }

    bool _DOC_Station::IsAKindOf(unsigned int uiClassID) {
        if (_DO_Station::s_uiClassID == uiClassID)
            return true;
        return _DOC_RootDO::IsAKindOf(uiClassID);
    }

    void _DOC_Station::DataSetsOperation(unsigned int uiOperation) {
        _DOC_RootDO::DataSetsOperation(uiOperation);
        if (uiOperation == 0) {
            CreateUpdatePolicy(1);
            GetUpdatePolicy(1)->RegisterProtocol(new (__FILE__, 0x1A) BasicUpdateProtocol<ConnectionInfo>);
        }
        if (uiOperation == 0) {
            CreateUpdatePolicy(2);
            GetUpdatePolicy(2)->RegisterProtocol(new (__FILE__, 0x1B) BasicUpdateProtocol<StationIdentification>);
        }
        if (uiOperation == 0) {
            GetUpdatePolicy(2)->AddFilter(new (__FILE__, 0x1C) ConstFilter);
        }
        if (uiOperation == 0) {
            CreateUpdatePolicy(3);
            GetUpdatePolicy(3)->RegisterProtocol(new (__FILE__, 0x1D) BasicUpdateProtocol<StationInfo>);
        }
        if (uiOperation == 0) {
            CreateUpdatePolicy(4);
            GetUpdatePolicy(4)->RegisterProtocol(new (__FILE__, 0x1E) BasicUpdateProtocol<StationState>);
        }
    }

    bool _DOC_Station::FormatVariableValue(
        const DuplicatedObject *pDO, Variable *pVariable, Variable *pSubVariable, String *pString
    ) const {
        if (String::IsEqual(pVariable->m_szName, "m_dsConnectionInfo"))
            return static_cast<const _DO_Station *>(pDO)->m_dsConnectionInfo.FormatVariableValue(
                pSubVariable, pString
            );
        if (String::IsEqual(pVariable->m_szName, "m_dsIdentification"))
            return static_cast<const _DO_Station *>(pDO)->m_dsIdentification.FormatVariableValue(
                pSubVariable, pString
            );
        if (String::IsEqual(pVariable->m_szName, "m_dsInfo"))
            return static_cast<const _DO_Station *>(pDO)->m_dsInfo.FormatVariableValue(pSubVariable, pString);
        if (String::IsEqual(pVariable->m_szName, "m_dsStationState"))
            return static_cast<const _DO_Station *>(pDO)->m_dsStationState.FormatVariableValue(
                pSubVariable, pString
            );
        return _DOC_RootDO::FormatVariableValue(pDO, pVariable, pSubVariable, pString);
    }

    bool _DOC_Station::DispatchAction(DuplicatedObject *pDO, unsigned short usAction, Message *pMsg) {
        return _DOC_RootDO::DispatchAction(pDO, usAction, pMsg);
    }

    void _DOC_Station::DispatchRMCCall(const CallMethodOperation &oOperation) {
        if (oOperation.GetMethodID() == m_usSignalAsFaultyID) {
            static_cast<_DO_Station *>(oOperation.GetTargetObject())->DispatchSignalAsFaulty(oOperation);
            oOperation.SetReturnValue(qResult(0x60001));
        } else {
            _DOC_RootDO::DispatchRMCCall(oOperation);
        }
    }

    bool _DOC_Station::DispatchRMCResult(RMCContext *pContext) {
        if (pContext->GetMethodID() == m_usSignalAsFaultyID) {
            pContext->GetResponseMessage();
            return true;
        }
        return _DOC_RootDO::DispatchRMCResult(pContext);
    }

    void _DOC_Station::FillDupSpacesInfo(DupSpace::_Role eRole, unsigned int *puiA, unsigned int *puiB) {
        _DOC_RootDO::FillDupSpacesInfo(eRole, puiA, puiB);
    }

    const char *_DOC_Station::GetDatasetNameString(unsigned char ucIndex) const {
        switch (ucIndex) {
        case 1:
            return "ConnectionInfo";
        case 2:
            return "StationIdentification";
        case 3:
            return "StationInfo";
        case 4:
            return "StationState";
        default:
            return _DOC_RootDO::GetDatasetNameString(ucIndex);
        }
    }

    // ---------------------------------------------------------------------
    // _DO_Station

    unsigned int _DO_Station::s_uiClassID;

    _DO_Station::_DO_Station() {
        SetDOClassID(s_uiClassID);
        TestInvariants();
    }

    void _DO_Station::InitDOClass(unsigned int uiClassID) {
        s_uiClassID = uiClassID;
        DOClass *pDOClass = new (__FILE__, 0x4E) _DOC_Station(uiClassID);
        pDOClass->CompleteInitialisation();
    }

    DuplicatedObject *_DO_Station::Create(unsigned int uiID) {
        return DuplicatedObject::Create(GetStaticClassID(), uiID);
    }

    DOHandle _DO_Station::GetIDGenerator() { return GetDOClass(GetStaticClassID())->GetIDGenerator(); }

    void _DO_Station::AddDSToDiscoveryMessage(Message *pMsg, Station *pStation) {
        GetDOClass()
            ->GetUpdatePolicy(1)
            ->AddToDiscoveryMessage(this, &m_dsConnectionInfo, 1, pStation, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(2)
            ->AddToDiscoveryMessage(this, &m_dsIdentification, 2, pStation, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(3)
            ->AddToDiscoveryMessage(this, &m_dsInfo, 3, pStation, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(4)
            ->AddToDiscoveryMessage(this, &m_dsStationState, 4, pStation, pMsg);
        _DO_RootDO::AddDSToDiscoveryMessage(pMsg, pStation);
    }

    void _DO_Station::ExtractDSFromDiscoveryMessage(Message *pMsg) {
        GetDOClass()
            ->GetUpdatePolicy(1)
            ->ExtractFromDiscoveryMessage(this, &m_dsConnectionInfo, 1, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(2)
            ->ExtractFromDiscoveryMessage(this, &m_dsIdentification, 2, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(3)
            ->ExtractFromDiscoveryMessage(this, &m_dsInfo, 3, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(4)
            ->ExtractFromDiscoveryMessage(this, &m_dsStationState, 4, pMsg);
        _DO_RootDO::ExtractDSFromDiscoveryMessage(pMsg);
    }

    void _DO_Station::CallOperationOnDatasets(DOOperation *pOperation, Operation::_Event eEvent) {
        if (eEvent == Operation::Begin && pOperation->CallsBackOnDataSet(1))
            m_dsConnectionInfo.CallOperationOnVars(eEvent, pOperation);
        else if (eEvent == Operation::End && pOperation->CallsBackOnDataSet(1))
            m_dsConnectionInfo.CallOperationOnVars(eEvent, pOperation);
        if (eEvent == Operation::Begin && pOperation->CallsBackOnDataSet(2))
            m_dsIdentification.CallOperationOnVars(eEvent, pOperation);
        else if (eEvent == Operation::End && pOperation->CallsBackOnDataSet(2))
            m_dsIdentification.CallOperationOnVars(eEvent, pOperation);
        if (eEvent == Operation::Begin && pOperation->CallsBackOnDataSet(3))
            m_dsInfo.CallOperationOnVars(eEvent, pOperation);
        else if (eEvent == Operation::End && pOperation->CallsBackOnDataSet(3))
            m_dsInfo.CallOperationOnVars(eEvent, pOperation);
        if (eEvent == Operation::Begin && pOperation->CallsBackOnDataSet(4)) {
            m_dsStationState.OperationBegin(pOperation);
            m_dsStationState.CallOperationOnVars(eEvent, pOperation);
        } else if (eEvent == Operation::End && pOperation->CallsBackOnDataSet(4)) {
            m_dsStationState.OperationEnd(pOperation);
            m_dsStationState.CallOperationOnVars(eEvent, pOperation);
        }
        _DO_RootDO::CallOperationOnDatasets(pOperation, eEvent);
    }

    bool _DO_Station::SpecificUpdate(DataSet *pDataSet, const Time &oTime) {
        if (pDataSet == NULL || pDataSet == &m_dsConnectionInfo) {
            GetDOClass()
                ->GetUpdatePolicy(1)
                ->Update(this, &m_dsConnectionInfo, 1, oTime);
            if (pDataSet == &m_dsConnectionInfo)
                return true;
        }
        if (pDataSet == NULL || pDataSet == &m_dsIdentification) {
            GetDOClass()
                ->GetUpdatePolicy(2)
                ->Update(this, &m_dsIdentification, 2, oTime);
            if (pDataSet == &m_dsIdentification)
                return true;
        }
        if (pDataSet == NULL || pDataSet == &m_dsInfo) {
            GetDOClass()
                ->GetUpdatePolicy(3)
                ->Update(this, &m_dsInfo, 3, oTime);
            if (pDataSet == &m_dsInfo)
                return true;
        }
        if (pDataSet == NULL || pDataSet == &m_dsStationState) {
            GetDOClass()
                ->GetUpdatePolicy(4)
                ->Update(this, &m_dsStationState, 4, oTime);
            if (pDataSet == &m_dsStationState)
                return true;
        }
        return _DO_RootDO::SpecificUpdate(pDataSet, oTime);
    }

    bool _DO_Station::SpecificRefresh(DataSet *pDataSet, const Time &oTime) {
        if (pDataSet == NULL)
            m_dsConnectionInfo.Refresh(oTime);
        if (pDataSet == &m_dsConnectionInfo)
            return m_dsConnectionInfo.Refresh(oTime);
        if (pDataSet == NULL)
            m_dsIdentification.Refresh(oTime);
        if (pDataSet == &m_dsIdentification)
            return m_dsIdentification.Refresh(oTime);
        if (pDataSet == NULL)
            m_dsInfo.Refresh(oTime);
        if (pDataSet == &m_dsInfo)
            return m_dsInfo.Refresh(oTime);
        if (pDataSet == NULL)
            m_dsStationState.Refresh(oTime);
        if (pDataSet == &m_dsStationState)
            return m_dsStationState.Refresh(oTime);
        return _DO_RootDO::SpecificRefresh(pDataSet, oTime);
    }

    bool _DO_Station::ExtractADataset(Message *pMsg, unsigned char ucIndex) {
        switch (ucIndex) {
        case 1:
            GetDOClass()
                ->GetUpdatePolicy(1)
                ->ExtractFromUpdateMessage(this, &m_dsConnectionInfo, 1, pMsg);
            return true;
        case 2:
            GetDOClass()
                ->GetUpdatePolicy(2)
                ->ExtractFromUpdateMessage(this, &m_dsIdentification, 2, pMsg);
            return true;
        case 3:
            GetDOClass()
                ->GetUpdatePolicy(3)
                ->ExtractFromUpdateMessage(this, &m_dsInfo, 3, pMsg);
            return true;
        case 4:
            GetDOClass()
                ->GetUpdatePolicy(4)
                ->ExtractFromUpdateMessage(this, &m_dsStationState, 4, pMsg);
            return true;
        default:
            return _DO_RootDO::ExtractADataset(pMsg, ucIndex);
        }
    }

    bool _DO_Station::CallSignalAsFaulty(RMCContext *pContext, const unsigned int &uiStationID) {
        if (!pContext->PrepareCallMessage(GetHandle(), MethodIDGenerator::GetID(String("SignalAsFaulty"))))
            return false;
        Message *pCallMsg = pContext->GetCallMessage();
        pCallMsg->Append(&uiStationID, 4, true);
        return pContext->PerformCallAndWait();
    }

    void _DO_Station::DispatchSignalAsFaulty(const CallMethodOperation &oOperation) {
        unsigned int uiStationID;
        const CallMethodOperation *pCallOperation = &oOperation;
        Message *pMsg = oOperation.GetCallMessage();
        pMsg->Extract(&uiStationID, 4, true);
        static_cast<Station *>(this)->SignalAsFaulty(uiStationID);
        pMsg = oOperation.PrepareSuccessMessage();
    }
}
