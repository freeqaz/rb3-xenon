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
// index (ObjDup/UpdatePolicy.h).

#include "ObjDup/Station.h"
#include "ObjDup/DOClass.h"
#include "ObjDup/DOOperation.h"
#include "ObjDup/MethodIDGenerator.h"
#include "ObjDup/RMCContext.h"
#include "ObjDup/UpdatePolicy.h"
#include "Platform/Result.h"
#include "Platform/String.h"
#include "Plugins/Message.h"

namespace Quazal {

    // ---------------------------------------------------------------------
    // Dataset extraction, used by BasicUpdateProtocol<>::ExtractFromMessage.
    // At /Ob1 the two small ones are expanded there and the two large ones
    // are called out of line.

    inline void _DS_ConnectionInfo::ExtractFrom(Message *pMsg) {
        *pMsg >> m_bURLInitialized;
        _Type_string::Extract(pMsg, &m_strStationURL1);
        _Type_string::Extract(pMsg, &m_strStationURL2);
        _Type_string::Extract(pMsg, &m_strStationURL3);
        _Type_string::Extract(pMsg, &m_strStationURL4);
        _Type_string::Extract(pMsg, &m_strStationURL5);
        pMsg->Extract((unsigned char *)&m_uiInputBandwidth, 4, true);
        pMsg->Extract((unsigned char *)&m_uiInputLatency, 4, true);
        pMsg->Extract((unsigned char *)&m_uiOutputBandwidth, 4, true);
        pMsg->Extract((unsigned char *)&m_uiOutputLatency, 4, true);
    }

    inline void _DS_StationIdentification::ExtractFrom(Message *pMsg) {
        _Type_string::Extract(pMsg, &m_strIdentificationToken);
        _Type_string::Extract(pMsg, &m_strProcessName);
        pMsg->Extract((unsigned char *)&m_uiProcessType, 4, true);
        pMsg->Extract((unsigned char *)&m_uiProductVersion, 4, true);
    }

    inline void _DS_StationInfo::ExtractFrom(Message *pMsg) {
        *pMsg >> m_hObserver;
        pMsg->Extract((unsigned char *)&m_uiMachineUID, 4, true);
    }

