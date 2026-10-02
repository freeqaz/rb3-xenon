#pragma once
#include "Platform/RootObject.h"
#include "Platform/qStd.h"

namespace Quazal {
    class Operation;
    class DOOperation;

    class OperationManager : public RootObject {
    public:
        DOOperation *GetCurrentOperation() const;
        void InvokeCallbacks(int, int, Operation *);
        void OperationBegins(Operation *pOperation) { m_lstOperations.push_back(pOperation); }
        void PopOperation(Operation *);

        int unk0; // 0x0
        int unk4; // 0x4
        qList<Operation *> m_lstOperations; // 0x8
    };
}
