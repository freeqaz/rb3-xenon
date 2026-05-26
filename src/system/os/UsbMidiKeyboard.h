#pragma once
#include "os/CritSec.h"
#include "os/Joypad.h"

// ProKeysData overlaps ProGuitarData at offset 0x34 in JoypadData
// This is the keyboard-specific interpretation of the same 16 bytes
struct ProKeysData {
    unsigned char unk0[8]; // bitfielded key data, 1 bit bool + 7 bit velocity

    bool mSustain : 1;
    unsigned char unk8char : 7;

    bool mStompPedal : 1;
    unsigned char mExpressionPedal : 7;

    bool unkabool : 1;
    unsigned char unkachar : 7; // modval / accelerometer axis 0

    bool unkbbool : 1;
    unsigned char unkbchar : 7;

    bool unkcbool : 1;
    unsigned char unkcchar : 7;

    bool unkdbool : 1;
    unsigned char unkdchar : 7;

    bool unkebool : 1;
    unsigned char unkemiddle : 2;
    unsigned char mLowHandPlacement : 5;

    unsigned char mConnectedAccessories;
};

class UsbMidiKeyboard {
public:
    UsbMidiKeyboard();
    ~UsbMidiKeyboard();

    static bool mUsbMidiKeyboardExists;
    static void Init();
    static void Terminate();
    static void Poll();
    static void SendMessage(const Message &msg) { JoypadPushThroughMsg(msg); }

    int GetAccelAxisVal(int pad, int axis) {
        if (0 <= axis && (unsigned int)axis < 4)
            return mAccelerometer[pad][axis];
        return 0;
    }

    bool GetKeyPressed(int pad, int key) {
        if ((unsigned int)key <= 0x7F)
            return mKeyPressed[pad][key];
        return false;
    }

    int GetKeyVelocity(int pad, int key) {
        if ((unsigned int)key <= 0x7F)
            return mKeyVelocity[pad][key];
        return 0;
    }

    void SetAccelerometer(int pad, int a1, int a2, int a3) {
        mAccelerometer[pad][0] = a1;
        mAccelerometer[pad][1] = a2;
        mAccelerometer[pad][2] = a3;
    }

    void SetSustain(int pad, bool sus) { mSustain[pad] = sus; }
    void SetStompPedal(int pad, bool stomp) { mStompPedal[pad] = stomp; }
    void SetModVal(int pad, int mod) { mModVal[pad] = mod; }
    void SetExpressionPedal(int pad, int exp) { mExpressionPedal[pad] = exp; }
    void SetConnectedAccessories(int pad, int conn) { mConnectedAccessories[pad] = conn; }
    void SetLowHandPlacement(int pad, int lh) { mLowHandPlacement[pad] = lh; }
    void SetHighHandPlacement(int pad, int hh) { mHighHandPlacement[pad] = hh; }

    void SetKeyPressed(int pad, int key, bool pressed) {
        if (0x7F < (unsigned int)key)
            return;
        mKeyPressed[pad][key] = pressed;
    }

    void SetKeyVelocity(int pad, int key, int vel) {
        if (0x7F < (unsigned int)key)
            return;
        mKeyVelocity[pad][key] = vel;
    }

    bool GetSustain(int);
    bool GetStompPedal(int i) const { return mStompPedal[i]; }
    int GetModVal(int i) const { return mModVal[i]; }
    int GetExpressionPedal(int i) const { return mExpressionPedal[i]; }
    int GetConnectedAccessory(int i) const { return mConnectedAccessories[i]; }
    int GetLowHandPlacement(int i) const { return mLowHandPlacement[i]; }
    int GetHighHandPlacement(int i) const { return mHighHandPlacement[i]; }

private:
    int GetSlottedKeyVelocityFromExtended(int, unsigned char *);

    bool mKeyPressed[4][128]; // 0x0
    int mKeyVelocity[4][128]; // 0x200
    int mModVal[4]; // 0xa00
    int mExpressionPedal[4]; // 0xa10
    int mConnectedAccessories[4]; // 0xa20
    bool mSustain[4]; // 0xa30
    bool mStompPedal[4]; // 0xa34
    int mAccelerometer[4][3]; // 0xa38
    int mLowHandPlacement[4]; // 0xa68
    int mHighHandPlacement[4]; // 0xa78
    int mPadNum; // 0xa88
};

class StaticCriticalSection : public CriticalSection {
public:
    StaticCriticalSection();
    ~StaticCriticalSection();
    static StaticCriticalSection *Instance();
};
