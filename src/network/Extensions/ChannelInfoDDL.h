#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/SessionStateDDL.h" // for Variable, Operation, _Type_*
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class Message;

    class _DS_ChannelInfo : public DataSet {
    public:
        _DS_ChannelInfo() {}
        ~_DS_ChannelInfo() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);

        unsigned char m_byCodec;                // 0x0
        unsigned char m_byNbStreams;            // 0x1
        unsigned char m_byTransmissionFrequency;// 0x2
        String m_strDescription;                // 0x4
        unsigned short m_uiPacketSize;          // 0x8
    };
}
