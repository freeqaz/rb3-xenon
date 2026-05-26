#pragma once

// Xbox Companion (SmartGlass) API header

struct HJSONWRITER__;
struct HJSONREADER__;

enum _JSONTokenType {
    kJSONTokenNone = 0,
    kJSONTokenBeginArray = 1,
    kJSONTokenEndArray = 2,
    kJSONTokenBeginMap = 3,
    kJSONTokenEndMap = 4,
    kJSONTokenString = 5,
    kJSONTokenNumber = 6,
    kJSONTokenTrue = 7,
    kJSONTokenFalse = 8,
    kJSONTokenNull = 9,
    kJSONTokenFieldName = 10,
    kJSONTokenEnd = 11,
    kJSONTokenComment = 12,
    kJSONTokenError = 13,
};

enum _XBC_EVENT_TYPE {
    XBC_EVENT_CLIENT_CONNECTED = 1,
    XBC_EVENT_CLIENT_DISCONNECTED = 2,
    XBC_EVENT_SEND_COMPLETE = 3,
    XBC_EVENT_DATA_RECEIVED = 4,
};

enum _XBC_DELIVERY_METHOD {
    XBC_DELIVERY_DEFAULT = 0,
};

#define XBC_MAX_CLIENTS 4

struct _XBC_EVENT_PARAMS {
    unsigned long clientID;
    unsigned int userIndex;
    _XBC_EVENT_TYPE eventType;
    HJSONREADER__ *jsonReader;
};

typedef void (*XBC_CALLBACK)(long, _XBC_EVENT_PARAMS *, void *);

// XBC API
long XbcInitialize(XBC_CALLBACK callback, void *state);
long XbcDoWork();
long XbcSendJSON(_XBC_DELIVERY_METHOD method, unsigned long clientID, HJSONWRITER__ *writer, void *context);

// XJSON Writer API
HJSONWRITER__ *XJSONCreateWriter();
long XJSONBeginArray(HJSONWRITER__ *writer);
long XJSONEndArray(HJSONWRITER__ *writer);
long XJSONWriteStringValue(HJSONWRITER__ *writer, const char *str, unsigned long len);
long XJSONWriteNumberValue(HJSONWRITER__ *writer, double value);
long XJSONWriteNullValue(HJSONWRITER__ *writer);
long XJSONGetBuffer(HJSONWRITER__ *writer, char *buffer, unsigned long *size);
long XJSONCloseWriter(HJSONWRITER__ *writer);

// XJSON Reader API
HJSONREADER__ *XJSONCreateReader();
long XJSONSetBuffer(HJSONREADER__ *reader, const char *buffer, unsigned long size, int flags);
long XJSONReadToken(HJSONREADER__ *reader, _JSONTokenType *tokenType, unsigned long *param1, unsigned long *param2);
long XJSONGetTokenValue(HJSONREADER__ *reader, wchar_t *buffer, unsigned long bufSize);
long XJSONCloseReader(HJSONREADER__ *reader);
