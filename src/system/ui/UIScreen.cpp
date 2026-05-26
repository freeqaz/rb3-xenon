#include "ui/UIScreen.h"
#include "gesture/GestureMgr.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Archive.h"
#include "os/Debug.h"
#include "os/JoypadMsgs.h"
#include "os/Timer.h"
#include "rndobj/Rnd.h"
#include "ui/UI.h"
#include "ui/UILabel.h"
#include "ui/UIPanel.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "utl/TextStream.h"

UIScreen *UIScreen::sUnloadingScreen = nullptr;
#ifdef HX_NATIVE
int UIScreen::sMaxScreenId;
#endif

#ifdef HX_NATIVE
static inline bool DebugUIFlow() {
    static bool checked = false;
    static bool val = false;
    if (!checked) {
        val = std::getenv("MILO_DEBUG_UI_FLOW") != nullptr;
        checked = true;
    }
    return val;
}
#endif

void EnterGlitchCB(float ms, void *panel) {
    UIPanel *uiPanel = static_cast<UIPanel *>(panel);
    MILO_LOG("%s %s Enter took %.2f ms\n", uiPanel->ClassName(), uiPanel->Name(), ms);
}

void UnloadGlitchCB(float ms, void *panel) {
    UIPanel *uiPanel = static_cast<UIPanel *>(panel);
    MILO_LOG(
        "%s %s CheckUnload took %.2f ms\n", uiPanel->ClassName(), uiPanel->Name(), ms
    );
}

UIScreen::UIScreen()
    : mFocusPanel(nullptr), mBack(nullptr), mClearVram(false), mShowing(true),
      mScreenId(sMaxScreenId++) {
    MILO_ASSERT(sMaxScreenId < 0x8000, 0x20);
}

BEGIN_HANDLERS(UIScreen)
    HANDLE_EXPR(focus_panel, mFocusPanel)
    HANDLE_ACTION(set_focus_panel, SetFocusPanel(_msg->Obj<class UIPanel>(2)))
    HANDLE_ACTION(print, Print(TheDebug))
    HANDLE_ACTION(reenter_screen, ReenterScreen())
    HANDLE_ACTION(
        set_panel_active, SetPanelActive(_msg->Obj<class UIPanel>(2), _msg->Int(3))
    )
    HANDLE_ACTION(set_showing, SetShowing(_msg->Int(2)))
    HANDLE_EXPR(has_panel, HasPanel(_msg->Obj<class UIPanel>(2)))
    HANDLE_ACTION(foreach_panel, ForeachPanel(_msg))
    HANDLE_EXPR(exiting, Exiting())
    HANDLE_ACTION(reload_strings, ReloadStrings())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_MEMBER_PTR(FocusPanel())
    HANDLE_MESSAGE(ButtonDownMsg)
END_HANDLERS

