#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/SessionStateDDL.h" // for Variable, Operation, _Type_uint16
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class Message;

    class _DS_StationState : public DataSet {
    public:
        _DS_StationState() {}
        ~_DS_StationState() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);
        // Defined inline in StationDDL.cpp, where the update protocol uses it.
        void ExtractFrom(Message *);

        unsigned short m_ui16State; // 0x0
    };
}
