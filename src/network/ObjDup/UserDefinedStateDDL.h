#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/SessionStateDDL.h" // for Variable, Operation::_Event decls
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class Message;

    class _DS_UserDefinedState : public DataSet {
    public:
        _DS_UserDefinedState() {}
        ~_DS_UserDefinedState() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);

        unsigned int m_uiUserDefinedState; // 0x0
    };
}