void UIScreen::SetTypeDef(DataArray *data) {
    Hmx::Object::SetTypeDef(data);
    mFocusPanel = nullptr;
    mPanelList.clear();
    static Symbol panels("panels");
    DataArray *panelsArr = data->FindArray(panels, false);
    if (panelsArr) {
        for (int i = 1; i < panelsArr->Size(); i++) {
            PanelRef pr;
            if (panelsArr->Type(i) == kDataArray) {
                static Symbol active("active");
                static Symbol always_load("always_load");
                DataArray *panelArray = panelsArr->Array(i);
                pr.mPanel = panelArray->Obj<UIPanel>(0);
                MILO_ASSERT(pr.mPanel, 0x3a);
                panelArray->FindData(active, pr.mActive, false);
                panelArray->FindData(always_load, pr.mAlwaysLoad, false);
            } else {
                pr.mPanel = panelsArr->Obj<UIPanel>(i);
                MILO_ASSERT(pr.mPanel, 0x41);
            }
#ifdef HX_NATIVE
            if (!pr.mPanel) {
                const char *panelName = (panelsArr->Type(i) == kDataSymbol) ? panelsArr->Sym(i).Str() : "?";
                fprintf(stderr, "DC3 Native: UIScreen '%s' panel[%d] '%s' not found — skipping\n", Name(), i, panelName);
                continue;
            }
            fprintf(stderr, "DC3 Native: UIScreen '%s' panel[%d] = '%s' (state=%d)\n",
                    Name(), i, pr.mPanel->Name(), (int)pr.mPanel->GetState());
#endif
            mPanelList.push_back(pr);
        }
    }
    static Symbol focus("focus");
    DataArray *focusArr = data->FindArray(focus, false);
    if (focusArr) {
        SetFocusPanel(focusArr->Obj<UIPanel>(1));
    }
    if (!mFocusPanel && !mPanelList.empty()) {
#ifdef HX_NATIVE
        if (focusArr) {
            // Focus panel specified in DTA but Obj<UIPanel> lookup failed
            const char *focusName = (focusArr->Size() > 1 && focusArr->Type(1) == kDataSymbol)
                ? focusArr->Str(1) : "?";
            MILO_WARN(
                "UIScreen '%s': focus panel '%s' not found, falling back to '%s'\n",
                Name(), focusName, mPanelList.front().mPanel->Name()
            );
        }
#endif
        SetFocusPanel(mPanelList.front().mPanel);
    }

    mBack = data->FindArray("back", false);
    static Symbol clear_vram("clear_vram");
    mClearVram = false;
    data->FindData(clear_vram, mClearVram, false);
}

void UIScreen::LoadPanels() {
    if (Archive::DebugArkOrder())
        MILO_LOG("ArkFile: ;%s\n", Name());

    FOREACH (it, mPanelList) {
        if (it->mAlwaysLoad || it->mPanel->IsReferenced()) {
            it->mPanel->CheckLoad();
            it->mLoaded = true;
        } else {
            it->mLoaded = false;
        }
    }
    static Message msg("load_panels");
    HandleType(msg);
}

void UIScreen::UnloadPanels() {
#ifdef HX_NATIVE
    // Clear UI animation tasks before panel destruction. On Xbox, ~Object's
    // ReplaceRefs triggers AnimTask::Replace → QueueTaskDelete for each dying
    // object. On native, cascade skips ReplaceRefs (ring corruption), so tasks
    // holding refs to panel objects survive with stale pointers → use-after-free.
    //
    // Only clear seconds/UI timelines — preserve beat-synced and tutorial tasks
    // which may be driven by audio/music systems independent of panels.
    // Safe because this runs BEFORE the new screen's Enter() creates tasks.
    TheTaskMgr.ClearTimelineTasks(kTaskSeconds);
    TheTaskMgr.ClearTimelineTasks(kTaskUISeconds);
    // Suppress FlushDeferredFrees between panel cascades. Without this,
    // panel A's cascade frees memory, then panel B's NullifyAllRefs walks
    // ring nodes in that freed memory → heap corruption. Deferring the
    // flush keeps all memory valid until every panel is destroyed.
    ObjectDir::BeginBatchDelete();
#endif
    FOREACH_REVERSE(it, mPanelList) {
        if (it->mLoaded) {
            AutoGlitchReport report(17.0f, UnloadGlitchCB, it->mPanel);
            it->mPanel->CheckUnload();
        }
    }
#ifdef HX_NATIVE
    ObjectDir::EndBatchDelete();
#endif
}

bool UIScreen::CheckIsLoaded() {
    FOREACH (it, mPanelList) {
        if (it->Active() && !it->mPanel->CheckIsLoaded()) {
#ifdef HX_NATIVE
            if (DebugUIFlow()) {
                printf("DC3 UI: Screen '%s' not loaded — panel '%s' (state=%d) blocking\n",
                       Name(), it->mPanel->Name(), (int)it->mPanel->GetState());
            }
#endif
            return false;
        }
    }

    return true;
}

