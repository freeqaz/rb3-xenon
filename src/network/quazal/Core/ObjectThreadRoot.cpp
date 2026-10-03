// Quazal NetZ - .\Core\ObjectThreadRoot.cpp
//
// Retail TU: .text 0x82AABE98..0x82AACDE0, built /Od /Oi- /Ob1 /GR- /EHs-c-
// (see objects.json). Its .rdata is the __FILE__ string followed by the
// ObjectThreadRoot vtable (scalar deleting dtor, pure CallObjectMethod) and the
// ThreadVariable<ObjectThreadRoot *> vtable, with no complete-object locators
// and no EH tables (no .pdata record of the TU has the EH bit).
//
// The X360 thread handle is a single HANDLE, INVALID_HANDLE_VALUE while no
// thread runs. A running-thread counter guarded by a critical section and a
// per-thread "current thread" variable are the TU's statics.
//
// The classes are declared here with the layouts retail uses.

#include "Platform/CriticalSection.h"
#include "Platform/MemoryManager.h"
#include "Platform/Platform.h"
#include "Platform/RootObject.h"
#include "Platform/ScopedCS.h"
#include "Platform/String.h"
#include "Platform/SystemError.h"
#include "Platform/qStd.h"

#define OTR_FILE ".\\Core\\ObjectThreadRoot.cpp"

extern "C" {
typedef void *HANDLE;
typedef unsigned long DWORD;
typedef DWORD (*LPTHREAD_START_ROUTINE)(void *);
HANDLE CreateThread(void *, DWORD, LPTHREAD_START_ROUTINE, void *, DWORD, DWORD *);
DWORD WaitForSingleObject(HANDLE, DWORD);
int CloseHandle(HANDLE);
int SetThreadPriority(HANDLE, int);
DWORD GetLastError();
DWORD GetCurrentThreadId();
DWORD XSetThreadProcessor(HANDLE, DWORD);
DWORD GetCurrentProcessorNumber();
}

#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define INFINITE 0xFFFFFFFF

namespace Quazal {

    class ThreadScrambler {
    public:
        static void ThreadStart();
    };

    class ThreadVariableList : public RootObject {
    public:
        static ThreadVariableList &GetInstanceRef();
        void ClearCurrentThreadValues();
    };

    class ThreadVariableRoot : public RootObject {
    public:
        ThreadVariableRoot();
        virtual ~ThreadVariableRoot();
        virtual void ResetValues() = 0;
        virtual void ClearValue() = 0;

        static unsigned int GetCurrentThreadKey();

        int m_iUnk4; // 0x4
        int m_iUnk8; // 0x8
    };

    template <class T>
    class ThreadVariable : public ThreadVariableRoot {
        // A plain map, not a qMap: retail's constructor expands the map
        // constructor in place, which a qMap member (one more level of
        // inlining) does not get under /Ob1.
        typedef std::map<
            unsigned int, T, std::less<unsigned int>,
            MemAllocator<std::pair<const unsigned int, T> > >
            ValueMap;
    public:
        ThreadVariable(const T &tDefault);
        virtual ~ThreadVariable();
        virtual void ResetValues();
        virtual void ClearValue();

        T &GetValueRef();
        void SetValue(const T &tValue);

        T m_tDefaultValue; // 0xc
        CriticalSection m_csValues; // 0x10
        ValueMap m_mapValues; // 0x24
    };

    template <class T>
    inline T &ThreadVariable<T>::GetValueRef() {
        ScopedCS oCS(m_csValues);
        ValueMap::iterator it = m_mapValues.find(GetCurrentThreadKey());
        if (it == m_mapValues.end()) {
            m_mapValues[GetCurrentThreadKey()] = m_tDefaultValue;
            return m_mapValues[GetCurrentThreadKey()];
        } else {
            return it->second;
        }
    }

    template <class T>
    inline ThreadVariable<T>::ThreadVariable(const T &tDefault)
        : m_tDefaultValue(tDefault), m_csValues(0) {
        m_mapValues[GetCurrentThreadKey()] = tDefault;
    }

    template <class T>
    void ThreadVariable<T>::ResetValues() {
        ScopedCS oCS(m_csValues);
        ValueMap::iterator it;
        while (!m_mapValues.empty()) {
            it = m_mapValues.begin();
            m_mapValues.erase(it);
        }
    }

