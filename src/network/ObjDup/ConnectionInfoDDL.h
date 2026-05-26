#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/SessionStateDDL.h" // for Variable, Operation, _Type_uint32
#include "ObjDup/DOCoreTypes.h"     // for _Type_string
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class Message;

    class _DS_ConnectionInfo : public DataSet {
    public:
        _DS_ConnectionInfo() {}
        ~_DS_ConnectionInfo() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);

        bool m_bURLInitialized;          // 0x0
        String m_strStationURL1;         // 0x4
        String m_strStationURL2;         // 0x8
        String m_strStationURL3;         // 0xc
        String m_strStationURL4;         // 0x10
        String m_strStationURL5;         // 0x14
        unsigned int m_uiInputBandwidth;  // 0x18
        unsigned int m_uiInputLatency;    // 0x1c
        unsigned int m_uiOutputBandwidth; // 0x20
        unsigned int m_uiOutputLatency;   // 0x24
    };
}