bool UIScreen::IsLoaded() const {
    FOREACH (it, mPanelList) {
        if (it->Active() && it->mPanel->GetState() == UIPanel::kUnloaded) {
            return false;
        }
    }

    // please don't tell me const_cast is what they did lol
    static Message is_loaded("is_loaded");
    DataNode result = const_cast<UIScreen *>(this)->HandleType(is_loaded);
    if (result.Type() != kDataUnhandled) {
        return result.Int();
    }

    return true;
}

void UIScreen::Poll() {
    static Message msg("poll_msg");
    HandleType(msg);

    FOREACH (it, mPanelList) {
        if (it->Active() && !it->mPanel->Paused()) {
            it->mPanel->Poll();
        }
    }
}

void UIScreen::Draw() {
    if (mShowing) {
        FOREACH (it, mPanelList) {
            if (it->Active() && it->mPanel->Showing()
                && TheRnd.ShouldDrawPanel(it->mPanel)) {
                static Symbol suppress_blacklight_text("suppress_blacklight_text");
                const DataNode *prop = Property(suppress_blacklight_text, false);
                TheUI->SetScreenBlacklghtDisabled(prop && prop->Int() != 0);
                it->mPanel->Draw();
            }
        }
    }
}

bool UIScreen::InComponentSelect() const {
    UIComponent *component = TheUI->FocusComponent();
    if (component) {
        return component->GetState() == UIComponent::kSelecting;
    }

    return false;
}

void UIScreen::Enter(UIScreen *scr) {
#ifdef HX_NATIVE
    printf("DC3 UI: Screen '%s' Enter (from '%s')\n", Name(), scr ? scr->Name() : "<null>");
#endif
    if (scr) {
        sUnloadingScreen = scr;
        scr->UnloadPanels();
    }
    Rnd::sPostProcPanelCount = 0;
    std::vector<const char *> vec;
    int i5 = 0;
    FOREACH (it, mPanelList) {
        if (it->Active() && it->mPanel->GetState() == UIPanel::kDown) {
#ifdef HX_NATIVE
            // Skip Kinect tutorial panels — no gesture input on native.
            // On Xbox, DTA scripts suppress these in controller mode.
            if (strstr(it->mPanel->Name(), "tutorial")) {
                continue;
            }
#endif
#ifdef HX_WEB
            fprintf(stderr, "DC3 Web: UIScreen '%s' entering panel '%s'...\n", Name(), it->mPanel->Name());
            fflush(stderr);
#endif
            AutoGlitchReport report(17, EnterGlitchCB, it->mPanel);
            it->mPanel->Enter();
#ifdef HX_WEB
            fprintf(stderr, "DC3 Web: UIScreen '%s' panel '%s' entered OK\n", Name(), it->mPanel->Name());
            fflush(stderr);
#endif
            if (Rnd::sPostProcPanelCount != i5) {
                vec.push_back(it->mPanel->Name());
                i5 = Rnd::sPostProcPanelCount;
            }
        }
    }
    if (Rnd::sPostProcPanelCount != 1) {
        if (Rnd::sPostProcPanelCount == 0) {
            MILO_LOG(
                "[POSTPROC WARNING] UIScreen '%s' doesn't have any panels that set the PostProc\n",
                Name()
            );
        } else {
            MILO_LOG(
                "[POSTPROC WARNING] UIScreen '%s' has %d panels that attempt to set the PostProc\n",
                Name(),
                Rnd::sPostProcPanelCount
            );
            for (int i = 0; i < Rnd::sPostProcPanelCount; i++) {
                MILO_LOG("[POSTPROC WARNING]    panel = '%s'\n", vec[i]);
            }
        }
        Rnd::sPostProcPanelCount = 0;
    }
#ifdef HX_WEB
    fprintf(stderr, "DC3 Web: UIScreen '%s' all panels entered, sending 'enter' msg...\n", Name());
    fflush(stderr);
#endif
    static Message msg("enter", 0);
    msg[0] = scr;
    HandleType(msg);
#ifdef HX_WEB
    fprintf(stderr, "DC3 Web: UIScreen '%s' 'enter' msg done\n", Name());
    fflush(stderr);
    // Web: skip the synchronous Poll() call here. On web, async file loading
    // (fetch) needs the browser event loop to progress, so calling Poll()
    // synchronously during Enter() causes a deadlock if panels are still loading.
    // The main loop will call Poll() on subsequent frames.
#else
    Poll();
#endif

#ifdef HX_NATIVE
    // Dump screen typeDef handlers for debugging
    {
        DataArray *td = TypeDef();
        if (td) {
            printf("DC3 Native: Screen '%s' typeDef:", Name());
            DataArray *nsArr = td->FindArray("next_screen", false);
            if (nsArr && nsArr->Size() > 1) {
                printf(" next_screen='%s'", nsArr->ForceSym(1).Str());
            }
            printf(" handlers:");
            for (int _i = 0; _i < td->Size(); _i++) {
                if (td->Type(_i) == kDataArray) {
                    DataArray *sub = td->Array(_i);
                    if (sub->Size() > 0 && sub->Type(0) == kDataSymbol) {
                        printf(" %s", sub->Sym(0).Str());
                    }
                }
            }
            printf("\n");
        }
    }
#endif
}