    template <class T>
    void ThreadVariable<T>::ClearValue() {
        ScopedCS oCS(m_csValues);
        ValueMap::iterator it;
        it = m_mapValues.find(GetCurrentThreadKey());
        if (it != m_mapValues.end()) {
            m_mapValues.erase(it);
        }
    }

    template <class T>
    inline ThreadVariable<T>::~ThreadVariable() {
        ResetValues();
    }

    template <class T>
    inline void ThreadVariable<T>::SetValue(const T &tValue) {
        ScopedCS oCS(m_csValues);
        m_mapValues[GetCurrentThreadKey()] = tValue;
    }

    class ObjectThreadRoot : public RootObject {
    public:
        class Handle : public RootObject {
        public:
            Handle() : m_hThread(INVALID_HANDLE_VALUE) {}
            ~Handle() {}

            HANDLE m_hThread; // 0x0
        };

        ObjectThreadRoot(const String &);
        virtual ~ObjectThreadRoot();
        virtual void CallObjectMethod() = 0;

        static ObjectThreadRoot *GetCurrentThread();
        static unsigned int GetDefaultPriority();
        static void SetDefaultPriority(unsigned int);
        static unsigned int GetCurrentThreadID();
        static DWORD ThreadProc(void *);
        void ThreadStarted(unsigned int);
        void ThreadEnded();
        bool Launch();
        bool CreateThreadImpl();
        void Run();
        void Execute();
        bool Wait(unsigned int);
        void SetPriority(unsigned int);
        static void ApplyProcessor(unsigned int);
        static void SetProcessor(unsigned int);
        void ReadyToRun();
        void SetName(const String &);
        static const char *GetCurrentThreadName();
        static unsigned int GetDefaultStackSize();
        static void SetDefaultStackSize(unsigned int);

        String mName; // 0x4
        Handle *mHandle; // 0x8
        DWORD mThreadID; // 0xc
        unsigned int mThreadPrio; // 0x10
        bool mLaunched; // 0x14
        bool mRunning; // 0x15
        bool mFinished; // 0x16

        static unsigned int s_uiDefaultPrio;
        static unsigned int s_uiDefaultStackSize;
    };

    // The number of running threads, with the critical section that guards it.
    struct ThreadCount {
        ThreadCount() : m_uiCount(0), m_csCount(0x40000000) {}

        unsigned int m_uiCount; // 0x0
        CriticalSection m_csCount; // 0x4
    };

    unsigned int ObjectThreadRoot::s_uiDefaultPrio = 1;
    unsigned int ObjectThreadRoot::s_uiDefaultStackSize = 0x10000;

    static unsigned int s_uiProcessor;
    static ThreadCount s_oThreadCount;
    static ThreadVariable<ObjectThreadRoot *> s_tvCurrentThread(0);

    ObjectThreadRoot::ObjectThreadRoot(const String &strName)
#line 172
        : mName(strName), mHandle(new (OTR_FILE, __LINE__) Handle()), mThreadID(0),
          mLaunched(false), mFinished(false) {
        mThreadPrio = GetDefaultPriority();
    }

    ObjectThreadRoot::~ObjectThreadRoot() {
        // Retail evaluates mHandle once before the delete with no code for
        // the test (its registers are allocated, nothing is emitted).
        if (mHandle) {}
        delete mHandle;
        mHandle = NULL;
    }

    ObjectThreadRoot *ObjectThreadRoot::GetCurrentThread() {
        return s_tvCurrentThread.GetValueRef();
    }

    unsigned int ObjectThreadRoot::GetDefaultPriority() { return s_uiDefaultPrio; }

    void ObjectThreadRoot::SetDefaultPriority(unsigned int uiPrio) { s_uiDefaultPrio = uiPrio; }

    unsigned int ObjectThreadRoot::GetCurrentThreadID() { return GetCurrentThreadId(); }

    DWORD ObjectThreadRoot::ThreadProc(void *pParam) {
        ObjectThreadRoot *pThread = (ObjectThreadRoot *)pParam;
        s_tvCurrentThread.SetValue(pThread);
        pThread->Run();
        return 0;
    }

