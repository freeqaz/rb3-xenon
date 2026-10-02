// Quazal NetZ - .\ChangeDupSetOperation.cpp
//
// The retail TU is 0x82ABDC98..0x82ABDF78: the constructor, the two in-class
// virtuals and the scalar deleting destructor its vtable (0x82181724) needs,
// then the destructor, AttachMigrationContext, GetImplicitStationConnection
// and Clone. Its .rdata is the vtable, "ChangeDupSet" and the file string,
// which ends at 0x82181778, where FaultProcessingContext's begins.
//
// Built /Od /Ob1 with EH off (no function carries an EH state). TraceImpl,
// ForceImplOperationCommonMethodsMacro and both CallsBackOnDataSet overloads
// are not in the TU: retail's vtable points them at shared empty/`return true`
// bodies elsewhere (0x82AC5BA8, 0x82AB4438, 0x82AB43A8, 0x82AA8860).
//
// The classes are declared here only as far as this TU uses them.

namespace Quazal {

    class RootObject {
    public:
        ~RootObject() {}
        static void *operator new(unsigned int, const char *, unsigned int);
        static void operator delete(void *);
        static void operator delete(void *, const char *, unsigned int);
    };

    class DOHandle : public RootObject {
    public:
        DOHandle(unsigned int val = 0) : mValue(val) {}
        DOHandle(const DOHandle &h) : mValue(h.mValue) {}
        ~DOHandle() {}

        unsigned int mValue; // 0x0
    };

    class DuplicatedObject;
    class Station;

    class DORef : public RootObject {
    public:
        DORef(DOHandle);
        ~DORef();

        DuplicatedObject *GetDOPtr() const { return m_poReferencedDO; }

        DuplicatedObject *m_poReferencedDO; // 0x0
        DOHandle m_hReferencedDO; // 0x4
        bool m_bLockRelevance; // 0x8
    };

    // The station reference is constructed through an inline DOHandle
    // constructor that calls DORef's out of line (two handle temps in the
    // retail constructor).
    template <class T>
    class DORefTemplate : public DORef {
    public:
        DORefTemplate(DOHandle h) : DORef(h) {}
        ~DORefTemplate() {}
    };

    class UserContext : public RootObject {
    public:
        unsigned int m_uiValue;
    };

    class StateMachine {
    public:
        class QEvent : public RootObject {
        public:
            virtual ~QEvent() {}
            virtual unsigned short GetSignal() const = 0;

            bool m_bRepeatEvent; // 0x4
        };
    };

    class Operation : public StateMachine::QEvent {
    public:
        enum _Event {
        };
        virtual ~Operation() {}
        virtual unsigned short GetSignal() const { return GetType(); }
        virtual int GetType() const = 0;
        virtual const char *GetClassNameString() const = 0;
        virtual void ForceImplOperationCommonMethodsMacro() = 0;
        virtual void TraceImpl(_Event, unsigned int) const = 0;

        DOHandle GetOrigin() const { return m_uiOrigin; }

        bool m_bOperationAborted; // 0x8
        UserContext m_uUserData; // 0xc
        unsigned int m_uiOrigin; // 0x10
    };

    class DOOperation : public Operation {
    public:
        DOOperation(DOHandle, DuplicatedObject *);
        virtual ~DOOperation();
        virtual DOHandle GetImplicitStationConnection() const;
        virtual DOOperation *Clone() const;
        virtual bool CallsBackOnDataSet() = 0;
        virtual bool CallsBackOnDataSet(unsigned char) = 0;

        DORef m_refTargetObject; // 0x14
    };

    class ChangeDupSetOperation : public DOOperation {
    public:
        enum Context {
        };
        ChangeDupSetOperation(DOHandle, DuplicatedObject *, DOHandle, bool, Context);
        virtual ~ChangeDupSetOperation();
        virtual int GetType() const { return 0xE; }
        virtual const char *GetClassNameString() const { return "ChangeDupSet"; }
        virtual void ForceImplOperationCommonMethodsMacro();
        virtual void TraceImpl(_Event, unsigned int) const;
        virtual DOHandle GetImplicitStationConnection() const;
        virtual DOOperation *Clone() const;
        virtual bool CallsBackOnDataSet();
        virtual bool CallsBackOnDataSet(unsigned char);

        void AttachMigrationContext(unsigned short);

        DORefTemplate<Station> m_refStation; // 0x20
        bool m_bAdd; // 0x2c
        unsigned short m_uiMigrationContext; // 0x2e
        Context m_eContext; // 0x30
        unsigned char m_ucFlags; // 0x34
    };

    ChangeDupSetOperation::ChangeDupSetOperation(
        DOHandle hTarget, DuplicatedObject *pDO, DOHandle hStation, bool bAdd, Context eContext
    )
        : DOOperation(hTarget, pDO), m_refStation(hStation) {
        m_bAdd = bAdd;
        m_eContext = eContext;
        m_uiMigrationContext = 0;
        m_ucFlags = 1;
    }

    ChangeDupSetOperation::~ChangeDupSetOperation() {}

    void ChangeDupSetOperation::AttachMigrationContext(unsigned short uiContext) {
        m_uiMigrationContext = uiContext;
    }

    DOHandle ChangeDupSetOperation::GetImplicitStationConnection() const {
        if (m_bAdd) {
            return m_refStation.m_hReferencedDO;
        } else {
            return DOHandle();
        }
    }

    DOOperation *ChangeDupSetOperation::Clone() const {
        ChangeDupSetOperation *pOp = new (__FILE__, 0x2D) ChangeDupSetOperation(
            GetOrigin(),
            m_refTargetObject.GetDOPtr(),
            m_refStation.m_hReferencedDO,
            m_bAdd,
            m_eContext
        );
        pOp->m_ucFlags = m_ucFlags;
        pOp->m_uiMigrationContext = m_uiMigrationContext;
        return pOp;
    }
}
