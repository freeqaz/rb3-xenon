#include "utl/OSCMessenger.h"
#include "os/Debug.h"
#include "os/HolmesClient.h"
#include "os/NetworkSocket.h"
#include "os/System.h"
#include "utl/Std.h"

OSCMessenger TheOSCMessenger;

OSCMessenger::~OSCMessenger() {
    delete mSocket1;
    delete mSocket2;
}

void OSCMessenger::Connect() {
    if (!UsingCD()) {
        unsigned int ip = HolmesResolveIP().mIP;
        if ((int)ip != 0) {
            mSocket1 = NetworkSocket::Create(false);
            mSocket1->Bind(0x303A);
            mSocket1->Listen();
            mSocket2 = NetworkSocket::Create(false);
            mSocket2->Connect(ip, 0x3039);
        }
    }
}

void OSCMessenger::Poll() {
    if (mSocket1 && mSocket1->CanRead()) {
        char data[0x80];
        mSocket1->Recv(data, 0x80);
        char str[0x80];
        strncpy(str, data, 0x80);
        str[0x7f] = 0;
        int pos = strlen(str) / 4 + 1;
        OSCValue value;
        value.mAddress = str;
        value.mHasNewValue = 1;
        MILO_ASSERT(data[pos] == ',', 0x46);
        char c4 = data[pos + 1];
        if (c4 == 's') {
            MILO_ASSERT(data[pos+2] == 0, 0x49);
            MILO_ASSERT(data[pos+3] == 0, 0x4A);
            strncpy(value.buffer, &data[pos + 4], 0x80);
            value.mType = 's';
        } else if (c4 == 'i') {
            MILO_ASSERT(data[pos+2] == 0, 0x51);
            MILO_ASSERT(data[pos+3] == 0, 0x52);
            value.mType = 'i';
            value.buffer[0] = data[pos + 4];
        } else if (c4 == 'f') {
            if (data[pos + 2] == 'f' && data[pos + 3] == 'f') {
                value.mType = 'v';
                int *valueBuffer = (int *)value.buffer;
                int *dataIn = (int *)data;
                valueBuffer[1] = dataIn[2];
                valueBuffer[0] = dataIn[1];
                valueBuffer[2] = dataIn[3];
            } else {
                MILO_ASSERT(data[pos+2] == 0, 0x67);
                MILO_ASSERT(data[pos+3] == 0, 0x68);
                value.mType = 'f';
                value.buffer[0] = data[pos + 4];
            }
        }
        bool found = false;
        FOREACH (it, mValues) {
            if (it->mAddress == str) {
                memcpy(it->buffer, value.buffer, 0x80);
                found = true;
                it->mHasNewValue = 1;
                break;
            }
        }
        if (!found) {
            mValues.push_back(value);
        }
    }
}

int OSCMessenger::GetInt(String str, int intValue) {
    OSCValue *val = GetValue(str);
    if (val) {
        MILO_ASSERT(val->mType == 'i', 0x131);
        intValue = ((int *)val->buffer)[0];
        val->mHasNewValue = 0;
        return intValue;
    } else {
        OSCValue newValue;
        newValue.mAddress = str;
        newValue.mHasNewValue = 0;
        newValue.mType = 'i';
        mValues.push_back(newValue);
        return intValue;
    }
}

OSCMessenger::OSCValue *OSCMessenger::GetValue(String str) {
    FOREACH (it, mValues) {
        if (it->mAddress == str) {
            return &(*it);
        }
    }
    return 0;
}

void OSCMessenger::SendOSCFloat(String str, float value) {
    if (mSocket2) {
        char buf[0x120];
        int len = MakeOSCAddress(str, buf);
        int i = len;
        buf[i++] = ',';
        buf[i++] = 'f';
        buf[i++] = '\0';
        buf[i++] = '\0';
        memcpy(&buf[i], &value, sizeof(float));
        mSocket2->Send(buf, i + 4);
    }
}

int OSCMessenger::MakeOSCAddress(String str, char *buf) {
    int len = strlen(str.c_str());
    strncpy(buf, str.c_str(), 0x80);
    int rem = len % 4;
    memset(buf + len, 0, 4 - rem);
    return len - rem + 4;
}

float OSCMessenger::GetFloat(String str, float fValue) {
    OSCValue *val = GetValue(str);
    if (val) {
        MILO_ASSERT(val->mType == 'f', 0x149);
        fValue = *(float *)val->buffer;
        val->mHasNewValue = 0;
    } else {
        OSCValue newValue;
        newValue.mHasNewValue = 0;
        newValue.mAddress = str;
        newValue.mType = 'f';
        *(float *)newValue.buffer = fValue;
        mValues.push_back(newValue);
    }
    return fValue;
}