bool UIScreen::Entering() const {
    FOREACH (it, mPanelList) {
        if (it->Active() && it->mPanel->Entering()) {
            return true;
        }
    }

    if (sUnloadingScreen != nullptr && sUnloadingScreen->Unloading()) {
        return true;
    }

    sUnloadingScreen = nullptr;
    return false;
}

void UIScreen::Exit(UIScreen *to) {
#ifdef HX_NATIVE
    printf("DC3 UI: Screen '%s' Exit (to '%s')\n", Name(), to ? to->Name() : "<null>");
#endif
    TheGestureMgr->SetInVoiceMode(false);
    static Message msg("exit", 0);
    msg[0] = to;
    HandleType(msg);

    if (to != NULL) {
        to->LoadPanels();
    }

    FOREACH (it, mPanelList) {
        if (!it->mLoaded) {
            continue;
        }

        if ((it->mPanel->ForceExit() || to == NULL || !to->HasPanel(it->mPanel))
            && it->mPanel->GetState() == UIPanel::kUp) {
            it->mPanel->Exit();
        }
    }
}

bool UIScreen::Exiting() const {
#ifdef __EMSCRIPTEN__
    // Web: exit animations never complete (Flow/timer/movie subsystems not
    // fully functional). Skip all exit waits to prevent stuck transitions.
    return false;
#else
    FOREACH (it, mPanelList) {
        if (it->Active() && it->mPanel->Exiting()) {
#ifdef HX_NATIVE
            static int sExitDiag = 0;
            if (sExitDiag++ < 5) {
                printf("DC3 UI: Screen '%s' still exiting — panel '%s' (state=%d) blocking\n",
                       Name(), it->mPanel->Name(), (int)it->mPanel->GetState());
            }
#endif
            return true;
        }
    }

    return false;
#endif
}

void UIScreen::Print(TextStream &s) {
    static Symbol file("file");
    s << "{UIScreen " << Name() << "\n";
    if (mPanelList.size() != 0) {
        s << "   Panels:\n";
        FOREACH (it, mPanelList) {
            s << "      " << it->mPanel->Name() << " ";
            bool a = it->mActive;
            if (!a) {
                s << "(active " << a << ") ";
            }
            a = it->mAlwaysLoad;
            if (!a) {
                s << "(always_load " << a << ") ";
            }
            const DataArray *typeDef = it->mPanel->TypeDef();
            if (typeDef) {
                DataArray *fileArray = typeDef->FindArray(file, false);
                if (fileArray) {
                    DataNode type = fileArray->Node(1);
                    if (type.Type() == kDataString || type.Type() == kDataSymbol) {
                        s << "(" << type.LiteralStr() << ") ";
                    } else {
                        s << "(dynamic) ";
                    }
                }
            } else {
                s << " ";
            }

            if (it->mPanel == mFocusPanel) {
                s << "(focus)";
            }

            s << "\n";
        }
    }

    s << "}\n";
}

