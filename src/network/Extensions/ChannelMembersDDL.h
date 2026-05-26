#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/SessionStateDDL.h" // for Variable, Operation
#include "Platform/RootObject.h"
#include "Platform/qStd.h"
#include "Platform/String.h"
#include "Platform/Time.h"
#include "VoiceChannelMember.h"

namespace Quazal {
    class Message;

    class _DS_ChannelMembers : public DataSet {
    public:
        _DS_ChannelMembers() {}
        ~_DS_ChannelMembers() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);

        qList<VoiceChannelMember> m_dsMemberList; // 0x4
    };
}