    void ObjectThreadRoot::ThreadStarted(unsigned int uiThreadID) {
        mThreadID = uiThreadID;
        s_oThreadCount.m_csCount.Enter();
        // Retail keeps the counter's previous value in a stack slot.
        unsigned int uiCount = s_oThreadCount.m_uiCount++;
        s_oThreadCount.m_csCount.Leave();
    }

    void ObjectThreadRoot::ThreadEnded() {
        s_oThreadCount.m_csCount.Enter();
        unsigned int uiCount = s_oThreadCount.m_uiCount--;
        s_oThreadCount.m_csCount.Leave();
        ThreadVariableList::GetInstanceRef().ClearCurrentThreadValues();
    }

    bool ObjectThreadRoot::Launch() {
        if (mHandle->m_hThread != INVALID_HANDLE_VALUE) {
            SystemError::SignalError(NULL, 0, 0xE000000E, 0);
            return false;
        }
        mRunning = false;
        mLaunched = true;
        CreateThreadImpl();
        while (!mRunning) {
            Platform::Sleep(10);
        }
        return true;
    }

    bool ObjectThreadRoot::CreateThreadImpl() {
        mHandle->m_hThread =
            CreateThread(NULL, s_uiDefaultStackSize, ThreadProc, this, 0, &mThreadID);
        if (mHandle->m_hThread == NULL) {
            SystemError::SignalError(NULL, 0, 0xE0000002, GetLastError());
            return false;
        }
        return true;
    }

    void ObjectThreadRoot::Run() {
        ThreadScrambler::ThreadStart();
        if (s_uiProcessor != 0) {
            ApplyProcessor(s_uiProcessor);
        }
        SetPriority(mThreadPrio);
        unsigned int uiThreadID = GetCurrentThreadID();
        mFinished = false;
        ThreadStarted(uiThreadID);
        Execute();
        mFinished = true;
        ThreadEnded();
    }

    void ObjectThreadRoot::Execute() { CallObjectMethod(); }

    bool ObjectThreadRoot::Wait(unsigned int uiTimeout) {
        if (mHandle->m_hThread == INVALID_HANDLE_VALUE) {
            return true;
        }
        uiTimeout = uiTimeout == INFINITE ? INFINITE : uiTimeout;
        if (WaitForSingleObject(mHandle->m_hThread, uiTimeout) == 0) {
            CloseHandle(mHandle->m_hThread);
            mHandle->m_hThread = INVALID_HANDLE_VALUE;
        } else {
            SystemError::SignalError(NULL, 0, 0xE000000C, 0);
            return false;
        }
        return true;
    }

    void ObjectThreadRoot::SetPriority(unsigned int uiPrio) {
        mThreadPrio = uiPrio;
        int iPriority = 0;
        switch (uiPrio) {
        case 0:
            iPriority = -1;
            break;
        case 1:
            iPriority = 0;
            break;
        case 2:
            iPriority = 1;
            break;
        }
        SetThreadPriority(mHandle->m_hThread, uiPrio);
    }

    void ObjectThreadRoot::ApplyProcessor(unsigned int uiProcessor) {
        HANDLE hThread = (HANDLE)-2;
        DWORD dwPrevious = XSetThreadProcessor(hThread, uiProcessor);
        if (dwPrevious == (DWORD)-1) {
        } else {
            DWORD dwCurrent = GetCurrentProcessorNumber();
        }
    }

    void ObjectThreadRoot::SetProcessor(unsigned int uiProcessor) {
        s_uiProcessor = uiProcessor;
        ApplyProcessor(uiProcessor);
    }

    void ObjectThreadRoot::ReadyToRun() { mRunning = true; }

    void ObjectThreadRoot::SetName(const String &strName) { mName = strName; }

    const char *ObjectThreadRoot::GetCurrentThreadName() {
        if (GetCurrentThread() != NULL) {
            return GetCurrentThread()->mName;
        } else {
            return NULL;
        }
    }

    unsigned int ObjectThreadRoot::GetDefaultStackSize() { return s_uiDefaultStackSize; }

    void ObjectThreadRoot::SetDefaultStackSize(unsigned int uiSize) {
        s_uiDefaultStackSize = uiSize;
    }

}
