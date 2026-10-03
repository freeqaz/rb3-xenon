// Quazal NetZ - .\GlobalDiscovery\GlobalDiscoveryExtension.cpp
//
// The retail TU is 0x82B2BE80..0x82B2C0E0: the GlobalDiscoveryExtension
// constructor, the GetType/IsAKindOf/deleting-destructor virtuals its vtable
// needs, then the destructor, BeginInitialization and Register in source order.
// The code before it (from 0x82B2B940) is the PseudoGlobalVariable instantiation
// of DuplicationSpace's static at 0x82E11CE0; the code after it (0x82B2C0E0) is
// the "Quazal Net-Z" product descriptor (vtable 0x8218B608, static instance
// 0x82E11780), which belongs to another TU.
//
// Built /Od /Oi- /EHs-c- /Ob1 /GR-: no EH prefixes or funclets, and the vtable
// at 0x8218B558 carries no locator slot. The classes it uses are declared here
// only as far as this TU uses them; their members are defined in other TUs.

#include "Platform/RootObject.h"

#define GLOBALDISCOVERYEXTENSION_FILE ".\\GlobalDiscovery\\GlobalDiscoveryExtension.cpp"

namespace Quazal {

    class String : public RootObject {
    public:
        String(const char *);
        ~String();

        char *m_szContent; // 0x0
    };

    class SystemComponent : public RootObject {
    public:
        enum _State {
        };

        static const char *type() { return "SystemComponent"; }

        SystemComponent(const String &);
        virtual ~SystemComponent();
        virtual void *AcquireRef();
        virtual void ReleaseRef();
        virtual const char *GetType() const { return type(); }
        virtual bool IsAKindOf(const char *str) const { return type() == str; }
        virtual void EnforceDeclareSysComponentMacro() = 0;
        virtual void TraceImpl(unsigned int) const;
        virtual _State StateTransition(_State);
        virtual void OnInitialize();
        virtual void OnTerminate();
        virtual bool BeginInitialization();
        virtual bool EndInitialization();
        virtual bool BeginTermination();
        virtual bool EndTermination();
        virtual bool ValidTransition(_State);
        virtual bool UseIsAllowed();
        virtual _State TestState();
        virtual void DoWork();

        unsigned short m_ui16RefCount; // 0x4
        String mName; // 0x8
        _State mState; // 0xc
        unsigned int mRefs; // 0x10
        SystemComponent *mParent; // 0x14
    };

    class SystemComponentGroup : public SystemComponent {
    public:
        bool RegisterComponent(SystemComponent *);
    };

    class SystemComponents : public SystemComponentGroup {
    public:
        SystemComponentGroup *GetExtensions() { return m_pExtensions; }

        char m_pad18[0x10];
        SystemComponentGroup *m_pExtensions; // 0x28
    };

    class InstanceTable : public RootObject {
    public:
        unsigned int LookupInstance(unsigned int, unsigned int);
    };

    class InstanceControl : public RootObject {
    public:
        virtual ~InstanceControl();

        static InstanceTable s_oInstanceTable;

        unsigned int m_icInstanceContext; // 0x4
        unsigned int m_icInstanceType; // 0x8
        void *m_pDelegatorInstance; // 0xc
    };

    class PseudoSingleton : public RootObject {
    public:
        static unsigned int GetCurrentContext();
    };

    // Retail calls GetInstance out of line (0x823EA910, an /O1 copy): /Ob1
    // declines it, and each call site keeps its three locals reserved in the
    // caller's frame.
    class Core : public RootObject {
    public:
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.LookupInstance(3, uiContext);
            Core *pCore = inst ? (Core *)inst->m_pDelegatorInstance : 0;
            return pCore;
        }
        SystemComponents *GetSystemComponents() { return m_pSystemComponents; }

        char m_pad0[0x10];
        SystemComponents *m_pSystemComponents; // 0x10
    };

    inline SystemComponents *GetSystemComponents() {
        if (Core::GetInstance() == 0)
            return 0;
        return Core::GetInstance()->GetSystemComponents();
    }

    class Operation;

    class OperationCallback : public RootObject {
    public:
        virtual ~OperationCallback() {}
        virtual void CallMethod(Operation *) = 0;

        unsigned int m_uiPriority; // 0x4
    };

    class OperationManager : public RootObject {
    public:
        void RegisterCallback(OperationCallback *);
    };

    class DuplicatedObject : public RootObject {
    public:
        static OperationManager *GetOperationManager();
    };

    class GlobalDiscovery : public OperationCallback {
    public:
        GlobalDiscovery();
        virtual void CallMethod(Operation *);
    };

    class GlobalDiscoveryExtension : public SystemComponent {
    public:
        static const char *type() { return "GlobalDiscoveryExtension"; }

        GlobalDiscoveryExtension();
        virtual ~GlobalDiscoveryExtension();
        virtual const char *GetType() const { return type(); }
        virtual bool IsAKindOf(const char *str) const {
            return type() == str || SystemComponent::IsAKindOf(str);
        }
        virtual void EnforceDeclareSysComponentMacro();
        virtual bool BeginInitialization();

        static bool Register();

        GlobalDiscovery m_oGlobalDiscovery; // 0x18
    };

    GlobalDiscoveryExtension::GlobalDiscoveryExtension()
        : SystemComponent("GlobalDiscovery extension") {}

    GlobalDiscoveryExtension::~GlobalDiscoveryExtension() {}

    bool GlobalDiscoveryExtension::BeginInitialization() {
        DuplicatedObject::GetOperationManager()->RegisterCallback(&m_oGlobalDiscovery);
        return true;
    }

    bool GlobalDiscoveryExtension::Register() {
        GlobalDiscoveryExtension *pExtension =
            new (GLOBALDISCOVERYEXTENSION_FILE, 0x21) GlobalDiscoveryExtension();
        GetSystemComponents()->GetExtensions()->RegisterComponent(pExtension);
        return true;
    }

}