    inline void _DS_StationState::ExtractFrom(Message *pMsg) {
        pMsg->Extract((unsigned char *)&m_ui16State, 2, true);
    }

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
            return static_cast<const _DO_Station *>(pDO)->m_oConnectionInfo.FormatVariableValue(
                pSubVariable, pString
            );
        if (String::IsEqual(pVariable->m_szName, "m_dsIdentification"))
            return static_cast<const _DO_Station *>(pDO)->m_oIdentification.FormatVariableValue(
                pSubVariable, pString
            );
        if (String::IsEqual(pVariable->m_szName, "m_dsInfo"))
            return static_cast<const _DO_Station *>(pDO)->m_oStationInfo.FormatVariableValue(pSubVariable, pString);
        if (String::IsEqual(pVariable->m_szName, "m_dsStationState"))
            return static_cast<const _DO_Station *>(pDO)->m_oState.FormatVariableValue(
                pSubVariable, pString
            );
        return _DOC_RootDO::FormatVariableValue(pDO, pVariable, pSubVariable, pString);
    }

    bool _DOC_Station::DispatchAction(DuplicatedObject *pDO, unsigned short usAction, Message *pMsg) {
        return _DOC_RootDO::DispatchAction(pDO, usAction, pMsg);
    }

    void _DOC_Station::DispatchRMCCall(const CallMethodOperation &oOperation) {
        if (oOperation.GetMethodID() == m_usSignalAsFaultyID) {
            static_cast<_DO_Station *>(oOperation.m_refTargetObject.GetDOPtr())->DispatchSignalAsFaulty(oOperation);
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

    // The user class over the DDL dataset: it zeroes the two integers and is
    // called (not expanded) from _DO_Station's constructor.
    StationIdentification::StationIdentification() {
        m_uiProcessType = 0;
        m_uiProductVersion = 0;
    }

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
            ->AddToDiscoveryMessage(this, &m_oConnectionInfo, 1, pStation, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(2)
            ->AddToDiscoveryMessage(this, &m_oIdentification, 2, pStation, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(3)
            ->AddToDiscoveryMessage(this, &m_oStationInfo, 3, pStation, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(4)
            ->AddToDiscoveryMessage(this, &m_oState, 4, pStation, pMsg);
        _DO_RootDO::AddDSToDiscoveryMessage(pMsg, pStation);
    }

    bool _DO_Station::ExtractDSFromDiscoveryMessage(Message *pMsg) {
        GetDOClass()
            ->GetUpdatePolicy(1)
            ->ExtractFromDiscoveryMessage(this, &m_oConnectionInfo, 1, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(2)
            ->ExtractFromDiscoveryMessage(this, &m_oIdentification, 2, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(3)
            ->ExtractFromDiscoveryMessage(this, &m_oStationInfo, 3, pMsg);
        GetDOClass()
            ->GetUpdatePolicy(4)
            ->ExtractFromDiscoveryMessage(this, &m_oState, 4, pMsg);
        return _DO_RootDO::ExtractDSFromDiscoveryMessage(pMsg);
    }

    // Events 0 and 1 are the operation's begin and end.
    void _DO_Station::CallOperationOnDatasets(DOOperation *pOperation, Operation::_Event eEvent) {
        if (eEvent == 0 && pOperation->CallsBackOnDataSet(1))
            m_oConnectionInfo.CallOperationOnVars(eEvent, pOperation);
        else if (eEvent == 1 && pOperation->CallsBackOnDataSet(1))
            m_oConnectionInfo.CallOperationOnVars(eEvent, pOperation);
        if (eEvent == 0 && pOperation->CallsBackOnDataSet(2))
            m_oIdentification.CallOperationOnVars(eEvent, pOperation);
        else if (eEvent == 1 && pOperation->CallsBackOnDataSet(2))
            m_oIdentification.CallOperationOnVars(eEvent, pOperation);
        if (eEvent == 0 && pOperation->CallsBackOnDataSet(3))
            m_oStationInfo.CallOperationOnVars(eEvent, pOperation);
        else if (eEvent == 1 && pOperation->CallsBackOnDataSet(3))
            m_oStationInfo.CallOperationOnVars(eEvent, pOperation);
        if (eEvent == 0 && pOperation->CallsBackOnDataSet(4)) {
            m_oState.OperationBegin(pOperation);
            m_oState.CallOperationOnVars(eEvent, pOperation);
        } else if (eEvent == 1 && pOperation->CallsBackOnDataSet(4)) {
            m_oState.OperationEnd(pOperation);
            m_oState.CallOperationOnVars(eEvent, pOperation);
        }
        _DO_RootDO::CallOperationOnDatasets(pOperation, eEvent);
    }

    bool _DO_Station::SpecificUpdate(DataSet *pDataSet, const Time &oTime) {
        if (pDataSet == NULL || pDataSet == &m_oConnectionInfo) {
            GetDOClass()
                ->GetUpdatePolicy(1)
                ->Update(this, &m_oConnectionInfo, 1, oTime);
            if (pDataSet == &m_oConnectionInfo)
                return true;
        }
        if (pDataSet == NULL || pDataSet == &m_oIdentification) {
            GetDOClass()
                ->GetUpdatePolicy(2)
                ->Update(this, &m_oIdentification, 2, oTime);
            if (pDataSet == &m_oIdentification)
                return true;
        }
        if (pDataSet == NULL || pDataSet == &m_oStationInfo) {
            GetDOClass()
                ->GetUpdatePolicy(3)
                ->Update(this, &m_oStationInfo, 3, oTime);
            if (pDataSet == &m_oStationInfo)
                return true;
        }
        if (pDataSet == NULL || pDataSet == &m_oState) {
            GetDOClass()
                ->GetUpdatePolicy(4)
                ->Update(this, &m_oState, 4, oTime);
            if (pDataSet == &m_oState)
                return true;
        }
        return _DO_RootDO::SpecificUpdate(pDataSet, oTime);
    }

    bool _DO_Station::SpecificRefresh(DataSet *pDataSet, const Time &oTime) {
        if (pDataSet == NULL)
            m_oConnectionInfo.Refresh(oTime);
        if (pDataSet == &m_oConnectionInfo)
            return m_oConnectionInfo.Refresh(oTime);
        if (pDataSet == NULL)
            m_oIdentification.Refresh(oTime);
        if (pDataSet == &m_oIdentification)
            return m_oIdentification.Refresh(oTime);
        if (pDataSet == NULL)
            m_oStationInfo.Refresh(oTime);
        if (pDataSet == &m_oStationInfo)
            return m_oStationInfo.Refresh(oTime);
        if (pDataSet == NULL)
            m_oState.Refresh(oTime);
        if (pDataSet == &m_oState)
            return m_oState.Refresh(oTime);
        return _DO_RootDO::SpecificRefresh(pDataSet, oTime);
    }

    bool _DO_Station::SpecificExtractADataset(Message *pMsg, unsigned char ucIndex) {
        switch (ucIndex) {
        case 1:
            GetDOClass()
                ->GetUpdatePolicy(1)
                ->ExtractFromUpdateMessage(this, &m_oConnectionInfo, 1, pMsg);
            return true;
        case 2:
            GetDOClass()
                ->GetUpdatePolicy(2)
                ->ExtractFromUpdateMessage(this, &m_oIdentification, 2, pMsg);
            return true;
        case 3:
            GetDOClass()
                ->GetUpdatePolicy(3)
                ->ExtractFromUpdateMessage(this, &m_oStationInfo, 3, pMsg);
            return true;
        case 4:
            GetDOClass()
                ->GetUpdatePolicy(4)
                ->ExtractFromUpdateMessage(this, &m_oState, 4, pMsg);
            return true;
        default:
            return _DO_RootDO::SpecificExtractADataset(pMsg, ucIndex);
        }
    }

    bool _DO_Station::CallSignalAsFaulty(RMCContext *pContext, const unsigned int &uiStationID) {
        if (!pContext->PrepareCallMessage(GetHandle(), MethodIDGenerator::GetID(String("SignalAsFaulty"))))
            return false;
        Message *pCallMsg = pContext->GetCallMessage();
        pCallMsg->Append((const unsigned char *)&uiStationID, 4, true);
        return pContext->PerformCallAndWait();
    }

    // The SignalAsFaulty RMC stub. Retail stores the operation's address to a
    // local it never reads again.
    void _DO_Station::DispatchSignalAsFaulty(const CallMethodOperation &oOperation) {
        unsigned int uiStationID;
        const CallMethodOperation *pCallOperation = &oOperation;
        Message *pMsg = oOperation.GetCallMessage();
        pMsg->Extract((unsigned char *)&uiStationID, 4, true);
        static_cast<Station *>(this)->SignalAsFaulty(uiStationID);
        pMsg = oOperation.PrepareSuccessMessage();
    }
}
