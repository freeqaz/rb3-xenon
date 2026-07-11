#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/DataUtl.h"
#include "utl/PoolAlloc.h"
#include <map>

extern Hmx::Object *gDataThis;

class DataFuncObj : public Hmx::Object {
private:
    DataArray *mFunc;

    DataFuncObj(DataArray *da) : mFunc(da) {
        da->AddRef();
        SetName(da->Str(1), ObjectDir::Main());
    }

public:
    virtual ~DataFuncObj() { mFunc->Release(); }
    virtual DataNode Handle(DataArray *_msg, bool _warn) {
        return mFunc->ExecuteScript(2, gDataThis, _msg, 1);
    }

    POOL_OVERLOAD(DataFuncObj, 0x4F);

    static DataNode New(DataArray *);
};

class DataThisPtr : public ObjPtr<Hmx::Object> {
public:
    DataThisPtr() : ObjPtr(nullptr, nullptr) {}
    virtual ~DataThisPtr() {}
#ifdef HX_NATIVE
    // Native ObjPtr keeps dc3's single-arg Replace virtual.
    virtual void Replace(Hmx::Object *);
#else
    // Retail X360 (rb3-Wii shape, verified vs fn_827385D0): overrides ObjPtr's
    // vtable slot +8 Replace(from, to) — the dying object arrives as `from`
    // and is compared against gDataThis directly. dc3's single-arg drift
    // captured the OLD mObject instead; retail never loads it.
    virtual void Replace(ObjRef *from, Hmx::Object *to);
#endif
};

#define DEF_DATA_FUNC(name) DataNode name(DataArray *array)

extern std::map<Symbol, DataFunc *> gDataFuncs;
extern DataThisPtr gDataThisPtr;

void DataRegisterFunc(Symbol s, DataFunc *func);
Symbol DataFuncName(DataFunc *);
bool FileListCallBack(char *);
void DataInitFuncs();
void DataTermFuncs();
