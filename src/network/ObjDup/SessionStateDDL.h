#pragma once
#include "ObjDup/DataSet.h"
#include "ObjDup/DOHandle.h"
#include "Core/Operation.h"
#include "Platform/RootObject.h"
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class Message;

    class Variable : public RootObject {
    public:
        int unk0;
        int unk4;
        const char *m_szName; // 0x8
    };

    class _Type_byte {
    public:
        static inline bool FormatVariableValue(const void *v, String *out) {
            out->Format("%d", *(const unsigned char *)v);
            return true;
        }
    };

    class _Type_bool {
    public:
        static inline bool FormatVariableValue(const void *v, String *out) {
            out->Format("%d", *(const bool *)v);
            return true;
        }
    };

    class _Type_uint16 {
    public:
        static inline bool FormatVariableValue(const void *v, String *out) {
            out->Format("%d", *(const unsigned short *)v);
            return true;
        }
    };

    class _Type_uint32 {
    public:
        static inline bool FormatVariableValue(const void *v, String *out) {
            out->Format("%d", *(const unsigned int *)v);
            return true;
        }
    };

    class _Type_dohandle {
    public:
        static inline bool FormatVariableValue(const void *v, String *out) {
            DOHandle h = *(const DOHandle *)v;
            out->Format("%s %x", h.GetClassNameString(), h.mValue);
            return true;
        }
    };

    class _DS_SessionState : public DataSet {
    public:
        _DS_SessionState() {}
        ~_DS_SessionState() {}

        bool FormatVariableValue(Variable *, String *) const;
        void AddSourceTo(Message *, Time, bool);
        void CallOperationOnVars(Operation::_Event, void *);

        unsigned char m_bySessionState; // 0x0
    };
}
