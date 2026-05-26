#pragma once
#include "ObjDup/DataSet.h"
#include "Platform/RootObject.h"
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class Message;
    class Variable;
    namespace Operation {
        enum _Event {};
    }

    class _DS_SharedSessionDescription : public DataSet {
    public:
        _DS_SharedSessionDescription() {}
        ~_DS_SharedSessionDescription() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);

        char m_szName[0x400];          // 0x0
        char m_szSomething1[0x80];     // 0x400
        char m_szSomething2[0x80];     // 0x480
    };
}
