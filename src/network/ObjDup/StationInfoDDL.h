#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/SessionStateDDL.h" // for Variable, Operation, _Type_uint32, _Type_dohandle
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class Message;

    class _DS_StationInfo : public DataSet {
    public:
        _DS_StationInfo() {}
        ~_DS_StationInfo() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);

        DOHandle m_hObserver;        // 0x0
        unsigned int m_uiMachineUID; // 0x4
    };
}
