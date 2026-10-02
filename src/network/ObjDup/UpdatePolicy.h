#pragma once
#include "Platform/RootObject.h"
#include "Platform/Time.h"

namespace Quazal {
    class DuplicatedObject;
    class Message;
    class Station;

    // How one dataset of a DO is sent: an UpdateProtocol per dataset, plus
    // optional filters that veto updates. Each DO class keeps one UpdatePolicy
    // per dataset index (DOClass::GetUpdatePolicy).
    class UpdateProtocol : public RootObject {
    public:
        UpdateProtocol() {}
        virtual ~UpdateProtocol() {}
        virtual bool UsesSpecialInnerUpdateLoop() { return false; }
        // The by-value Time is destroyed on return, so even a constant return
        // goes through a stack temp (retail's stw/lwz at -0x10).
        virtual unsigned int SpecialInnerUpdateLoop(DuplicatedObject *, void *, unsigned char, Time) {
            return 0;
        }
        virtual unsigned int GetCommunicationFlags(DuplicatedObject *, void *, unsigned char) = 0;
        virtual void AddToMessage(DuplicatedObject *, void *, unsigned char, Time, Message *, bool) = 0;
        virtual void ExtractFromMessage(DuplicatedObject *, void *, unsigned char, Message *, bool) = 0;
    };

    // The default protocol: the dataset sends and extracts itself.
    template <class T>
    class BasicUpdateProtocol : public UpdateProtocol {
    public:
        // Retail tests a constant through an inline call (li 1; clrlwi; cmpwi)
        // rather than returning 1 directly.
        static bool UsesReliableUpdates() { return true; }
        virtual unsigned int GetCommunicationFlags(DuplicatedObject *, void *, unsigned char) {
            if (UsesReliableUpdates())
                return 1;
            else
                return 0;
        }
        virtual void
        AddToMessage(DuplicatedObject *, void *pData, unsigned char, Time t, Message *pMsg, bool b) {
            static_cast<T *>(pData)->AddSourceTo(pMsg, t, b);
        }
        virtual void
        ExtractFromMessage(DuplicatedObject *, void *pData, unsigned char, Message *pMsg, bool) {
            static_cast<T *>(pData)->ExtractFrom(pMsg);
        }
    };

    class GlobalUpdateFilter : public RootObject {
    public:
        virtual ~GlobalUpdateFilter() {}
        virtual bool UpdateRequired(DuplicatedObject *, void *, Time) = 0;
        virtual void UpdateSent(DuplicatedObject *, void *, Time, unsigned int) {}
    };

    // A filter for datasets that never change once sent.
    class ConstFilter : public GlobalUpdateFilter {
    public:
        virtual ~ConstFilter() {}
        virtual bool UpdateRequired(DuplicatedObject *, void *, Time) { return false; }
    };

    class UpdatePolicy : public RootObject {
    public:
        void Update(DuplicatedObject *, void *, unsigned char, Time);
        void AddToDiscoveryMessage(DuplicatedObject *, void *, unsigned char, Station *, Message *);
        void ExtractFromDiscoveryMessage(DuplicatedObject *, void *, unsigned char, Message *);
        void ExtractFromUpdateMessage(DuplicatedObject *, void *, unsigned char, Message *);
        void AddFilter(GlobalUpdateFilter *);
        void RegisterProtocol(UpdateProtocol *);
    };
}
