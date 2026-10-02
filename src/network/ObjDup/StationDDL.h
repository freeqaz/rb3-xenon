#pragma once
#include "RootDO.h"
#include "ObjDup/ConnectionInfoDDL.h"
#include "ObjDup/StationIdentificationDDL.h"
#include "ObjDup/StationInfoDDL.h"
#include "ObjDup/StationStateDDL.h"

namespace Quazal {
    class StationURL;

    // The user dataset classes over the DDL-generated ones. Retail calls
    // ConnectionInfo's, StationInfo's and StationState's destructors out of
    // line from the _DO_Station destructor (0x82A7BBF0), and expands
    // StationIdentification's.
    class ConnectionInfo : public _DS_ConnectionInfo {
    public:
        ~ConnectionInfo();
        const char *GetURL(int) const;
    };

    class StationIdentification : public _DS_StationIdentification {
    public:
    };

    class StationInfo : public _DS_StationInfo {
    public:
        ~StationInfo();
        unsigned int GetMachineUniqueID() const;
        void InitMachineUniqueID();
        void SetObserver(unsigned int ui) { m_hObserver.mValue = ui; }
    };

    class StationState : public _DS_StationState {
    public:
        ~StationState();
        void Set(unsigned short);
    };

    class _DO_Station : public RootDO {
    public:
        _DO_Station();
        virtual ~_DO_Station() {}
        virtual void CallOperationOnDatasets(DOOperation *, Operation::_Event);
        virtual bool IsACoreDO() const;
        virtual bool IsABootstrapDO() const;

        static unsigned int GetStaticClassID() { return s_uiClassID; }
        static unsigned int s_uiClassID;

        ConnectionInfo m_oConnectionInfo; // 0x70
        StationIdentification m_oIdentification; // 0x98
        StationInfo m_oStationInfo; // 0xa8
        StationState m_oState; // 0xb0
    };
}
