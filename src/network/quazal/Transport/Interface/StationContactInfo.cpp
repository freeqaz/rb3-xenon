// Quazal NetZ - StationContactInfo
//
// Retail TU: .text 0x82B2F110..0x82B2FC38, compiled /Od /Oi- /Ob1 /GR- (see
// objects.json). It follows JobConnectEndPoint and precedes
// TransportSignatureGenerator. It allocates nothing, so it has no __FILE__
// string; its .rdata is its own basic_string literal (0x8218C290), the EH
// tables of its two constructors, destructor and SortAndFilterTarget, then the
// strings and the vtable (0x8218C3B8, no complete-object locator) used by the
// COMDAT tail.
//
// The TU's own functions end with Trace at 0x82B2F99C. The rest
// (0x82B2F9A0..0x82B2FC38) is COMDAT code no function of this TU calls: an
// m_uiFirst/m_uiLast DDL helper pair, and a class with an 11-slot vtable that
// retail constructs from 0x82AF8028. It is not written here.
//
// A StationContactInfo is the URL list of one station plus the RV connection
// id those URLs share.
//
// The classes are declared here with the layouts retail uses rather than taken
// from the shared Quazal headers.
//
// This TU is built /Od: its locals are laid out by a walk over the scope's
// symbol hash table, so the local NAMES below determine the stack offsets.

#include "Platform/MemoryManager.h"
#include <list>

namespace Quazal {

    // The list allocator and list wrapper as JobConnectEndPoint uses them.
    template <class T>
    class MemAllocator {
    public:
        typedef unsigned int size_type;
        typedef int difference_type;
        typedef T value_type;
        typedef T *pointer;
        typedef T &reference;
        typedef const T *const_pointer;
        typedef const T &const_reference;

        template <class T2>
        struct rebind {
            typedef MemAllocator<T2> other;
        };

        MemAllocator() {}

        template <class T2>
        operator MemAllocator<T2>() const {
            return MemAllocator<T2>();
        }

        pointer address(reference value) const { return &value; }
        const_pointer address(const_reference value) const { return &value; }
        size_type max_size() const { return size_type(-1) / sizeof(T); }

        pointer allocate(const size_type count, const void *hint = 0) const {
            return reinterpret_cast<pointer>(MemoryManager::Allocate(
                MemoryManager::GetDefaultMemoryManager(), count * sizeof(T), "Unknown", 0,
                MemoryManager::_InstType7
            ));
        }
        void deallocate(pointer ptr, size_type count) const {
            MemoryManager::Free(
                MemoryManager::GetDefaultMemoryManager(), ptr, MemoryManager::_InstType7
            );
        }
        void construct(pointer ptr, const_reference value) const { new (ptr) T(value); }
        void destroy(pointer ptr) const { ptr->~T(); }
    };

    template <class T>
    class qList : public std::list<T, MemAllocator<T> >, public RootObject {
    public:
        qList() {}
    };

    class String : public RootObject {
    public:
        ~String();
        static bool IsEqual(const char *, const char *);

        char *m_szContent;
    };

    class StationURL : public RootObject {
    public:
        StationURL(const StationURL &);
        ~StationURL();
        String GetAddress() const;
        unsigned int GetRVConnectionID() const;
        unsigned int GetType() const;
        bool IsPublic() const { return (GetType() & 2) == 2; }
        void Trace(unsigned int) const;

        char m_data[0x64];
    };

    class OutputFormat : public RootObject {
    public:
        void IncreaseIndent(unsigned int);
        void DecreaseIndent(unsigned int);
    };

    class TraceLog : public RootObject {
    public:
        static TraceLog *GetInstance();
        OutputFormat *GetOutputFormat();
    };

    class StationContactInfo : public RootObject {
    public:
        StationContactInfo();
        StationContactInfo(const qList<StationURL> &);
        ~StationContactInfo();

        bool SetRVConnectionID(unsigned int);
        unsigned int GetRVConnectionID() const;
        bool AddURL(const StationURL &);
        void SortAndFilterTarget(StationContactInfo &) const;
        void Trace(unsigned int);

        unsigned int m_uiRVConnectionID; // 0x0
        qList<StationURL> m_lstURLs; // 0x4
    };

    StationContactInfo::StationContactInfo() { m_uiRVConnectionID = 0; }

    StationContactInfo::StationContactInfo(const qList<StationURL> &lstURLs) {
        m_uiRVConnectionID = 0;
        qList<StationURL>::const_iterator it = lstURLs.begin();
        while (it != lstURLs.end()) {
            AddURL(*it);
            ++it;
        }
    }

    StationContactInfo::~StationContactInfo() { m_lstURLs.clear(); }

    bool StationContactInfo::SetRVConnectionID(unsigned int uiRVConnectionID) {
        if (m_uiRVConnectionID == 0 || uiRVConnectionID == m_uiRVConnectionID) {
            m_uiRVConnectionID = uiRVConnectionID;
            return true;
        }
        return false;
    }

    unsigned int StationContactInfo::GetRVConnectionID() const { return m_uiRVConnectionID; }

    bool StationContactInfo::AddURL(const StationURL &oURL) {
        if (oURL.GetRVConnectionID() == 0) {
            m_lstURLs.push_back(oURL);
            return true;
        } else if (SetRVConnectionID(oURL.GetRVConnectionID())) {
            m_lstURLs.push_back(oURL);
            return true;
        }
        return false;
    }

    // Keeps the target's public URLs last, and drops its private ones unless
    // one of its public addresses is also one of ours (same NAT).
    void StationContactInfo::SortAndFilterTarget(StationContactInfo &oTarget) const {
        qList<StationURL>::iterator itTarget = oTarget.m_lstURLs.begin();
        bool bSameNAT = false;
        unsigned int uiNbPublic = 0;
        while (itTarget != oTarget.m_lstURLs.end() && !bSameNAT) {
            if (itTarget->IsPublic()) {
                qList<StationURL>::const_iterator itLocal = m_lstURLs.begin();
                while (itLocal != m_lstURLs.end() && !bSameNAT) {
                    bool bEqual = String::IsEqual(
                        itTarget->GetAddress().m_szContent, itLocal->GetAddress().m_szContent
                    );
                    if (bEqual) {
                        bSameNAT = true;
                    }
                    ++itLocal;
                }
                uiNbPublic++;
            }
            ++itTarget;
        }
        if (uiNbPublic == 0) {
            return;
        }
        qList<StationURL> lstSorted;
        if (bSameNAT) {
            for (itTarget = oTarget.m_lstURLs.begin(); itTarget != oTarget.m_lstURLs.end(); ++itTarget) {
                if (!itTarget->IsPublic()) {
                    lstSorted.insert(lstSorted.end(), *itTarget);
                }
            }
        }
        for (itTarget = oTarget.m_lstURLs.begin(); itTarget != oTarget.m_lstURLs.end(); ++itTarget) {
            if (itTarget->IsPublic()) {
                lstSorted.insert(lstSorted.end(), *itTarget);
            }
        }
        oTarget.m_lstURLs = lstSorted;
    }

    void StationContactInfo::Trace(unsigned int uiFlags) {
        TraceLog::GetInstance()->GetOutputFormat()->IncreaseIndent(2);
        qList<StationURL>::iterator it = m_lstURLs.begin();
        while (it != m_lstURLs.end()) {
            it->Trace(uiFlags);
            ++it;
        }
        TraceLog::GetInstance()->GetOutputFormat()->DecreaseIndent(2);
    }

}
