#pragma once
#include "Core/PseudoGlobalVariable.h"
#include "Platform/RootObject.h"
#include "ObjDup/DOHandle.h"

namespace Quazal {
    class Message;
    class SelectionIterator;
    class DuplicatedObject;

    class BundlingPolicy : public RootObject {
    public:
        BundlingPolicy();
        virtual ~BundlingPolicy();
        virtual void SendToSelection(Message *, SelectionIterator *, DuplicatedObject *, unsigned int) = 0;
        // Slots 2 and 3, as Session calls them (0x82A77DA8, 0x82A78718).
        virtual void Flush() = 0;
        virtual void AddStation(DOHandle) = 0;

        static BundlingPolicy *GetInstance() { return s_pInstance.GetValue(); }

        static PseudoGlobalVariable<BundlingPolicy *> s_pInstance;
    };
}
