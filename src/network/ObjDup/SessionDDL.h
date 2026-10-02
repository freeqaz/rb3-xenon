#pragma once
#include "RootDO.h"
#include "ObjDup/DataSet.h"
#include "ObjDup/SessionInfo.h"

namespace Quazal {
    // The session's datasets. Retail's _DO_Session dtor (0x82A76CC8) destroys
    // +0x5FC, +0x5F8, +0x570 and +0x70 out of line before ~RootDO, so each
    // dataset class has an out-of-line dtor.
    class SharedSessionDescription : public DataSet {
    public:
        ~SharedSessionDescription();
        void Refresh();
        void Clear();

        char m_szName[0x400]; // 0x0
        char m_szSomething1[0x80]; // 0x400
        char m_szSomething2[0x80]; // 0x480
    };

    class SessionState : public DataSet {
    public:
        ~SessionState();
        void SetState(unsigned char);
        unsigned char GetState();

        unsigned char m_bySessionState; // 0x0
        unsigned char pad1[3];
    };

    class SessionFlags : public DataSet {
    public:
        ~SessionFlags();

        unsigned char m_byFlags; // 0x0
        unsigned char pad1[3];
    };

    class _DO_Session : public RootDO {
    public:
        _DO_Session();
        virtual ~_DO_Session() {}
        virtual bool HasGlobalDOProperty() const;
        virtual void CallOperationOnDatasets(DOOperation *, Operation::_Event);
        virtual bool IsACoreDO() const;
        virtual bool IsABootstrapDO() const;

        static unsigned int GetStaticClassID() { return s_uiClassID; }
        static unsigned int s_uiClassID;

        SharedSessionDescription m_dsSharedSessionDescription; // 0x70
        SessionInfo m_dsSessionInfo; // 0x570
        SessionState m_dsSessionState; // 0x5F8
        SessionFlags m_dsSessionFlags; // 0x5FC
    };
}
