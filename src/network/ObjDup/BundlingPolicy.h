#pragma once
#include "Core/PseudoGlobalVariable.h"
#include "Platform/RootObject.h"

namespace Quazal {
    class Message;
    class SelectionIterator;
    class DuplicatedObject;

    class BundlingPolicy : public RootObject {
    public:
        BundlingPolicy();
        virtual ~BundlingPolicy();
        virtual void SendToSelection(Message *, SelectionIterator *, DuplicatedObject *, unsigned int) = 0;

        static BundlingPolicy *GetInstance() { return s_pInstance.GetValue(); }

        static PseudoGlobalVariable<BundlingPolicy *> s_pInstance;
    };
}