bool UIScreen::Unloading() const {
    FOREACH (it, mPanelList) {
        if (it->mLoaded && it->mPanel->Unloading()) {
            return true;
        }
    }

    return false;
}

void UIScreen::SetFocusPanel(UIPanel *panel) {
    if (panel == mFocusPanel)
        return;

    if (mFocusPanel != nullptr)
        mFocusPanel->FocusIn();

    mFocusPanel = panel;

    if (mFocusPanel != nullptr)
        mFocusPanel->FocusOut();
}

void UIScreen::SetShowing(bool show) { mShowing = show; }

bool UIScreen::HasPanel(UIPanel *panel) {
    FOREACH (it, mPanelList) {
        if (it->mPanel == panel && it->mActive) {
            return true;
        }
    }

    return false;
}

// Exits all active panels, then re-enters them.
// Used to reset panel state without fully unloading.
void UIScreen::ReenterScreen() {
    AutoGlitchReport hang(50.0f, "UIScreen::ReenterScreen");

    // Exit all active panels
    FOREACH_POST (it, mPanelList) {
        if (it->Active()) {
            it->mPanel->Exit();
        }
    }

    // Re-enter all active panels
    FOREACH_POST (it, mPanelList) {
        if (it->Active()) {
            it->mPanel->Enter();
        }
    }
}

void UIScreen::SetPanelActive(UIPanel *panel, bool active) {
    bool found = false;
    FOREACH (it, mPanelList) {
        if (it->mPanel == panel) {
            it->mActive = active;
            found = true;
        }
    }
    MILO_ASSERT(found, 0x164);
}

bool UIScreen::AllPanelsDown() {
    FOREACH (it, mPanelList) {
        if (it->Active() && it->mPanel->GetState() != UIPanel::kDown) {
            return false;
        }
    }

    return true;
}

bool UIScreen::SharesPanels(UIScreen *screen) {
    FOREACH (it, mPanelList) {
        if (screen->HasPanel(it->mPanel)) {
            return true;
        }
    }

    return false;
}

DataNode UIScreen::OnMsg(ButtonDownMsg const &msg) {
#ifdef HX_NATIVE
    // On Xbox, movie/overlay panels convert button presses to skip_selected
    // messages during fullscreen movies (attract, credits). On native, those
    // panels aren't functional (no BINK), so route the message directly.
    // The same DTA skip_selected handler fires, producing the same transition.
    {
        DataArray *td = TypeDef();
        if (td && td->FindArray("skip_selected", false)) {
            static Message skipMsg("skip_selected");
            HandleType(skipMsg);
            return DataNode(0);
        }
    }
#endif
    if (mBack != nullptr && msg.GetAction() == kAction_Cancel) {
        DataNode n = mBack->Evaluate(1);
        if (n.Type() != kDataUnhandled) {
            static Symbol go_back_screen("go_back_screen");
            Message m(go_back_screen, n.Str(), msg.GetUser());
            TheUI->Handle(m, false);
        }
    }

    return DATA_UNHANDLED;
}

DataNode UIScreen::ForeachPanel(const DataArray *da) {
    // {$screen foreach_panel $panel ...}

    DataNode *var = da->Var(2);
    DataNode tmp = *var;

    FOREACH_POST (it, mPanelList) {
        if (!it->mActive) {
            continue;
        }

        *var = it->mPanel;
        for (int i = 3; i < da->Size(); i++) {
            da->Command(i)->Execute();
        }
    }

    *var = tmp;
    return DataNode(0);
}

void UIScreen::ReloadStrings() {
    Message msg(Symbol("reload_string"));
    FOREACH (it, mPanelList) {
#ifdef HX_NATIVE
        if (!it->mPanel) continue;
#endif
        ObjectDir *panelDir = it->mPanel->DataDir();
        if (!panelDir) {
            continue;
        }
        for (ObjDirItr<UILabel> labelIt(panelDir, true); labelIt; ++labelIt) {
            labelIt->Handle(msg, true);
        }
    }
}

// EnterGlitchCB/UnloadGlitchCB defined at top of file
