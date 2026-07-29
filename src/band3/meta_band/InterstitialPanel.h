#pragma once
#include "meta/DeJitterPanel.h"

// RB3-360 retail offsets, read off the target bodies in the 0x8261EFB8 block:
//   InterstitialPanel: mCamshotDone @0x90 (ctor stb, Draw/Enter/Exiting),
//                      unk88 @0x94 (int), mShowing @0x98 (ctor stb 1).
//   BackdropPanel:     mOutroDone @0x90 (ctor stb 0, Enter stb 1, Exiting).
// The rb3-Wii dev header's 0x85/0x88/0x8c are Wii-sized and wrong here.
// The redundant `virtual ~Derived() {}` redeclarations are dropped: retail's
// BackdropPanel scalar-deleting dtor (0x8261F8E0) has no own-vptr store.
class InterstitialPanel : public DeJitterPanel {
public:
    InterstitialPanel();
    OBJ_CLASSNAME(InterstitialPanel);
    OBJ_SET_TYPE(InterstitialPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Draw();
    virtual void Enter();
    virtual bool Exiting() const;
    virtual void Load();
    virtual void Unload();

    void SetCamshotDone();
    NEW_OBJ(InterstitialPanel);
    static void Init() { REGISTER_OBJ_FACTORY(InterstitialPanel); }

    bool mCamshotDone; // 0x90
    int unk88; // 0x94
    bool mShowing; // 0x98
};

class BackdropPanel : public DeJitterPanel {
public:
    BackdropPanel();
    OBJ_CLASSNAME(BackdropPanel);
    OBJ_SET_TYPE(BackdropPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Enter();
    virtual void Exit();
    virtual bool Exiting() const;

    void SetOutroDone();
    NEW_OBJ(BackdropPanel);
    static void Init() { REGISTER_OBJ_FACTORY(BackdropPanel); }

    bool mOutroDone; // 0x90
};
