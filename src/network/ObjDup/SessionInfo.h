#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/DOHandle.h"

namespace Quazal {
    class SessionInfo : public DataSet {
    public:
        SessionInfo();
        ~SessionInfo();

        void SetSessionName(const char *);
        char *GetSessionName();
        void GenerateSessionID();
        void SetSessionID(unsigned int);
        unsigned int GetSessionID();

        DOHandle m_dohRootMulticastGroup; // 0x0
        char m_szSessionName[0x80];       // 0x4
        unsigned int m_uiSessionID;       // 0x84
    };
}
