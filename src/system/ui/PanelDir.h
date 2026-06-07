#pragma once
#include "obj/Object.h"
#include "rndobj/Cam.h"
#include "rndobj/Dir.h"
#include "ui/UIComponent.h"
#include "ui/UIPanel.h"
#include "utl/MemMgr.h"

class Flow;
class UITrigger;

class PanelDir : public RndDir {
public:
    enum RequestFocus {
        kNoFocus = 0,
        kMaybeFocus = 1,
        kAlwaysFocus = 2,
    };
    PanelDir();
    // Hmx::Object
    virtual ~PanelDir();
    OBJ_CLASSNAME(PanelDir)
    OBJ_SET_TYPE(PanelDir)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    // ObjectDir
    virtual void SyncObjects();
    virtual void RemovingObject(Hmx::Object *);
    virtual bool Entering() const;
    virtual bool Exiting() const;
    virtual UIComponent *FocusComponent();
    virtual UIComponent *FindComponent(const char *);
    virtual void SetFocusComponent(UIComponent *, Symbol);
    // RndDrawable
    virtual RndCam *CamOverride();
    virtual void DrawShowing();
    // RndPollable
    virtual void Enter();
    virtual void Exit();

    OBJ_MEM_OVERLOAD(0x19);
    NEW_OBJ(PanelDir)

    void SetOwnerPanel(UIPanel *panel) { mOwnerPanel = panel; }
    UIPanel *OwnerPanel() { return mOwnerPanel; }
    RndCam *Cam() const { return mCam; }
    void SetCam(RndCam *cam) { mCam = cam; }
    void EnableComponent(UIComponent *, PanelDir::RequestFocus);
    void DisableComponent(UIComponent *, JoypadAction);
    DataNode GetFocusableComponentList();
    void SetShowFocusComponent(bool show);
    void UpdateFocusComponentState();

private:
    UIComponent *GetFirstFocusableComponent();
    UIComponent *ComponentNav(UIComponent *, JoypadAction, JoypadButton, Symbol);
    DataNode OnEnableComponent(DataArray const *);
    bool PanelNav(JoypadAction, JoypadButton, Symbol);
    DataNode OnMsg(ButtonDownMsg const &);
    DataNode OnDisableComponent(DataArray const *);
    void SyncEditModePanels();
    bool
    PropSyncEditModePanels(std::vector<FilePath> &, DataNode &, DataArray *, int, PropOp);

    static bool sAlwaysNeedFocus;

protected:
    void SendTransition(Message const &, Symbol, Symbol);

    // Offsets below are TRUE retail X360 offsets, reconstructed from target asm
    // (e.g. PanelDir::RemovingObject: mTriggers@0x1f0, mComponents@0x1f8,
    // mFocusComponent@0x1dc). The old `// 0xHEX` comments were stale Wii values.
    /** The currently focused-on component. */
    UIComponent *mFocusComponent; // 0x1dc
    class UIPanel *mOwnerPanel; // 0x1e0
    /** "Camera to use in game, else standard UI cam" */
    ObjPtr<RndCam> mCam; // 0x1e4
    /** The list of UITriggers within this PanelDir. */
    std::list<UITrigger *> mTriggers; // 0x1f0
    /** The list of UIComponents within this PanelDir. */
    std::list<UIComponent *> mComponents; // 0x1f8
    // NOTE: DC3's PanelDir adds `std::list<Flow*> mFlows;` between mTriggers and
    // mComponents; RB3 retail lacks it (target asm shows mTriggers@0x1f0 and
    // mComponents@0x1f8 — adjacent 8-byte lists, no gap — and the rb3-Wii oracle
    // has no mFlows anywhere). Removed to shrink PanelDir by 8 so every
    // PanelDir-derived member lands at its retail offset. (+4 TrackPanelDir fns)
    /** "Trigger postprocs before drawing this panel.
     * If checked, this panel will not be affected by the postprocs." */
    bool mCanEndWorld; // 0x200
    /** "Forces the usage of the 'cam' property to render in milo. This is a milo only
     * feature." */
    bool mUseSpecifiedCam; // 0x201
    /** "Additional panels to display behind this panel." */
    std::vector<RndDir *> mBackPanels; // 0x204
    /** The file paths of the aforementioned back panels. */
    std::vector<FilePath> mBackFilenames; // 0x210
    /** "Additional panels to display in front of this panel." */
    std::vector<RndDir *> mFrontPanels; // 0x21c
    /** The file paths of the aforementioned front panels. */
    std::vector<FilePath> mFrontFilenames; // 0x228
    /** "Whether or no this panel displays its view only panels" */
    bool mShowEditModePanels; // 0x234
    /** Whether or not to show the currently focused component. */
    bool mShowFocusComponent; // 0x235
};
