#pragma once
#include "Platform/RootObject.h"

namespace Quazal {
    class DDLDeclarations : public RootObject {
    public:
        DDLDeclarations(bool);
        virtual ~DDLDeclarations();
        virtual void Load() = 0;

        void RegisterIfRequired();
        static void LoadAll();
        static void UnloadAll();
        static void ResetDOClassIDs();

        static DDLDeclarations *s_pFirstDDLDecl;
        static unsigned int s_uiBaseClassID;
        static unsigned int s_uiFirstUserClassID;

        int m_refCount;             // 0x4
        bool m_bRegistered;         // 0x8
        DDLDeclarations *m_pNext;   // 0xc
        bool m_bIsUserClass;        // 0x10
    };
}
