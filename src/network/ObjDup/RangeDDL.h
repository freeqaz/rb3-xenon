#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/SessionStateDDL.h" // for Variable, Operation, _Type_uint32
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class Message;

    class _DS_Range : public DataSet {
    public:
        _DS_Range() {}
        ~_DS_Range() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);

        unsigned int m_uiFirst; // 0x0
        unsigned int m_uiLast;  // 0x4
    };
}
