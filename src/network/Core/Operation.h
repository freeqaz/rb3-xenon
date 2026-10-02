#pragma once
#include "Core/StateMachine.h"
#include "Platform/UserContext.h"
#include "ObjDup/DOHandle.h"

namespace Quazal {

    class Operation : public StateMachine::QEvent {
    public:
        enum _Event {
        };
        Operation(unsigned int);
        virtual ~Operation() {}
        virtual unsigned short GetSignal() const { return GetType(); }
        virtual int GetType() const = 0;
        virtual const char *GetClassNameString() const = 0;
        virtual void ForceImplOperationCommonMethodsMacro() = 0;
        virtual void TraceImpl(_Event, unsigned int) const = 0;

        void Trace(_Event) const;
        void Trace(unsigned int) const;
        DOHandle GetOrigin() const { return m_uiOrigin; }
        void SetUserData(UserContext);
        UserContext GetUserData();

        bool m_bOperationAborted; // 0x8
        UserContext m_uUserData; // 0xc
        unsigned int m_uiOrigin; // 0x10
    };

}