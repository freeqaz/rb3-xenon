#ifndef NET_JSONUTILS_H
#define NET_JSONUTILS_H

#include "net/json-c/json.h"
#include "system/utl/Str.h"
#include <vector>

class JsonObject {
public:
    JsonObject();
    virtual ~JsonObject();

    enum JsonType {
        kType_Null = 0,
        kType_Boolean = 1,
        kType_Double = 2,
        kType_Int = 3,
        kType_Object = 4,
        kType_Array = 5,
        kType_String = 6
    };

    json_object *GetObject() { return mObject; }
    const char *GetObjectAsString() const;
    JsonType GetType() const;

    friend class JsonConverter;

protected:
    json_object *mObject;
};

class JsonArray : public JsonObject {
public:
    JsonArray();
    virtual ~JsonArray();

    void AddMember(JsonObject *);
    int GetSize() const;
};

class JsonString : public JsonObject {
public:
    JsonString(const char *);
    virtual ~JsonString();

    const char *GetValue() const;
};

class JsonInt : public JsonObject {
public:
    JsonInt(int);
    virtual ~JsonInt();

    int GetValue() const;
};

class JsonDouble : public JsonObject {
public:
    JsonDouble(double);
    virtual ~JsonDouble();

    double GetValue() const;
};

class JsonConverter : public JsonArray {
public:
    JsonConverter();
    virtual ~JsonConverter();

    // Same name as src/system/net/JsonUtils.h's member.  This header is a
    // second, older declaration of the json classes that only
    // band3/net_band/RockCentral.cpp includes; with the member spelled
    // `objects` the two TUs compiled JsonConverter differently
    // (tools/layout_odr.py).
    std::vector<JsonObject *> mObjects; // 0x8

    JsonArray *NewArray();
    JsonString *NewString(const char *value);
    JsonInt *NewInt(int value);
    JsonDouble *NewDouble(double value);

    JsonObject *LoadFromString(const String &str);
    JsonObject *GetElement(JsonArray *array, int index);

private:
    void PushObject(JsonObject *obj);
};

#endif
