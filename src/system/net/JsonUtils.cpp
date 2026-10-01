#include "net/JsonUtils.h"
#include "net/json-c/json_object_private.h"
#include "net/json-c/json_object.h"
#include "net/json-c/json_tokener.h"
#include "net/json-c/linkhash.h"
#include "net/json-c/printbuf.h"
#include "os/Debug.h"

#pragma region JsonObject

JsonObject::EType JsonObject::GetType() const {
    if (mObject)
        return (JsonObject::EType)json_object_get_type(mObject);
    else
        return kType_Null;
}

char const *JsonObject::Str() const {
    MILO_ASSERT(GetType() == kType_String, 0x20);
    return json_object_get_string(mObject);
}

bool JsonObject::Bool() const {
    MILO_ASSERT(GetType() == kType_Boolean, 0x26);
    return json_object_get_boolean(mObject);
}

// Retail does not query the type here; the check is native-only.
int JsonObject::Int() const {
#ifdef HX_NATIVE
    MILO_ASSERT(GetType() == kType_Int, 0x2c);
#endif
    return json_object_get_int(mObject);
}

double JsonObject::Double() const {
#ifdef HX_NATIVE
    MILO_ASSERT(GetType() == kType_Double, 0x32);
#endif
    return json_object_get_double(mObject);
}

#pragma endregion JsonObject
#pragma region JsonArray

JsonArray::JsonArray() { mObject = json_object_new_array(); }

JsonArray::~JsonArray() {
    for (int i = json_object_array_length(mObject) - 1; i >= 0; i--) {
        json_object_put(json_object_array_get_idx(mObject, i));
    }
}

int JsonArray::GetSize() const { return json_object_array_length(mObject); }

// The array takes its own reference on the member's json object.
void JsonArray::AddMember(JsonObject *obj) {
    obj->AddRef();
    json_object_array_add(mObject, obj->mObject);
}

JsonString::JsonString(const char *s) { mObject = json_object_new_string(s); }

JsonInt::JsonInt(int i) { mObject = json_object_new_int(i); }

JsonDouble::JsonDouble(double d) { mObject = json_object_new_double(d); }

#pragma endregion JsonArray
#pragma region JsonConverter

JsonConverter::JsonConverter() {}

JsonConverter::~JsonConverter() {
    if (mObjects.size() > 0) {
        for (int i = mObjects.size() - 1; i >= 0; i--) {
            delete mObjects[i];
        }
    }
}

// Retail fn_82B82260 (108 B). NOT DEFINED ANYWHERE in this tree before now:
// src/network/net/JsonUtils.h declares a `NewArray()` on a structurally different
// JsonConverter, and RockCentral.obj carries the mangled name only as an UNDEFINED
// external reference -- which is why a byte-scan for the symbol is not a definition
// test (COFF SectionNumber must be > 0).
//
// Retail-byte evidence (lane W16-BO):
//   li r3, 0x8                <- sizeof(JsonArray) == 8, per cl /d1reportSingleClassLayout
//   bl operator new / null guard
//   bl 0x82B81DE0             <- ??0JsonArray@@AAA@XZ  (`AAA` == PRIVATE ctor, which is
//                                exactly what this header declares; the parallel
//                                network/ header makes it public, so retail agrees with
//                                THIS class shape)
//   lwz r3, 4(r31) / bl json_object_get      <- AddRef() inlined; mObject is at +0x4
//   addi r3, r30, 8 / bl vector<T*>::push_back  <- mObjects is at +0x8
//   mr r3, r31                <- returns the new array
// Independently corroborated by the call graph: exactly FOUR retail `bl` callers, all
// inside ?DataPointToQString@RockCentral@@SAXABVDataPoint@@AAVString@Quazal@@@Z, and our
// RockCentral::DataPointToQString calls jc.NewArray() exactly four times.
JsonArray *JsonConverter::NewArray() {
    JsonArray *arr = new JsonArray();
    // EXPERIMENT (W16-BO): name the upcast temporary. Retail sinks the
    // `stw r31,0x50(r1)` that materialises push_back's const-ref argument BEFORE
    // `bl json_object_get`; we emit it after (the row's 1 insert + 1 delete). A
    // named JsonObject* lvalue is initialised at its declaration, which should
    // place that store ahead of the AddRef call. Semantically identical -- the
    // upcast is offset 0 under single inheritance.
    JsonObject *entry = arr;
    arr->AddRef();
    mObjects.push_back(entry);
    return arr;
}

