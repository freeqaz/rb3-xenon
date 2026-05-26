#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/DOHandle.h"
#include "ObjDup/SessionStateDDL.h" // for Variable, Operation, _Type_uint32, _Type_dohandle
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class Message;

    class _DS_SessionInfo : public DataSet {
    public:
        _DS_SessionInfo() {}
        ~_DS_SessionInfo() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);

        DOHandle m_dohRootMulticastGroup; // 0x0
        char m_szSessionName[0x80];       // 0x4
        unsigned int m_uiSessionID;       // 0x84
    };
}
