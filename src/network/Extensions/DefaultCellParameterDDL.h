#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/SessionStateDDL.h" // for Variable, Operation, _Type_uint32
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class Message;

    class _DS_DefaultCellParameter : public DataSet {
    public:
        _DS_DefaultCellParameter() {}
        ~_DS_DefaultCellParameter() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);

        unsigned int m_idDupSpace; // 0x0
    };
}
