#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/SessionStateDDL.h" // for Variable, Operation, _Type_uint32
#include "ObjDup/DOCoreTypes.h"     // for _Type_string
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class Message;

    class _DS_StationIdentification : public DataSet {
    public:
        _DS_StationIdentification() {}
        ~_DS_StationIdentification() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);

        String m_strIdentificationToken; // 0x0
        String m_strProcessName;         // 0x4
        unsigned int m_uiProcessType;    // 0x8
        unsigned int m_uiProductVersion; // 0xc
    };
}