// Each New* takes one reference for the converter, which releases its objects
// when it is destroyed (same shape as NewArray).
JsonString *JsonConverter::NewString(const char *s) {
    JsonString *str = new JsonString(s);
    JsonObject *entry = str;
    str->AddRef();
    mObjects.push_back(entry);
    return str;
}

JsonInt *JsonConverter::NewInt(int i) {
    JsonInt *val = new JsonInt(i);
    JsonObject *entry = val;
    val->AddRef();
    mObjects.push_back(entry);
    return val;
}

JsonDouble *JsonConverter::NewDouble(double d) {
    JsonDouble *val = new JsonDouble(d);
    JsonObject *entry = val;
    val->AddRef();
    mObjects.push_back(entry);
    return val;
}

JsonObject *JsonConverter::LoadFromString(const String &str) {
#ifdef HX_NATIVE
    printbuf *buf = printbuf_new();
    if (!buf) {
        return nullptr;
    }
    printbuf_memappend(buf, str.c_str(), str.length());
    json_object *obj = json_tokener_parse(buf->buf);
    if (!obj) {
        printbuf_free(buf);
        return nullptr;
    }
    JsonObject *jObj = new JsonObject();
    jObj->Set(obj);
    printbuf_free(buf);
    JsonObject *temp = jObj;
    json_object_get(obj);
    mObjects.push_back(temp);
    return jObj;
#else
    // Retail (fn_82B82440): the object is allocated first, the parse result is
    // stored without an error check, and the length is a strlen of the text.
    JsonObject *jObj = new JsonObject();
    printbuf *buf = printbuf_new();
    if (!buf) {
        return nullptr;
    }
    printbuf_memappend(buf, str.c_str(), strlen(str.c_str()));
    jObj->Set(json_tokener_parse(buf->buf));
    printbuf_free(buf);
    jObj->AddRef();
    mObjects.push_back(jObj);
    return jObj;
#endif
}

JsonObject *JsonConverter::GetValue(JsonArray *inArray, int inIdx) {
    MILO_ASSERT(0 <= inIdx && inIdx <= inArray->GetSize(), 0x10a);
    // retail takes ONE reference (on the element, still in r3) and pushes --
    // not AddRef + PushObject's second AddRef
    JsonObject *obj = new JsonObject();
    json_object *o = (*inArray)[inIdx];
    obj->Set(o);
    JsonObject *temp = obj;
    json_object_get(o);
    mObjects.push_back(temp);
    return obj;
}

const char *JsonConverter::Str(JsonArray *j, int i) { return GetValue(j, i)->Str(); }

JsonObject *JsonConverter::GetByName(JsonObject *j, const char *cc) {
    if (j->GetType() != JsonObject::kType_Object) {
        return nullptr;
    }
    lh_table *lh = j->Get();
    const void *v = lh_table_lookup(lh, cc);
    if (!v) {
        return nullptr;
    }
    json_object *obj = (json_object *)v;
    json_object_get(obj);
    JsonObject *jObj = new JsonObject();
    jObj->Set(obj);
    jObj->AddRef();
    mObjects.push_back(jObj);
    return jObj;
}

void JsonConverter::PushObject(JsonObject *obj) {
    obj->AddRef();
    mObjects.push_back(obj);
}

#pragma endregion JsonConverter
