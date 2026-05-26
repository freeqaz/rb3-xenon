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

protected:
    json_object *mObject; // 0x4
};

class JsonArray : public JsonObject {
    friend class JsonConverter;

private:
    JsonArray();
    virtual ~JsonArray();

    json_object *operator[](int idx) { return json_object_array_get_idx(mObject, idx); }

public:
    int GetSize() const;
};

class JsonConverter : public JsonArray {
public:
    JsonConverter();
    virtual ~JsonConverter();

    JsonObject *LoadFromString(String const &);
    JsonObject *GetValue(JsonArray *, int);
    const char *Str(JsonArray *, int);
    JsonObject *GetByName(JsonObject *, char const *);
    void PushObject(JsonObject *);

private:
    std::vector<JsonObject *> mObjects; // 0x8
};
