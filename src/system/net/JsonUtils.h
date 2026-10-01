#pragma once
#include "net/json-c/json_object.h"
#include "types.h"
#include "utl/Str.h"

class JsonObject {
public:
    enum EType { // From RB3
        kType_Null = 0,
        kType_Boolean = 1,
        kType_Double = 2,
        kType_Int = 3,
        kType_Object = 4,
        kType_Array = 5,
        kType_String = 6
    };

    JsonObject() : mObject(nullptr) {}
    virtual ~JsonObject() { Release(); }

    lh_table *Get() const { return json_object_get_object(mObject); }
    void Set(json_object *o) { mObject = o; }
    void AddRef() { json_object_get(mObject); }
    void Release() { json_object_put(mObject); }

    EType GetType() const;
    char const *Str() const;
    bool Bool() const;
    int Int() const;
    double Double() const;

protected:
    friend class JsonArray;
    json_object *mObject; // 0x4
};

class JsonArray : public JsonObject {
    friend class JsonConverter;

private:
    JsonArray();
    virtual ~JsonArray();

    json_object *operator[](int idx) { return json_object_array_get_idx(mObject, idx); }

public:
    void AddMember(JsonObject *);
    int GetSize() const;
};

// Leaf values. Retail RTTI names all three (vtables 0x8219B1CC/D4/DC); none
// declares a destructor of its own, so each vtable's only slot is the
// JsonObject deleting destructor. The constructors are out of line.
class JsonString : public JsonObject {
public:
    JsonString(const char *);
};

class JsonInt : public JsonObject {
public:
    JsonInt(int);
};

class JsonDouble : public JsonObject {
public:
    JsonDouble(double);
};

class JsonConverter : public JsonArray {
public:
    JsonConverter();
    virtual ~JsonConverter();

    /** Retail fn_82B82260 (108 B) in unit `default/JsonUtils`. Declared here (not
        merely in the parallel src/network/net/JsonUtils.h, which nothing compiles)
        because retail emits it OUT OF LINE from this TU. */
    JsonArray *NewArray();
    JsonString *NewString(const char *);
    JsonInt *NewInt(int);
    JsonDouble *NewDouble(double);

    JsonObject *LoadFromString(String const &);
    JsonObject *GetValue(JsonArray *, int);
    const char *Str(JsonArray *, int);
    JsonObject *GetByName(JsonObject *, char const *);
    void PushObject(JsonObject *);

private:
    std::vector<JsonObject *> mObjects; // 0x8
};
