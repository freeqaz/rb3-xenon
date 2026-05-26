#include "ui/PanelDir.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Cam.h"
#include "rndobj/Dir.h"
#include "rndobj/EventTrigger.h"
#include "ui/UI.h"
#include "ui/UIComponent.h"
#include "ui/UIPanel.h"
#include "ui/UITrigger.h"
#include "ui/Utl.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#ifdef HX_NATIVE
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include "flow/Flow.h"
extern void FlushTransparentDraws();
extern void FlushPostProcessingForOverlay();
#endif

#ifdef HX_NATIVE
bool PanelDir::sAlwaysNeedFocus;
#endif

bool gSendFocusMsg = true;

#ifdef HX_NATIVE
namespace {
enum NativeFlowFilterMode {
    kNativeFlowFilterAll = 0,
    kNativeFlowFilterCurated = 1,
    kNativeFlowFilterMenuOnly = 2,
};

bool DebugPanelFlowNames(const char *dirName) {
    static int enabled = -1;
    if (enabled == -1) {
        const char *env = std::getenv("MILO_DEBUG_PANEL_FLOWS");
        enabled = (env && env[0] && std::strcmp(env, "0") != 0) ? 1 : 0;
    }
    if (!enabled || !dirName) {
        return false;
    }
    return std::strcmp(dirName, "main") == 0 || std::strcmp(dirName, "helpbar") == 0
        || std::strcmp(dirName, "letterbox") == 0 || std::strcmp(dirName, "background") == 0;
}

NativeFlowFilterMode GetNativeFlowFilterMode() {
    static int mode = -1;
    if (mode == -1) {
        const char *env = std::getenv("MILO_NATIVE_FLOW_FILTER");
        if (!env || !env[0] || std::strcmp(env, "curated") == 0 || std::strcmp(env, "1") == 0) {
            mode = kNativeFlowFilterCurated;
        } else if (
            std::strcmp(env, "all") == 0 || std::strcmp(env, "0") == 0
        ) {
            mode = kNativeFlowFilterAll;
        } else if (
            std::strcmp(env, "menu_only") == 0 || std::strcmp(env, "main_only") == 0
        ) {
            mode = kNativeFlowFilterMenuOnly;
        } else {
            mode = kNativeFlowFilterCurated;
        }
    }
    return (NativeFlowFilterMode)mode;
}

std::string LowerString(const char *str) {
    std::string lowered;
    if (!str) {
        return lowered;
    }
    lowered.reserve(std::strlen(str));
    while (*str) {
        lowered.push_back((char)std::tolower((unsigned char)*str));
        ++str;
    }
    return lowered;
}

bool ContainsAny(const std::string &text, const char *const *tokens) {
    for (const char *const *token = tokens; *token; ++token) {
        if (text.find(*token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool ShouldActivateNativeFlow(const char *dirName, const char *flowPath) {
    NativeFlowFilterMode mode = GetNativeFlowFilterMode();
    if (mode == kNativeFlowFilterAll) {
        return true;
    }

    std::string flow = LowerString(flowPath);
    std::string dir = LowerString(dirName);

    // Dirs whose flows are entirely game-code-triggered (EnterControllerMode,
    // ShowWaveGestureIcon, etc.).  Auto-activating them causes conflicting
    // show/hide animations that fight over transform positions.
    static const char *kGameTriggeredDirs[] = {
        "helpbar", "blacklight", "autosaving_icon", nullptr,
    };
    for (const char **d = kGameTriggeredDirs; *d; ++d) {
        if (dir == *d) return false;
    }

    if (mode == kNativeFlowFilterMenuOnly) {
        if (dir == "letterbox" || dir == "newskeletondir") {
            return false;
        }
    }

    static const char *kSkipTokens[] = {
        "hide",
        "exit",
        "deactivate",
        "immediate",
        "end_",
        "heartbeat_stop",
        nullptr,
    };
    static const char *kKeepTokens[] = {
        "enter",
        "show",
        "select",
        "highlight",
        "activate",
        "start_",
        "update_",
        "udpate_",
        "overlay_colorswitch",
        nullptr,
    };

    if (ContainsAny(flow, kSkipTokens)) {
        return false;
    }
    if (ContainsAny(flow, kKeepTokens)) {
        return true;
    }

    // Keep the current broad behavior outside the known-problem UI dirs.
    if (!dirName || dir.empty()) {
        return true;
    }
    // Allow main/background flows — they contain positioning PropAnims
    // driven by DTA enter scripts that don't run on native.
    return dir != "letterbox";
}
}
#endif

PanelDir::PanelDir()
    : mFocusComponent(nullptr), mOwnerPanel(nullptr), mCam(this), mCanEndWorld(true),
      mUseSpecifiedCam(false), mShowEditModePanels(false), mShowFocusComponent(true) {
    if (TheLoadMgr.EditMode()) {
        mShowEditModePanels = true;
    }
}

PanelDir::~PanelDir() {
    FOREACH (it, mBackPanels) {
        RELEASE(*it);
    }
    FOREACH (it, mFrontPanels) {
        RELEASE(*it);
    }
}

BEGIN_HANDLERS(PanelDir)
    HANDLE(enable, OnEnableComponent)
    HANDLE(disable, OnDisableComponent)
    HANDLE_ACTION(set_focus, SetFocusComponent(_msg->Obj<UIComponent>(2), gNullStr))
    HANDLE_EXPR(focus_name, mFocusComponent ? mFocusComponent->Name() : "")
    HANDLE_EXPR(get_focusable_components, GetFocusableComponentList())
    HANDLE_ACTION(set_show_focus_component, SetShowFocusComponent(_msg->Int(2)))
    HANDLE_SUPERCLASS(RndDir)
    HANDLE_MESSAGE(ButtonDownMsg)
    if (sym != "button_down") {
        HANDLE_MEMBER_PTR(mFocusComponent)
    }
    HANDLE_EXPR(loaded_dir, this)
END_HANDLERS

BEGIN_PROPSYNCS(PanelDir)
    SYNC_PROP(cam, mCam)
    SYNC_PROP(postprocs_before_draw, mCanEndWorld)
    SYNC_PROP(use_specified_cam, mUseSpecifiedCam)
    SYNC_PROP(focus_component, mFocusComponent)
    SYNC_PROP(owner_panel, mOwnerPanel) {
        static Symbol _s("front_view_only_panels");
        if (sym == _s) {
            PropSyncEditModePanels(mFrontFilenames, _val, _prop, _i + 1, _op);
            return true;
        }
    }
    {
        static Symbol _s("back_view_only_panels");
        if (sym == _s) {
            PropSyncEditModePanels(mBackFilenames, _val, _prop, _i + 1, _op);
            return true;
        }
    }
    SYNC_PROP_MODIFY(show_view_only_panels, mShowEditModePanels, SyncEditModePanels())
    SYNC_SUPERCLASS(RndDir)
END_PROPSYNCS

BEGIN_SAVES(PanelDir)
    SAVE_REVS(8, 0)
    SAVE_SUPERCLASS(RndDir)
    if (!IsProxy()) {
        bs << mCam;
    }
    bs << mCanEndWorld;
    bs << mBackFilenames << mFrontFilenames << mShowEditModePanels;
    bs << mUseSpecifiedCam;
END_SAVES

BEGIN_COPYS(PanelDir)
    COPY_SUPERCLASS(RndDir)
    CREATE_COPY(PanelDir)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mCam)
        COPY_MEMBER(mCanEndWorld)
        COPY_MEMBER(mBackFilenames)
        COPY_MEMBER(mFrontFilenames)
        COPY_MEMBER(mShowEditModePanels)
        COPY_MEMBER(mUseSpecifiedCam)
        SyncEditModePanels();
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(8, 0)

void PanelDir::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(8, 0)
    RndDir::PreLoad(d.stream);
    d.PushRev(this);
}

void PanelDir::PostLoad(BinStream &bs) {
    BinStreamRev d(bs, bs.PopRev(this));
    RndDir::PostLoad(d.stream);
    if (!IsProxy()) {
        if (d.rev > 0) {
            d >> mCam;
        }
        if (d.rev > 1 && d.rev < 3) {
            Symbol s;
            d >> s;
        }
    }
    if (d.rev < 7 && !mCam) {
        SetCurViewport(kNumViewports, TheUI->GetCam());
    }
    if (d.rev > 3) {
        d >> mCanEndWorld;
    }
    if (d.rev > 4) {
        d >> mBackFilenames >> mFrontFilenames;
    }
    if (d.rev > 5) {
        d >> mShowEditModePanels;
    }
    if (d.rev > 7) {
        if (gLoadingProxyFromDisk) {
            bool b;
            d >> b;
        } else {
            d >> mUseSpecifiedCam;
        }
    }
    SyncEditModePanels();
}

void PanelDir::SyncObjects() {
    RndDir::SyncObjects();
    mComponents.clear();
    for (ObjDirItr<UIComponent> it(this, true); it != nullptr; ++it) {
        mComponents.push_back(it);
    }
    mTriggers.clear();
    for (ObjDirItr<UITrigger> it(this, true); it != nullptr; ++it) {
        mTriggers.push_back(it);
        it->CheckAnims();
    }
    if (sAlwaysNeedFocus) {
        UIComponent *comp = GetFirstFocusableComponent();
        if (!mFocusComponent && comp) {
            gSendFocusMsg = false;
            SetFocusComponent(comp, gNullStr);
            gSendFocusMsg = true;
        }
    }
}

void PanelDir::RemovingObject(Hmx::Object *o) {
    ObjMatchPr pr(o);
    mComponents.remove_if(pr);
    mTriggers.remove_if(pr);
    if (sAlwaysNeedFocus) {
        if (mFocusComponent == o) {
            mFocusComponent = nullptr;
            UIComponent *focus = GetFirstFocusableComponent();
            if (focus) {
                SetFocusComponent(focus, gNullStr);
            }
        }
    }
    RndDir::RemovingObject(o);
}

bool PanelDir::Entering() const {
    FOREACH (it, mComponents) {
        if ((*it)->Entering())
            return true;
    }
    FOREACH (it, mTriggers) {
        if ((*it)->IsBlocking())
            return true;
    }
    return false;
}

bool PanelDir::Exiting() const {
    FOREACH (it, mComponents) {
        if ((*it)->Exiting())
            return true;
    }
    FOREACH (it, mTriggers) {
        if ((*it)->IsBlocking())
            return true;
    }
    return false;
}

UIComponent *PanelDir::FocusComponent() { return mFocusComponent; }

UIComponent *PanelDir::FindComponent(const char *name) {
    return Find<UIComponent>(name, false);
}

void PanelDir::SetFocusComponent(UIComponent *newComponent, Symbol nav_type) {
    if (newComponent && !newComponent->CanHaveFocus())
        MILO_NOTIFY(
            "Trying to set focus on a component that can't have focus.  Component: %s",
            newComponent->Name()
        );
    else if (newComponent == mFocusComponent) {
        if (mFocusComponent) {
            mFocusComponent->SetState(UIComponent::kFocused);
        }
    } else {
        UIComponent *focused = FocusComponent();
        if (mFocusComponent && mFocusComponent->GetState() != UIComponent::kDisabled) {
            mFocusComponent->SetState(UIComponent::kNormal);
        }
        mFocusComponent = newComponent;
        UpdateFocusComponentState();
        if (gSendFocusMsg) {
            UIComponentFocusChangeMsg msg(newComponent, focused, this, nav_type);
            TheUI->Handle(msg, false);
        }
    }
}

RndCam *PanelDir::CamOverride() {
    if (TheLoadMgr.EditMode() && !mUseSpecifiedCam)
        return nullptr;
    if (mCam)
        return mCam;
    return TheUI->GetCam();
}

void PanelDir::DrawShowing() {
    if (mCanEndWorld) {
        TheRnd.EndWorld();
#ifdef HX_NATIVE
        FlushPostProcessingForOverlay();
#endif
    }
    RndCam *curCam = RndCam::Current();
    RndCam *camOverride = CamOverride();
    if (camOverride && camOverride != RndCam::Current()) {
#ifdef HX_NATIVE
        FlushTransparentDraws();
#endif
        camOverride->Select();
    }
    if (!mEnv) {
        RndEnviron *curEnv = TheUI->GetEnv();
        if (curEnv != RndEnviron::Current()) {
            curEnv->Select(nullptr);
        }
    }
    FOREACH (it, mBackPanels) {
        if (*it)
            (*it)->DrawShowing();
    }
    RndDir::DrawShowing();
    FOREACH (it, mFrontPanels) {
        if (*it)
            (*it)->DrawShowing();
    }
    if (curCam && curCam != RndCam::Current()) {
#ifdef HX_NATIVE
        FlushTransparentDraws();
#endif
        curCam->Select();
    }
}

void PanelDir::Enter() {
    RndDir::Enter();
    FOREACH (it, mTriggers) {
        (*it)->Enter();
    }
    static Message ui_enter("ui_enter");
    static Symbol ui_enter_forward("ui_enter_forward");
    static Symbol ui_enter_back("ui_enter_back");
    SendTransition(ui_enter, ui_enter_forward, ui_enter_back);
#ifdef HX_NATIVE
    // Activate game-code-triggered Flows (startMode==0) that normally fire from
    // DTA enter scripts on Xbox. Flows with startMode>0 auto-start through the
    // normal Flow::Enter() path (called by RndDir::Enter above) and don't need
    // blanket activation here.
    for (ObjDirItr<Flow> it(this, true); it != nullptr; ++it) {
        if (it->GetStartMode() > 0) {
            // Event-triggered flows with "enter" in the name need explicit
            // activation on native — the DTA enter script message that would
            // normally trigger them may not reach the flow node.
            std::string name = LowerString(it->Name());
            if (name.find("enter") == std::string::npos)
                continue;
        }
        const char *flowPath = PathName((Hmx::Object *)it);
        if (!ShouldActivateNativeFlow(Name(), flowPath))
            continue;
        if (!it->IsRunning())
            it->Activate();
    }
#endif
}

void PanelDir::Exit() {
    RndDir::Exit();
    static Message msg("ui_exit");
    static Symbol ui_exit_forward("ui_exit_forward");
    static Symbol ui_exit_back("ui_exit_back");
    SendTransition(msg, ui_exit_forward, ui_exit_back);
}

UIComponent *PanelDir::GetFirstFocusableComponent() {
    UIComponent *ret = nullptr;
    FOREACH (it, mComponents) {
        UIComponent *component = *it;
        MILO_ASSERT(component, 0x214);
        if (component->CanHaveFocus()) {
            ret = component;
            break;
        }
    }
    return ret;
}

UIComponent *PanelDir::ComponentNav(
    UIComponent *comp, JoypadAction act, JoypadButton btn, Symbol controller_type
) {
    UIComponent *compIt = nullptr;
    bool overloaded =
        TheUI->OverloadHorizontalNav(act, btn, JoypadTypeHasLeftyFlip(controller_type));
    if (act == kAction_Down)
        compIt = comp->NavDown();
    if (!compIt && (act == kAction_Right || (overloaded && act == kAction_Down))) {
        compIt = comp->NavRight();
    }
    if (!compIt && act == kAction_Up) {
        FOREACH (it, mComponents) {
            if ((*it)->NavDown() == comp) {
                compIt = *it;
                break;
            }
        }
    }
    if (!compIt && (act == kAction_Left || (overloaded && act == kAction_Up))) {
        FOREACH (it, mComponents) {
            if ((*it)->NavRight() == comp) {
                compIt = *it;
                break;
            }
        }
    }
    return compIt;
}

void PanelDir::EnableComponent(UIComponent *c, PanelDir::RequestFocus focusable) {
    if (c->GetState() == UIComponent::kDisabled)
        c->SetState(UIComponent::kNormal);
    if (c->CanHaveFocus()
        && (focusable == kAlwaysFocus
            || (focusable == kMaybeFocus && !mFocusComponent))) {
        SetFocusComponent(c, gNullStr);
    }
}

DataNode PanelDir::OnEnableComponent(DataArray const *da) {
    UIComponent *c = da->Obj<UIComponent>(2);
    if (da->Size() == 4) {
        EnableComponent(c, (RequestFocus)da->Int(3));
    } else if (da->Size() == 3) {
        EnableComponent(c, kNoFocus);
    } else
        MILO_NOTIFY("wrong number of args to PanelDir enable");
    return 0;
}

void PanelDir::SendTransition(Message const &msg, Symbol forward, Symbol back) {
    static Message dirMsg = Message("");
    dirMsg.SetType(TheUI->WentBack() ? back : forward);
    RndDir::Handle(msg, false);
    RndDir::Handle(dirMsg, false);
}

bool PanelDir::PanelNav(JoypadAction act, JoypadButton btn, Symbol controller_type) {
    UIComponent *comp = mFocusComponent;
    if (!comp) {
        goto fail;
    }
    do {
        comp = ComponentNav(comp, act, btn, controller_type);
        if (!comp)
            return false;
        if (comp == mFocusComponent)
            goto fail;
        if (comp->GetState() == UIComponent::kDisabled) {
            continue;
        }
        static Symbol none("none");
        if (controller_type != none) {
            static Symbol panelNavigated("panel_navigated");
            static Message panelNavigatedMsg(panelNavigated);
            TheUI->Handle(panelNavigatedMsg, false);
        }
        SetFocusComponent(comp, controller_type);
        return true;
    } while (true);
fail:
    return false;
}

DataNode PanelDir::OnMsg(ButtonDownMsg const &msg) {
    DataNode node(kDataUnhandled, 0);
    if (mFocusComponent) {
        node = mFocusComponent->Handle(msg, false);
    }
    if (node.Type() == kDataUnhandled) {
        if (PanelNav(
                msg.GetAction(),
                msg.GetButton(),
                JoypadControllerTypePadNum(msg.GetPadNum())
            )) {
            return 0;
        }
    }
    return node;
}

void PanelDir::DisableComponent(UIComponent *c, JoypadAction nav_action) {
    MILO_ASSERT(nav_action == kAction_None || IsNavAction(nav_action), 0x1C8);
    static Symbol none("none");
    if (c == mFocusComponent) {
        if (nav_action == kAction_None) {
            PanelNav(kAction_Down, kPad_NumButtons, none);
            if (c == mFocusComponent) {
                PanelNav(kAction_Up, kPad_NumButtons, none);
            }
        } else
            PanelNav(nav_action, kPad_NumButtons, none);
    }
    if (c == mFocusComponent)
        mFocusComponent = nullptr;
    c->SetState(UIComponent::kDisabled);
}

DataNode PanelDir::OnDisableComponent(const DataArray *da) {
    UIComponent *c = da->Obj<UIComponent>(2);
    if (da->Size() == 4) {
        DisableComponent(c, (JoypadAction)da->Int(3));
    } else if (da->Size() == 3) {
        DisableComponent(c, kAction_None);
    } else
        MILO_NOTIFY("wrong number of args to PanelDir disable");
    return 0;
}

DataNode PanelDir::GetFocusableComponentList() {
    std::vector<UIComponent *> components;
    FOREACH (it, mComponents) {
        UIComponent *component = *it;
        MILO_ASSERT(component, 0x1f4);
        if (component->CanHaveFocus()) {
            components.push_back(component);
        }
    }
    DataArrayPtr ptr(new DataArray(components.size()));
    int i = 0;
    std::vector<UIComponent *>::iterator it = components.begin();
    for (; it != components.end(); ++it, ++i) {
        ptr->Node(i) = *it;
    }
    return ptr;
}

void PanelDir::SyncEditModePanels() {
    if (TheLoadMgr.EditMode()) {
        FOREACH (it, mBackPanels) {
            RELEASE(*it);
        }
        FOREACH (it, mFrontPanels) {
            RELEASE(*it);
        }
        if (mShowEditModePanels) {
            FOREACH (it, mBackFilenames) {
                FilePath fp3c(*it);
                if (fp3c.length() != 0) {
                    RndDir *curDir =
                        dynamic_cast<RndDir *>(DirLoader::LoadObjects(fp3c, 0, 0));
                    if (curDir) {
                        mBackPanels.push_back(curDir);
                        curDir->Enter();
                    }
                }
            }
            FOREACH (it, mFrontFilenames) {
                FilePath fp48(*it);
                if (fp48.length() != 0) {
                    RndDir *curDir =
                        dynamic_cast<RndDir *>(DirLoader::LoadObjects(fp48, 0, 0));
                    if (curDir) {
                        mFrontPanels.push_back(curDir);
                        curDir->Enter();
                    }
                }
            }
        }
    }
}

bool PanelDir::PropSyncEditModePanels(
    std::vector<FilePath> &paths, DataNode &val, DataArray *prop, int i, PropOp op
) {
    if (op == kPropSize) {
        MILO_ASSERT(i == prop->Size(), 0x2c6);
        val = (int)paths.size();
        return true;
    } else {
        MILO_ASSERT(i == prop->Size() - 1, 0x2cb);
        std::vector<FilePath>::iterator it = paths.begin() + prop->Int(i);
        switch (op) {
        case kPropGet:
            val = *it;
            break;
        case kPropSet:
            it->SetRoot(val.Str());
            SyncEditModePanels();
            break;
        case kPropRemove:
            paths.erase(it);
            SyncEditModePanels();
            break;
        case kPropInsert:
            paths.insert(it, val.Str());
            SyncEditModePanels();
            break;
        default:
            return false;
        }
        return true;
    }
}

void PanelDir::SetShowFocusComponent(bool b) {
    mShowFocusComponent = b;
    UpdateFocusComponentState();
}

void PanelDir::UpdateFocusComponentState() {
    if (!mFocusComponent)
        return;
    if (mShowFocusComponent)
        mFocusComponent->SetState(UIComponent::kFocused);
    else
        mFocusComponent->SetState(UIComponent::kNormal);
}
