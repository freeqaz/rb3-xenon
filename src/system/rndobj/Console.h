#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Overlay.h"
#include "os/Keyboard.h"

// size 0x68
class RndConsole : public Hmx::Object {
public:
    struct Breakpoint { // total size: 0x8
        Breakpoint() : parent(0) {}
        ~Breakpoint();
        DataArray *parent; // offset 0x0, size 0x4
        int index; // offset 0x4, size 0x4
    };

    RndConsole();
    virtual ~RndConsole();
    virtual DataNode Handle(DataArray *, bool);

    void MoveLevel(int);
    void SetBreak(DataArray *);
    void Clear(int);
    void Breakpoints();
    void Break(DataArray *);
    void List();
    void Where();
    void Step(int);
    void Continue();
    void Help(Symbol);
    bool Showing() { return mShowing; }
    void SetShowing(bool);

private:
    void ExecuteLine();
    void InsertBreak(DataArray *, int);
    bool OnMsg(const KeyboardKeyMsg &);

    bool mShowing; // 0x2c
    RndOverlay *mOutput; // 0x30
    RndOverlay *mInput; // 0x34
    std::list<String> mBuffer; // 0x38
    std::list<String>::iterator mBufPtr; // 0x40
    int mMaxBuffer; // 0x44
    int mTabLen; // 0x48
    int mCursor; // 0x4c
    Hmx::Object *mKeyboardOverride; // 0x50
    bool mPumpMsgs; // 0x54
    DataArray *mDebugging; // 0x58
    int mLevel; // 0x5c
    std::list<Breakpoint> mBreakpoints; // 0x60
};
