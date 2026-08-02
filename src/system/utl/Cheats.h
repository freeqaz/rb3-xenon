#pragma once
#include "obj/Object.h"
#include "os/JoypadMsgs.h"
#include "os/Keyboard.h"
#include "obj/Data.h"
#include "os/User.h"

struct CheatLog {
    bool mQuick;
    int mPad;
    DataNode mScript;
};

struct KeyCheat {
    int mKey;
    bool mCtrl;
    bool mAlt;
    DataArray *mScript;
};
struct LongJoyCheat {
    LongJoyCheat() : ixProgress(0), mScript(nullptr) {}

    std::vector<int> mSequence;
    unsigned int ixProgress;
    DataArray *mScript;
};

struct QuickJoyCheat {
    int mButton;
    DataArray *mScript;
};

class CheatsManager : public Hmx::Object {
public:
    enum ShiftMode {
        kLeftShift = 0,
        kRightShift = 1,
        kDownShift = 2
    };

    CheatsManager();
    virtual DataNode Handle(DataArray *, bool);
    void AppendLog(FixedString &);

    Symbol CheatMode() { return mSymMode; }
    void SetKeyCheatsEnabled(bool b) { mKeyCheatsEnabled = b; };
    void Log(int, bool, DataArray *);
    bool KeyCheatsEnabled() { return mKeyCheatsEnabled; };
    void CallCheatScript(bool b1, DataArray *da, LocalUser *lu, bool b2);
    void RebuildKeyCheatsForMode();
#ifdef HX_NATIVE
    void SetUnsafeCheatsUsed(bool b) { mUnsafeCheatsUsed = b; };
#endif
    void AddQuickJoyCheat(const QuickJoyCheat &cheat, ShiftMode mode) {
        mQuickJoyCheats[mode].push_back(cheat);
    }
    void AddKeyCheat(const KeyCheat &cheat) { mKeyCheats.push_back(cheat); }
    void AddLongJoyCheat(const LongJoyCheat &cheat) { mLongJoyCheats.push_back(cheat); }

    Symbol SymMode() { return mSymMode; }
    void SetSymMode(Symbol sym) {
        if (sym != mSymMode) {
            mSymMode = sym;
            RebuildKeyCheatsForMode();
        }
    }

private:
    int OnMsg(ButtonDownMsg const &);
    DataNode OnMsg(KeyboardKeyReleaseMsg const &);
    DataNode OnMsg(KeyboardKeyMsg const &);

protected:
    std::vector<LongJoyCheat> mLongJoyCheats; // 0x2c
    std::vector<QuickJoyCheat> mQuickJoyCheats[2]; // 0x38
    std::vector<KeyCheat> mKeyCheats; // 0x50
    Symbol mSymMode; // 0x5c
    std::vector<QuickJoyCheat *> mJoyCheatPtrsMode[2]; // 0x60
    std::vector<KeyCheat *> mKeyCheatPtrsMode; // 0x78
    Timer mLastButtonTime; // 0x88
    bool mKeyCheatsEnabled; // 0xb8
    bool mJoyCheatsEnabled; // 0xb9
    bool mUnlockAll; // 0xba
    std::list<CheatLog> mBuffer; // 0xbc
    int mMaxBuffer; // 0xc4
#ifdef HX_NATIVE
    // NOT PRESENT IN RB3 RETAIL.  These five are DC3-era additions (dc3-decomp
    // is NEWER than RB3).  Retail's CheatsInit allocates the object with
    // `li r3, 0xc0`, and our class measured 0xd0 -- exactly these 10 bytes plus
    // their 6 bytes of tail padding.  Independently, the rb3-Wii DEV oracle's
    // CheatsManager also ends at mMaxBuffer.  Kept for the native build only.
    bool mCtrlOverriddeMode;
    bool mIsOverridingKeyboard;
    Hmx::Object *mPreviousOverride;
    bool mUnsafeCheatsUsed;
    bool mDisplayCheats;
#endif
    // String mMessage; // 0xd4
    // float mMessageTimer; // 0xdc
};

void EnableKeyCheats(bool);
bool GetEnabledKeyCheats();
bool CheatsInitialized();
void CheatsInit();
void LogCheat(int, bool, DataArray *);
void AppendCheatsLog(FixedString &);
void CallQuickCheat(DataArray *da, LocalUser *lu);
void InitQuickJoyCheats(const DataArray *a, CheatsManager::ShiftMode);
void CheatsTerminate();
Symbol GetCheatMode();
