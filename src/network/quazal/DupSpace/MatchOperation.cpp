// Quazal NetZ - DupSpace/MatchOperation.cpp
// Retail TU: .text 0x82B481D0..0x82B48824 (9 functions), built
// /Od /Oi- /EHs-c- /Ob1 (objects.json).
//
// The constructor is out of line and comes first; the vtable's inline virtuals
// (GetType, GetClassNameString, the deleting destructor) follow it. The
// ForceImplOperationCommonMethodsMacro body and Operation::GetSignal are
// folded into other TUs' identical copies.
//
// The declarations below are local to this TU; their layouts are the ones the
// retail code uses.

#include "Core/Operation.h"
#include "Core/Scheduler.h"
#include "ObjDup/DOClass.h"
#include "ObjDup/DORefTemplate.h"
#include "ObjDup/RootDO.h"
#include "Platform/ScopedCS.h"

#define MATCHOPERATION_FILE ".\\DupSpace\\MatchOperation.cpp"

namespace Quazal {

    class DuplicationSpace : public RootObject {
    public:
        static void CheckRelevance();
        void MatchSingleObject(DuplicatedObject *);
        void MatchAcrossPublishers(DuplicatedObject *);
        void MatchAcrossSubscribers(DuplicatedObject *);
        void MatchAcrossCells(DuplicatedObject *);
        void MatchAcrossPS(DuplicatedObject *);
    };

    class DupSpaceOperation : public Operation {
    public:
        DupSpaceOperation(DuplicationSpace *);
        virtual ~DupSpaceOperation();

        void OperationBegin();
        void OperationEnd();

        DuplicationSpace *m_pDupSpace; // 0x14
    };

    class MatchOperation : public DupSpaceOperation {
    public:
        enum _MatchType {
            CheckRelevance = 0,
            MatchSingleObject = 1,
            MatchAcrossPublishers = 2,
            MatchAcrossSubscribers = 3,
            MatchAcrossCells = 4,
            MatchAcrossPS = 5,
            NoMatch = 6,
        };

        MatchOperation(DuplicationSpace *, _MatchType, DOHandle);
        virtual ~MatchOperation();
        virtual int GetType() const { return 10; }
        virtual const char *GetClassNameString() const { return "Match"; }
        virtual void ForceImplOperationCommonMethodsMacro() {}
        virtual void TraceImpl(_Event, unsigned int) const;

        void ExecuteQueuedOperation(int);
        void ExecuteOperation();
        static void Queue(DuplicationSpace *, _MatchType, DOHandle, int);

        DOHandle GetObjectHandle() const { return m_hObject; }
        _MatchType GetMatchType() const { return m_eMatchType; }

        _MatchType m_eMatchType; // 0x18
        DOHandle m_hObject; // 0x1c
    };

    class DuplicationSpaceTable : public RootObject {
    public:
        static DuplicationSpaceTable *GetInstance();
        void QueueMatchOperation(MatchOperation *, int);

        unsigned int m_unk0[13];
        int m_iPendingRelevanceChecks; // 0x34
        void DecPending() { m_iPendingRelevanceChecks--; }
    };

    MatchOperation::MatchOperation(
        DuplicationSpace *pDupSpace, _MatchType eMatchType, DOHandle hObject
    )
        : DupSpaceOperation(pDupSpace) {
        m_eMatchType = eMatchType;
        m_hObject = hObject;
    }

    MatchOperation::~MatchOperation() {}

    // Retail returns at entry; the rest of the body is still emitted.
    void MatchOperation::TraceImpl(_Event eEvent, unsigned int uiTraceFlags) const {
        return;
        if (m_eMatchType == NoMatch)
            return;
        DORefTemplate<RootDO> oRef(GetObjectHandle());
        RootDO *pObject = oRef.Get();
        switch (m_eMatchType) {
            // Retail's dispatch is followed by an unconditional branch past the
            // switch: a statement ahead of the first case label.
            break;
        case MatchSingleObject:
            if (pObject == NULL)
                return;
            break;
        case MatchAcrossPublishers:
            if (pObject == NULL)
                return;
            break;
        case MatchAcrossSubscribers:
            if (pObject == NULL)
                return;
            break;
        case MatchAcrossCells:
            if (pObject == NULL)
                return;
            break;
        case MatchAcrossPS:
            if (pObject == NULL)
                return;
            break;
        }
    }

    void MatchOperation::ExecuteQueuedOperation(int iDelay) {
        if (GetMatchType() == CheckRelevance)
            DuplicationSpaceTable::GetInstance()->DecPending();
        ExecuteOperation();
        delete this;
    }

    // Retail's frame is 0x10 larger than this one: 12 bytes sit between the
    // DOHandle argument temporaries and the ScopedCS release temporary, the
    // footprint of an inline whose call does not appear in the code. What it
    // was is open; nothing is added here to reproduce it.
    void MatchOperation::ExecuteOperation() {
        ScopedCS oCS(Scheduler::GetInstance()->unk38);
        OperationBegin();
        // Retail branches over an empty block when there is nothing to match.
        if (m_eMatchType == NoMatch) {
        } else {
            DORefTemplate<RootDO> oRef(GetObjectHandle());
            RootDO *pObject = oRef.Get();
            switch (m_eMatchType) {
            case CheckRelevance:
                DuplicationSpace::CheckRelevance();
                break;
            case MatchSingleObject:
                if (pObject)
                    m_pDupSpace->MatchSingleObject(pObject);
                break;
            case MatchAcrossPublishers:
                if (pObject)
                    m_pDupSpace->MatchAcrossPublishers(pObject);
                break;
            case MatchAcrossSubscribers:
                if (pObject)
                    m_pDupSpace->MatchAcrossSubscribers(pObject);
                break;
            case MatchAcrossCells:
                if (pObject)
                    m_pDupSpace->MatchAcrossCells(pObject);
                break;
            case MatchAcrossPS:
                if (pObject)
                    m_pDupSpace->MatchAcrossPS(pObject);
                break;
            }
        }
        OperationEnd();
    }

    void MatchOperation::Queue(
        DuplicationSpace *pDupSpace, _MatchType eMatchType, DOHandle hObject, int iDelay
    ) {
        MatchOperation *pOperation = new (MATCHOPERATION_FILE, 0x9d)
            MatchOperation(pDupSpace, eMatchType, hObject);
        DuplicationSpaceTable::GetInstance()->QueueMatchOperation(pOperation, iDelay);
    }

}
