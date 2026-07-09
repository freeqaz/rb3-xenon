#pragma once
#include "obj/Data.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "ui/UIComponent.h"
#include "utl/Str.h"

class PanelDir;

class UIPanel : public virtual Hmx::Object {
public:
    enum State {
        kUnloaded = 0,
        kUp = 1,
        kDown = 2,
    };

    UIPanel();
    // Hmx::Object
    virtual ~UIPanel() { Unload(); }
    OBJ_CLASSNAME(UIPanel);
    OBJ_SET_TYPE(UIPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Load(BinStream &bs) { Hmx::Object::Load(bs); }
    virtual void SetTypeDef(DataArray *);
    virtual class ObjectDir *DataDir();

    NEW_OBJ(UIPanel)

    // UIPanel
protected:
    virtual void Load();

public:
    virtual void Draw();
    virtual void Enter();
    virtual bool Entering() const;
    virtual void Exit();
    virtual bool Exiting() const;
    virtual bool Unloading() const;
    virtual void Poll();
    virtual void SetPaused(bool paused) { mPaused = paused; }
    virtual void FocusIn() {}
    virtual void FocusOut() {}
    virtual void Unload();
    virtual bool IsLoaded() const;

    bool Showing() const { return mShowing; }
    void SetShowing(bool b) { mShowing = b; }
    bool IsReferenced() const { return mLoadRefs != 0; }
    bool Paused() { return mPaused; }
    void CheckLoad();
    bool CheckIsLoaded();
    UIComponent *FocusComponent();
    void SetFocusComponent(UIComponent *);
    void SetLoadedDir(PanelDir *, bool);
    void UnsetLoadedDir();
    PanelDir *LoadedDir() { return mDir; }
    void CheckUnload();
    State GetState() const { return mState; }
    bool ForceExit() const { return mForceExit; }

    static bool GetFinalDrawPass() { return sIsFinalDrawPass; }
    static void SetFinalDrawPass(bool pass) { sIsFinalDrawPass = pass; }

protected:
    virtual void PollForLoading();
    virtual void FinishLoad();

private:
    static int sMaxPanelId;
    static bool sIsFinalDrawPass;

    DataNode OnLoad(DataArray *);

protected:
    // RB3-360 retail layout (verified vs the target binary, NOT dc3's). dc3 is
    // *newer* and added a trailing `mFinalDrawPassFlag` bool (with a Draw()-gate
    // using it) that RB3 predates — keeping it made every UIPanel-derived field
    // read +4 (CalibrationPanel::Exit, GamePanel, etc.). rb3-Wii UIPanel has no
    // such field. Offsets below cross-checked against the binary: UIPanel::Exit
    // sets mState (int) at +0x20 and tests mLoaded (bool) at +0x1c; with String
    // = 0xc bytes (mFocusName 0x10..0x1b) the bool/int order packs as below and
    // CalibrationPanel's first own field (mStream) lands at the correct 0x40.
    PanelDir *mDir; // 0x8
    DirLoader *mLoader; // 0xc
    String mFocusName; // 0x10 (0xc bytes -> ends 0x1c)
    bool mLoaded; // 0x1c
    /** The panel's current state. */
    State mState; // 0x20
    bool mPaused; // 0x24
    bool mShowing; // 0x25
    bool mForceExit; // 0x26
    /** The number of refs to this loaded UIPanel. */
    int mLoadRefs; // 0x28
    FilePath mFilePath; // 0x2c (0xc bytes -> ends 0x38)
    /** This panel's ID. */
    int mPanelId; // 0x38
};
