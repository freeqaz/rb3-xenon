#include "ui/UI.h"
#include "UIComponent.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/em_asm.h>
#endif
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataUtl.h"
#include "obj/Dir.h"
#include "obj/MessageTimer.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/Joypad.h"
#include "os/JoypadClient.h"
#include "os/JoypadMsgs.h"
#include "os/Keyboard.h"
#include "os/System.h"
#include "os/UserMgr.h"
#include "rndobj/Cam.h"
#include "ui/CheatProvider.h"
#include "ui/InlineHelp.h"
#include "ui/LabelNumberTicker.h"
#include "ui/LabelShrinkWrapper.h"
#include "ui/LocalePanel.h"
#include "ui/PanelDir.h"
#include "ui/Screenshot.h"
#include "ui/UIButton.h"
#include "ui/UIColor.h"
#include "ui/UIFontImporter.h"
#include "ui/UIGuide.h"
#include "ui/UILabel.h"
#include "ui/UIList.h"
#include "ui/UIPicture.h"
#include "ui/UIScreen.h"
#include "ui/UIPanel.h"
#include "ui/UISlider.h"
#include "ui/UITrigger.h"
#include "utl/Cheats.h"
#include "utl/FilePath.h"
#include "utl/KnownIssues.h"
#include "utl/Locale.h"
#include "utl/OSCMessenger.h"
#include "utl/Std.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#ifdef HX_NATIVE
#include <cstdlib>
#include <cstring>
static float sHeadlessFakeUISeconds = 0.0f;
static bool sDebugUIFlow = false;
static bool sDebugUIFlowChecked = false;
static inline bool DebugUIFlow() {
    if (!sDebugUIFlowChecked) {
        sDebugUIFlow = getenv("MILO_DEBUG_UI_FLOW") != nullptr;
        sDebugUIFlowChecked = true;
    }
    return sDebugUIFlow;
}
#endif

namespace {
    JoypadAction NavButtonToNavAction(JoypadButton btn) {
        switch (btn) {
        case kPad_DLeft:
            return kAction_Left;
        case kPad_DRight:
            return kAction_Right;
        case kPad_DDown:
            return kAction_Down;
        case kPad_DUp:
            return kAction_Up;
        default:
            return kAction_None;
        }
    }

#ifdef HX_NATIVE
    enum NativeUICamMode {
        kNativeUICamDefault,      // Corrected: scale distance for HD visible area
        kNativeUICamOriginal,     // Original camera (too zoomed in for HD)
        kNativeUICamZHack,
        kNativeUICamRotateHack
    };

    NativeUICamMode GetNativeUICamMode() {
        static int cached = -1;
        if (cached == -1) {
            cached = kNativeUICamDefault;
            const char *env = std::getenv("MILO_UI_CAM_MODE");
            if (env && env[0]) {
                if (std::strcmp(env, "default") == 0) {
                    cached = kNativeUICamDefault;
                } else if (std::strcmp(env, "original") == 0 || std::strcmp(env, "none") == 0) {
                    cached = kNativeUICamOriginal;
                } else if (std::strcmp(env, "z_hack") == 0) {
                    cached = kNativeUICamZHack;
                } else if (std::strcmp(env, "rotate_hack") == 0) {
                    cached = kNativeUICamRotateHack;
                }
            }
        }
        return (NativeUICamMode)cached;
    }
#endif
}

const char *TransitionStateString(UIManager::TransitionState s) {
    switch (s) {
    case UIManager::kTransitionTo:
        return "to";
    case UIManager::kTransitionFrom:
        return "from";
    case UIManager::kTransitionPop:
        return "pop";
    default:
        return "";
    }
}

void TerminateCallback() {
    MILO_ASSERT(TheUI, 0x1CE);
    TheUI->Terminate();
}

void FailAppendCallback(FixedString &str) {
    if ((TheUI && TheUI->CurrentScreen()) || TheUI->TransitionScreen()) {
        str += "\n";
        if (TheUI->CurrentScreen()) {
            str += "Screen: ";
            str += TheUI->CurrentScreen()->Name();
        }
        if (TheUI->InTransition()) {
            str += "Trans: ";
            str += TransitionStateString(TheUI->GetTransitionState());
            str += " ";
            str += TheUI->TransitionScreen()->Name();
        }
    }
}

void UITerminateCallback() { TheUI->Terminate(); }

#pragma region UIManager

UIManager::UIManager()
    : mWentBack(0), mMaxPushDepth(100), mJoyClient(0), mCurrentScreen(0), mSink(0),
      mOverloadHorizontalNav(0), mCancelTransitionNotify(0), mDefaultAllowEditText(1),
      mDisableScreenBlacklight(0), mOverlay(0), mAutomator(0), mShowDevMenu(0) {}

UIManager::~UIManager() {}

void UIManager::SetScreenBlacklghtDisabled(bool disable) {
    mDisableScreenBlacklight = disable;
}

bool UIManager::DefaultAllowEditText() const { return mDefaultAllowEditText; }

bool UIManager::InComponentSelect() {
    if (mCurrentScreen)
        return mCurrentScreen->InComponentSelect();
    else
        return false;
}

UIPanel *UIManager::FocusPanel() {
    if (mCurrentScreen)
        return mCurrentScreen->FocusPanel();
    else
        return nullptr;
}

UIComponent *UIManager::FocusComponent() {
    UIPanel *focusPanel = FocusPanel();
    if (focusPanel)
        return focusPanel->FocusComponent();
    else
        return nullptr;
}

void UIManager::GotoFirstScreen() {
    UIScreen *screen = DataVariable("first_screen").Obj<UIScreen>();
#ifdef HX_NATIVE
    if (DebugUIFlow()) printf("DC3 UI: GotoFirstScreen -> '%s'\n", screen ? screen->Name() : "<null>");
#endif
    GotoScreen(screen, false, false);
    mTimer.Restart();
}

void UIManager::ToggleLoadTimes() {
    mOverlay->CurrentLine() = gNullStr;
    mOverlay->SetShowing(!mOverlay->Showing());
}

void UIManager::Draw() {
#ifdef HX_NATIVE
    if (false) {
        printf("DC3 UI::Draw: cam=%p env=%p screen=%s pushed=%d\n",
               mCam, mEnv,
               mCurrentScreen ? mCurrentScreen->Name() : "<null>",
               (int)mPushedScreens.size());
        if (mCam) {
            const Vector3& cp = mCam->WorldXfm().v;
            const Hmx::Matrix3& cm = mCam->WorldXfm().m;
            printf("  UI cam pos=(%.1f,%.1f,%.1f) near=%.1f far=%.1f fov=%.1f\n",
                   cp.x, cp.y, cp.z, mCam->NearPlane(), mCam->FarPlane(), mCam->YFov());
            printf("  UI cam rot: fwd=(%.2f,%.2f,%.2f) up=(%.2f,%.2f,%.2f) right=(%.2f,%.2f,%.2f)\n",
                   cm.z.x, cm.z.y, cm.z.z, cm.y.x, cm.y.y, cm.y.z, cm.x.x, cm.x.y, cm.x.z);
        }
    }
    // Select the UI camera and environment for screen-space rendering.
    // On Xbox 360, NgRnd's draw pipeline did this per-panel. Our native
    // renderer uses a single pass, so we select once before UI draws.
    RndCam* savedCam = RndCam::Current();
    RndEnviron* savedEnv = RndEnviron::Current();
    if (mCam) {
        switch (GetNativeUICamMode()) {
        case kNativeUICamDefault:
            break;
        case kNativeUICamOriginal:
            break;
        case kNativeUICamZHack:
            mCam->SetLocalPos(Vector3(0, -768, 387));
            break;
        case kNativeUICamRotateHack: {
            Transform camXfm;
            camXfm.m.x = Vector3(0, 0, 1);
            camXfm.m.y = Vector3(0, 1, 0);
            camXfm.m.z = Vector3(-1, 0, 0);
            camXfm.v = Vector3(0, -768, 387);
            mCam->SetLocalXfm(camXfm);
            break;
        }
        }
        mCam->Select();
    }
    if (mEnv) mEnv->Select(nullptr);
#endif
    for (std::vector<UIScreen *>::iterator it = mPushedScreens.begin();
         it != mPushedScreens.end();
         ++it) {
        (*it)->Draw();
    }
    if (mCurrentScreen)
        mCurrentScreen->Draw();
#ifdef HX_NATIVE
    // Restore previous camera/environment
    if (savedCam) savedCam->Select();
    if (savedEnv) savedEnv->Select(nullptr);
#endif
}

void UIManager::GotoScreen(const char *name, bool b2, bool b3) {
    UIScreen *screen = ObjectDir::Main()->Find<UIScreen>(name, true);
    MILO_ASSERT(screen, 0x37E);
    GotoScreen(screen, b2, b3);
}

void UIManager::GotoScreen(UIScreen *scr, bool b1, bool b2) {
    GotoScreenImpl(scr, b1, b2);
}

void UIManager::ResetScreen(UIScreen *screen) {
    if (mTransitionState != kTransitionNone && mTransitionState != kTransitionFrom) {
        bool old = mCancelTransitionNotify;
        mCancelTransitionNotify = false;
        CancelTransition();
        mCancelTransitionNotify = old;
    }
    if (mPushedScreens.empty()) {
        GotoScreen(screen, false, false);
    } else {
        MILO_ASSERT(mPushedScreens.size() == 1, 0x3E5);
        PopScreen(screen);
    }
}

UIScreen *UIManager::BottomScreen() {
    return !mPushedScreens.empty() ? mPushedScreens.front() : mCurrentScreen;
}

int UIManager::PushDepth() const { return mPushedScreens.size(); }

UIScreen *UIManager::ScreenAtDepth(int depth) {
    MILO_ASSERT(depth < mPushedScreens.size(), 0x46F);
    return mPushedScreens[depth];
}

void UIManager::UseJoypad(bool useJoypad, bool enableAutoRepeat) {
#ifdef HX_NATIVE
    if (DebugUIFlow()) printf("DC3 UI: UseJoypad(%d, %d)\n", (int)useJoypad, (int)enableAutoRepeat);
#endif
    if (useJoypad && !mJoyClient) {
        mJoyClient = new JoypadClient(this);
        mJoyClient->SetVirtualDpad(true);
        if (enableAutoRepeat) {
            mJoyClient->SetRepeatMask(0xf000);
        }
    } else if (!useJoypad) {
        if (mJoyClient) {
            RELEASE(mJoyClient);
        }
    }
}

void UIManager::CancelTransition() {
    if (mCancelTransitionNotify && mTransitionState != kTransitionNone
        && mTransitionState != kTransitionFrom) {
        MILO_NOTIFY("Cancelled transition");
    }
    TransitionState oldState = mTransitionState;
    UIScreen *oldScreen = mTransitionScreen;
    mTransitionState = kTransitionNone;
    mTransitionScreen = nullptr;
    if (oldState == kTransitionTo) {
        if (mCurrentScreen) {
            mCurrentScreen->Enter(oldScreen);
        } else if (oldScreen)
            oldScreen->UnloadPanels();
    } else if (oldState == kTransitionPop && mCurrentScreen) {
        mCurrentScreen->Enter(nullptr);
    }
}

bool UIManager::OverloadHorizontalNav(JoypadAction act, JoypadButton btn, bool b) const {
    return !(!mOverloadHorizontalNav || NavButtonToNavAction(btn) == act && !b);
}

void UIManager::Terminate() {
    CheatProvider::Terminate();
    UILabel::Terminate();
    SetName(0, 0);
    KeyboardUnsubscribe(this);
    RELEASE(mCam);
    RELEASE(mEnv);
    RELEASE(mJoyClient);
    TheDebug.RemoveExitCallback(TerminateCallback);
    RELEASE(mAutomator);
}

bool UIManager::IsGameScreenActive() {
    bool ret = BottomScreen() && streq(BottomScreen()->Name(), "game_screen");
    auto& _ref0 = mCurrentScreen;
    if (_ref0)
        ret &= BottomScreen() != _ref0;
    return ret;
}

bool UIManager::BlockHandlerDuringTransition(Symbol s, DataArray *da) {
    if (s != KeyboardKeyMsg::Type()) {
        if (s == ButtonDownMsg::Type() || s == ButtonUpMsg::Type()) {
            UIPanel *focus = FocusPanel();
            if (focus) {
                static Symbol allowed_transition_actions("allowed_transition_actions");
                const DataNode *prop = focus->Property(allowed_transition_actions, false);
                DataArray *arr;
                if (prop)
                    arr = prop->Array();
                else
                    arr = nullptr;
                if (arr) {
                    for (int i = 0; i < arr->Size(); i++) {
                        if (arr->Int(i) == da->Int(4))
                            return false;
                    }
                }
            }
        } else {
            return false;
        }
    }
    return true;
}

void UIManager::GotoScreenImpl(UIScreen *scr, bool b1, bool b2) {
#ifdef HX_NATIVE
    // Skip screens that require campaign/performer session state.
    if (scr && strstr(scr->Name(), "campaign")) {
        MILO_WARN("Skipping screen '%s' on native (campaign not supported)", scr->Name());
        return;
    }
    if (DebugUIFlow()) printf("DC3 UI: GotoScreenImpl -> '%s' (force=%d, b2=%d)\n",
           scr ? scr->Name() : "<null>", b1, b2);
#endif
    // Only proceed if:
    // - Force transition (b1), OR
    // - Already in a transition, OR
    // - Changing to a different screen
    // AND we're not already transitioning to this exact screen
    if (((b1 || mTransitionState != kTransitionNone) || mCurrentScreen != scr)
        && ((mTransitionState != kTransitionTo && mTransitionState != kTransitionPop)
            || mTransitionScreen != scr)) {
        CancelTransition();

#ifdef MILO_DEBUG
        // Verify that the new screen doesn't share panels with any pushed screens
        // (panels should be unique per screen to avoid resource conflicts)
        if (scr) {
            for (std::vector<UIScreen *>::iterator it = mPushedScreens.begin();
                 it != mPushedScreens.end();
                 ++it) {
                if (scr->SharesPanels(*it)) {
                    MILO_FAIL("%s shares panels with %s", scr->Name(), (*it)->Name());
                }
            }
        }
#endif

        // Log the transition for debugging
        const char *curName = mCurrentScreen ? mCurrentScreen->Name() : "<none>";
        const char *newName = scr ? scr->Name() : "<none>";
        TheDebug << MakeString("transition from %s to %s\n", curName, newName);
#ifdef HX_NATIVE
        if (DebugUIFlow()) printf("DC3 UI: Transition '%s' -> '%s' (wentBack=%d)\n", curName, newName, (int)b2);
#endif

        // Store transition state and notify listeners
        mWentBack = b2;
        UIScreenChangeMsg msg(scr, mCurrentScreen, mWentBack);
        Handle(msg, false);

        // Begin transition to new screen
        mTransitionState = kTransitionTo;
        mTransitionScreen = scr;

        // Exit current screen or load new screen panels
        if (mCurrentScreen) {
            mCurrentScreen->Exit(scr);
        } else if (scr) {
            scr->LoadPanels();
        }

#ifdef MILO_DEBUG
        // Start tracking load time for the new screen
        if (mTransitionScreen) {
            if (mOverlay) mOverlay->CurrentLine() = gNullStr;
            mLoadTimer.Restart();
        }
#endif
    }
}

void UIManager::PopScreen(UIScreen *screen) {
    if (mPushedScreens.empty()) {
        MILO_NOTIFY("No screen to pop\n");
    } else {
        GotoScreenImpl(nullptr, false, false);
        mTransitionState = kTransitionPop;
        if (screen)
            mTransitionScreen = screen;
        else
            mTransitionScreen = mPushedScreens.back();
    }
}

DataNode UIManager::OnIsResource(DataArray *arr) {
    Symbol sym = arr->Sym(3);
    static Symbol objects("objects");
    static Symbol resources_path("resources_path");
    DataArray *rsrcArr = SystemConfig(objects, sym)->FindArray(resources_path, false);
    if (rsrcArr) {
        FilePath rsrcPath(FileMakePath(FileGetPath(rsrcArr->File()), rsrcArr->Str(1)));
        FilePath inputPath(FileRoot(), arr->Str(2));
        if (rsrcPath == FileGetPath(inputPath.c_str()))
            return 1;
    } else {
        MILO_NOTIFY("%s does not have a resources_path set", sym);
    }
    return 0;
}

DataNode UIManager::OnGotoScreen(DataArray const *arr) {
#ifdef HX_NATIVE
    if (DebugUIFlow()) {
        const DataNode &node = arr->Evaluate(2);
        const char *curScr = mCurrentScreen ? mCurrentScreen->Name() : "<none>";
        if (node.Type() == kDataObject) {
            Hmx::Object *o = node.GetObj(nullptr);
            printf("DC3 UI: goto_screen from '%s' -> obj='%s'\n", curScr, o ? o->Name() : "<null>");
        } else if (node.Type() == kDataSymbol || node.Type() == kDataString) {
            printf("DC3 UI: goto_screen from '%s' -> sym='%s'\n", curScr, node.Sym().Str());
        } else {
            printf("DC3 UI: goto_screen from '%s' -> type=%d\n", curScr, (int)node.Type());
        }
    }
#endif
    Hmx::Object *obj = arr->GetObj(2);
    UIScreen *screen = dynamic_cast<UIScreen *>(obj);
    if (screen == nullptr && obj)
        MILO_FAIL("%s is not a screen", obj->Name());

#ifdef HX_NATIVE
    // If DTA resolves to null screen (e.g., tutorial exit with missing state),
    // try falling back to main_screen to avoid dead-end
    if (screen == nullptr && !obj) {
        UIScreen *fallback = ObjectDir::Main()->Find<UIScreen>("main_screen", false);
        if (fallback) {
            if (DebugUIFlow()) printf("DC3 UI: goto_screen resolved to null, falling back to 'main_screen'\n");
            screen = fallback;
        }
    }
#endif

    if (arr->Size() > 4) {
        GotoScreen(screen, arr->Int(3), arr->Int(4));
    } else if (arr->Size() > 3) {
        GotoScreen(screen, arr->Int(3), false);
    } else {
        GotoScreen(screen, false, false);
    }
    return 0;
}

DataNode UIManager::OnGoBackScreen(DataArray const *arr) {
    Hmx::Object *obj = arr->GetObj(2);
    UIScreen *screen = dynamic_cast<UIScreen *>(obj);
    if (screen == nullptr && obj) {
        MILO_FAIL("%s is not a screen", obj->Name());
    }
    GotoScreen(screen, false, true);
    return DATA_UNHANDLED;
}

void UIManager::ReloadStrings() {
    Message msg(Symbol("reload_strings"));

    if (mCurrentScreen) {
        mCurrentScreen->Handle(msg, true);
    }

    FOREACH (it, mPushedScreens) {
        (*it)->Handle(msg, true);
    }
}

void UIManager::FakeKeyboardAction(JoypadButton btn, JoypadAction action) {
    static ButtonDownMsg downMsg(nullptr, (JoypadButton)0x18, (JoypadAction)0, 0);
    downMsg[0] = TheUserMgr->GetLocalUserFromPadNum(0);
    downMsg[1] = btn;
    downMsg[2] = action;
    downMsg[3] = 0;
    Handle(downMsg, false);
}

void UIManager::Poll() {
    START_AUTO_TIMER("ui_poll_raw");
    if (mAutomator)
        mAutomator->Poll();
#ifdef HX_NATIVE
    // Headless mode: advance UI seconds by fixed 1/30s per frame (see TaskMgr::Poll).
    // Uses a file-scope variable so the screen-transition reset can zero it.
    static bool sHeadless = !!getenv("MILO_HEADLESS");
    if (sHeadless) {
        sHeadlessFakeUISeconds += 1.0f / 30.0f;
        TheTaskMgr.SetUISeconds(sHeadlessFakeUISeconds, false);
    } else
#endif
    TheTaskMgr.SetUISeconds(mTimer.SplitMs() * 0.001f, false);
    for (std::vector<UIScreen *>::iterator it = mPushedScreens.begin();
         it != mPushedScreens.end();
         ++it) {
        (*it)->Poll();
    }
    if (mCurrentScreen)
        mCurrentScreen->Poll();
#ifdef HX_NATIVE
    // Boot flow: advance pre-main screens that depend on unavailable subsystems
    // (BINK movies, save system, Kinect init). These are infrastructure screens,
    // not game flow — DTA handler order for the game starts at main_screen.
    // From main_screen onward, flow is 1:1 with Xbox (DTA + input script driven).
    {
        static const char *sStuckScreen = nullptr;
        static int sStuckFrames = 0;

        const char *curName = mCurrentScreen ? mCurrentScreen->Name() : nullptr;
        if (curName && mTransitionState == kTransitionNone) {
            if (curName == sStuckScreen) {
                sStuckFrames++;
            } else {
                sStuckScreen = curName;
                sStuckFrames = 0;
            }

            struct BootAdvance {
                const char *from;
                const char *to;
                int delay;
            };
            // DC3_FAST_BOOT=1 (env var) or ?fast_boot=1 (URL param on web):
            // skip boot screens in 10 frames instead of ~360
            static int sFastBoot = -1;
            if (sFastBoot < 0) {
                const char *fb = getenv("DC3_FAST_BOOT");
                sFastBoot = (fb && atoi(fb) != 0) ? 1 : 0;
#ifdef __EMSCRIPTEN__
                if (!sFastBoot) {
                    sFastBoot = EM_ASM_INT({
                        var s = location.search || "";
                        return (s.indexOf("fast_boot=1") >= 0) ? 1 : 0;
                    });
                }
#endif
                if (sFastBoot)
                    printf("DC3 UI: Fast boot enabled (10-frame transitions)\n");
            }
            int fast = sFastBoot ? 10 : 0;

            static const BootAdvance sBoot[] = {
                {"attract_screen", "autosave_warning_screen", 90},
                {"autosave_warning_screen", "title_screen", 90},
                {"title_screen", "wait_main_after_saveload_screen", 60},
                {"wait_main_after_saveload_screen", "main_screen", 120},
                // Kinect tutorials — skip if somehow reached
                {"title_screen_to_voice_control_tutorial_screen", "main_screen", 1},
                {"tutorial_voice_control_screen_0", "main_screen", 1},
                {"tutorial_voice_control_screen_1", "main_screen", 1},
                {"tutorial_party_mode_screen_0", "main_screen", 1},
                {"tutorial_party_mode_screen_1", "main_screen", 1},
                {nullptr, nullptr, 0}
            };

            for (int i = 0; fast && sBoot[i].from; i++) {
                int delay = (sBoot[i].delay > 10) ? 10 : sBoot[i].delay;
                if (!strcmp(curName, sBoot[i].from) && sStuckFrames == delay) {
                    UIScreen *next = ObjectDir::Main()->Find<UIScreen>(sBoot[i].to, false);
                    if (next) {
                        if (DebugUIFlow() || fast)
                            fprintf(stderr, "DC3 UI: Boot advance '%s' -> '%s' (after %d frames)\n",
                                   curName, sBoot[i].to, delay);
                        sStuckScreen = nullptr;
                        sStuckFrames = 0;
                        GotoScreen(next, false, false);
                    }
                    break;
                }
            }
        } else {
            sStuckFrames = 0;
        }
    }
#endif
    if (mTransitionState == kTransitionTo) {
#ifdef HX_NATIVE
        {
            static const char *sLastTrans = nullptr;
            static int sTransCount = 0;
            const char *curTrans = mTransitionScreen ? mTransitionScreen->Name() : "<null>";
            if (curTrans != sLastTrans) {
                sLastTrans = curTrans;
                sTransCount = 0;
            }
            if (sTransCount < 3 || sTransCount % 500 == 0) {
                bool loaded = !mTransitionScreen || mTransitionScreen->CheckIsLoaded();
                bool exited = !mCurrentScreen || !mCurrentScreen->Exiting();
                bool blocked = IsBlockingTransition();
#ifdef HX_WEB
                printf("DC3 UI: TransitionTo check #%d: loaded=%d exited=%d blocked=%d "
                       "trans='%s' cur='%s'\n",
                       sTransCount, loaded, exited, (int)blocked,
                       curTrans,
                       mCurrentScreen ? mCurrentScreen->Name() : "<null>");
                fflush(stdout);
#else
                if (DebugUIFlow()) printf("DC3 UI: TransitionTo check: loaded=%d exited=%d blocked=%d "
                       "trans='%s' cur='%s'\n",
                       loaded, exited, (int)blocked,
                       curTrans,
                       mCurrentScreen ? mCurrentScreen->Name() : "<null>");
#endif
            }
            sTransCount++;
        }
#endif
        if ((!mTransitionScreen || mTransitionScreen->CheckIsLoaded())
            && (!mCurrentScreen || !mCurrentScreen->Exiting())
            && !IsBlockingTransition()) {
            UIScreen *trans = mTransitionScreen;
            UIScreen *oldCur = mCurrentScreen;
            mTransitionState = kTransitionFrom;
            mCurrentScreen = trans;
#ifdef HX_NATIVE
            // Native: DTA set_sink never fires in DC3 (investigated 2026-03-12).
            // Screens don't have {ui set_sink} in their TypeDefs. Set mSink to
            // current screen so HANDLE_MEMBER_PTR(mSink) routes button input.
            mSink = trans;
#endif
            mTransitionScreen = oldCur;
#ifdef HX_WEB
            printf("DC3 UI: transition complete, will enter '%s'\n", trans ? trans->Name() : "<null>");
            fflush(stdout);
#endif
            if (trans) {
                if (trans->AllPanelsDown() && mPushedScreens.empty()
                    && IsTimelineResetAllowed()) {
                    mTimer.Restart();
                    TheTaskMgr.SetUISeconds(0, true);
#ifdef HX_NATIVE
                    // Reset the headless fake UI seconds accumulator in sync
                    // with the real timer restart (see UIManager::Poll).
                    sHeadlessFakeUISeconds = 0.0f;
#endif
                }

                mCurrentScreen->Enter(mTransitionScreen);
            }
        }
    }
    if (mTransitionState == kTransitionPop) {
        if (!mCurrentScreen || !mCurrentScreen->Exiting()) {
            if (mCurrentScreen)
                mCurrentScreen->UnloadPanels();
            UIScreen *oldCurScreen = mCurrentScreen;
            MILO_ASSERT(!mPushedScreens.empty(), 0x2D8);
            mCurrentScreen = mPushedScreens.back();
            mPushedScreens.pop_back();
            mTransitionState = kTransitionNone;
            if (mTransitionScreen == mCurrentScreen) {
                mTransitionScreen = nullptr;
                UITransitionCompleteMsg completeMsg(mCurrentScreen, oldCurScreen);
                Handle(completeMsg, false);
            } else {
                GotoScreenImpl(mTransitionScreen, false, false);
            }
        }
    }
    if (mTransitionState == kTransitionFrom) {
#ifdef HX_NATIVE
        // Wait for enter animations with timeout
        static int sEnterWaitFrames = 0;
        bool screenEntered = !mCurrentScreen || !mCurrentScreen->Entering();
        if (!screenEntered) {
            if (++sEnterWaitFrames > 90) { // ~3s safety net for stuck enter animations
                printf("DC3 UI WARNING: Enter animation timeout for '%s' — force-completing\n",
                       mCurrentScreen ? mCurrentScreen->Name() : "<null>");
                screenEntered = true;
                sEnterWaitFrames = 0;
            }
        } else {
            sEnterWaitFrames = 0;
        }
#endif
        if (
#ifdef HX_NATIVE
            screenEntered
#else
            !mCurrentScreen || !mCurrentScreen->Entering()
#endif
            ) {
            if (mOverlay && mOverlay->Showing() && mLoadTimer.Running()
                && mCurrentScreen) {
                mLoadTimer.Stop();
                mOverlay->CurrentLine() = MakeString(
                    "%s entered in %f seconds",
                    mCurrentScreen->Name(),
                    mLoadTimer.Ms() * 0.001f
                );
                TheDebug << MakeString("%s\n", mOverlay->CurrentLine());
            }
            UIScreen *oldTrans = mTransitionScreen;
            UITransitionCompleteMsg completeMsg2(mCurrentScreen, oldTrans);
            mTransitionState = kTransitionNone;
            mTransitionScreen = nullptr;
#ifdef HX_NATIVE
            if (DebugUIFlow()) printf("DC3 UI: Transition complete -> '%s' (from '%s')\n",
                   mCurrentScreen ? mCurrentScreen->Name() : "<null>",
                   oldTrans ? oldTrans->Name() : "<null>");
#endif
            Handle(completeMsg2, false);
        }
    }
    TheKnownIssues.Draw();
    TheOSCMessenger.Poll();
}

void UIManager::PushScreen(UIScreen *screen) {
    MILO_ASSERT(screen, 0x38C);
    if (!mCurrentScreen) {
        MILO_NOTIFY(
            "Called PushScreen() with %s when mCurrentScreen is NULL, are you calling PushScreen() twice in the same frame?",
            screen->Name()
        );
    } else {
        CancelTransition();
        if (mCurrentScreen) {
            mPushedScreens.push_back(mCurrentScreen);
        } else {
            MILO_LOG("UIManager::PushScreen NULL current screen. Not pushing it.\n");
        }
        if (mPushedScreens.size() >= mMaxPushDepth) {
            MILO_NOTIFY(
                "Exceeded max push depth of %i, pushing %s", mMaxPushDepth, screen->Name()
            );
            MILO_LOG("mPushedScreens:\n");
            FOREACH (it, mPushedScreens) {
                if (*it) {
                    MILO_LOG("%s\n", (*it)->Name());
                } else {
                    MILO_LOG("NULL pushed screen? That's pretty bad.\n");
                }
            }
        }
        mCurrentScreen = nullptr;
        GotoScreenImpl(screen, false, false);
    }
}

DataNode UIManager::OnForeachCurrentScreen(const DataArray *arr) {
    DataNode *var = arr->Var(2);
    DataNode n(*var);
    std::vector<UIScreen *> screens(mPushedScreens);
    if (mCurrentScreen) {
        screens.push_back(mCurrentScreen);
    }
    FOREACH (it, screens) {
        *var = *it;
        for (int i = 3; i < arr->Size(); i++) {
            arr->Command(i)->Execute();
        }
    }
    *var = n;
    return 0;
}

void UIManager::Init() {
    MILO_ASSERT(TheUI, 0x1f3);
    mAutomator = new Automator(*this);
    SetName("ui", ObjectDir::Main());
    DataArray *cfg = SystemConfig("ui");
    SetTypeDef(SystemConfig("ui"));
    UseJoypad(cfg->FindInt("use_joypad"), cfg->FindInt("enable_auto_repeat"));
    KeyboardSubscribe(this);
    mCurrentScreen = nullptr;
    mTransitionState = kTransitionNone;
    mTransitionScreen = nullptr;
    mWentBack = false;
    mCam = ObjectDir::Main()->New<RndCam>("[ui.cam]");
    DataArray *camCfg = cfg->FindArray("cam");
    mCam->SetFrustum(
        camCfg->FindFloat("near"),
        camCfg->FindFloat("far"),
        camCfg->FindFloat("fov") * DEG2RAD,
        1.0f
    );
    mCam->SetLocalPos(Vector3(0, camCfg->FindFloat("y"), 0));
    DataArray *zArr = camCfg->FindArray("z-range");
    mCam->SetZRange(zArr->Float(1), zArr->Float(2));
    mEnv = Hmx::Object::New<RndEnviron>();
    Hmx::Color envAmbientColor;
    cfg->FindArray("env")->FindData("ambient", envAmbientColor, true);
    mEnv->SetAmbientColor(envAmbientColor);
    cfg->FindData("max_push_depth", mMaxPushDepth, false);
    cfg->FindData("cancel_transition_notify", mCancelTransitionNotify, false);
    cfg->FindData("default_allow_edit_text", mDefaultAllowEditText, false);
    bool notify = false;
    cfg->FindData("verbose_locale_notifies", notify, false);
    Locale::SetLocaleVerboseNotify(notify);
    REGISTER_OBJ_FACTORY(UIScreen)
    REGISTER_OBJ_FACTORY(UIPanel)
    REGISTER_OBJ_FACTORY(PanelDir)
    UIComponent::Init();
    UIButton::Init();
    REGISTER_OBJ_FACTORY(UIColor)
    UILabel::Init();
    UIList::Init();
    REGISTER_OBJ_FACTORY(UIPicture)
    UISlider::Init();
    REGISTER_OBJ_FACTORY(UITrigger)
    InlineHelp::Init();
    REGISTER_OBJ_FACTORY(UIFontImporter)
    REGISTER_OBJ_FACTORY(UIGuide)
    REGISTER_OBJ_FACTORY(Screenshot)
    LabelNumberTicker::Init();
    LabelShrinkWrapper::Init();
    TheDebug.AddExitCallback(UITerminateCallback);

    std::vector<ObjDirPtr<ObjectDir> > dirPtrs;
    DataArray *frontloadArr = cfg->FindArray("frontload_subdirs", false);
    if (frontloadArr) {
        dirPtrs.resize(frontloadArr->Size() - 1);
        for (int i = 1; i < frontloadArr->Size(); i++) {
            String curStr = frontloadArr->Str(i);
            dirPtrs[i - 1].LoadFile(curStr.c_str(), false, true, kLoadFront, false);
        }
    }
    CheatProvider::Init();
    REGISTER_OBJ_FACTORY(LocalePanel)
    static Message cheat_init("cheat_init");
    Hmx::Object::Handle(cheat_init, false);
    mOverlay = RndOverlay::Find("ui", true);
    mOverlay->SetShowing(false);
    TheOSCMessenger.Connect();
    TheDebug.AddFailAppendCallback(FailAppendCallback);
    PreloadSharedSubdirs("ui");
    UILabel::sRequireFixedLength = true;
    static Message init("init");
    Hmx::Object::Handle(init, false);
    UILabel::sRequireFixedLength = false;
    cfg->FindData("overload_horizontal_nav", mOverloadHorizontalNav, false);
    TheKnownIssues.Init();
}

BEGIN_HANDLERS(UIManager)
    if ((InTransition() || InComponentSelect())
        && BlockHandlerDuringTransition(sym, _msg)) {
        return 0;
    }
    HANDLE_MEMBER_PTR(mSink)
    HANDLE_ACTION(set_sink, mSink = _msg->Obj<Hmx::Object>(2))
    HANDLE_ACTION(use_joypad, UseJoypad(_msg->Int(2), true))
    HANDLE_ACTION(set_virtual_dpad, mJoyClient->SetVirtualDpad(_msg->Int(2)))
    HANDLE_ACTION(push_screen, PushScreen(_msg->Obj<UIScreen>(2)))
    HANDLE_ACTION_IF_ELSE(
        pop_screen, _msg->Size() > 2, PopScreen(_msg->Obj<UIScreen>(2)), PopScreen(0)
    )
    HANDLE(goto_screen, OnGotoScreen)
    HANDLE(go_back_screen, OnGoBackScreen)
    HANDLE_ACTION(reset_screen, ResetScreen(_msg->Obj<UIScreen>(2)))
    HANDLE_EXPR(focus_panel, FocusPanel())
    HANDLE_EXPR(current_screen, CurrentScreen())
    HANDLE_EXPR(transition_screen, TransitionScreen())
    HANDLE_EXPR(bottom_screen, BottomScreen())
    HANDLE_EXPR(in_transition, InTransition())
    HANDLE(is_resource, OnIsResource)
    HANDLE(foreach_current_screen, OnForeachCurrentScreen)
    HANDLE_EXPR(went_back, WentBack())
    HANDLE_EXPR(is_game_screen_active, IsGameScreenActive())
    HANDLE_ACTION(toggle_load_times, ToggleLoadTimes())
    HANDLE_EXPR(showing_load_times, mOverlay->Showing())
    HANDLE_ACTION(toggle_dev_menu, mShowDevMenu = !mShowDevMenu)
    HANDLE_EXPR(show_dev_menu, mShowDevMenu)
    HANDLE_MEMBER_PTR(mAutomator)
    HANDLE_ACTION(
        fake_keyboard_action,
        FakeKeyboardAction((JoypadButton)_msg->Int(2), (JoypadAction)_msg->Int(3))
    )
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_MEMBER_PTR(mCurrentScreen)
END_HANDLERS

#pragma endregion UIManager
#pragma region AutoMator

const char *Automator::ToggleAuto() {
    mCurScript = 0;
    if (mScreenScripts) {
        mScreenScripts->Release();
        mScreenScripts = 0;
    } else {
        Loader *ldr = TheLoadMgr.AddLoader(mAutoPath.c_str(), kLoadFront);
        DataLoader *dl = dynamic_cast<DataLoader *>(ldr);
        MILO_ASSERT(dl, 0x90);
        TheLoadMgr.PollUntilLoaded(dl, 0);
        mScreenScripts = dl->Data();
        mCurScreenIndex = 0;
        if (mScreenScripts) {
            StartAuto(mUIManager.CurrentScreen());
        }
    }
    return AutoScript();
}

void Automator::StartAuto(UIScreen *screen) {
    MILO_ASSERT(mScreenScripts, 0xC0);
    mCurScript = nullptr;
    if (screen) {
        mCurMsgIndex = 1;
        for (int i = mCurScreenIndex; i < mScreenScripts->Size(); i++) {
            DataArray *arr = mScreenScripts->Array(i);
            if (arr->Sym(0) == screen->Name()) {
                mCurScript = arr;
                mCurScreenIndex++;
                break;
            }
        }
    }
}

Symbol Automator::CurRecordScreen() {
    DataArray *recordArr = mRecord;
    if (recordArr->Size() > 0) {
        return recordArr->Array(recordArr->Size() - 1)->Sym(0);
    } else
        return gNullStr;
}

void Automator::AddRecord(Symbol s, DataArray *arr) {
    MILO_ASSERT(mRecord, 0x14F);
    int recordSize = mRecord->Size();
    DataArray *addArr;
    if (CurRecordScreen() == s) {
        addArr = mRecord->Array(recordSize - 1);
    } else {
        addArr = new DataArray(1);
        addArr->Node(0) = s;
        mRecord->Insert(recordSize, addArr);
    }
    addArr->Insert(addArr->Size(), arr);
}

void Automator::FinishRecord() {
    if (mRecord) {
        MILO_ASSERT(!mRecordPath.empty(), 0x162);
        DataWriteFile(mRecordPath.c_str(), mRecord, 0);
    }
    if (mRecord) {
        mRecord->Release();
        mRecord = nullptr;
    }
}

Automator::Automator(UIManager &mgr)
    : mUIManager(mgr), mScreenScripts(0), mRecord(0), mAutoPath("automator.dta"),
      mRecordPath("automator.dta"), mCurScript(0), mSkipNextQuickCheat(0) {}

Automator::~Automator() {
    if (mScreenScripts) {
        mScreenScripts->Release();
        mScreenScripts = 0;
    }
    FinishRecord();
}

DataNode Automator::OnCustomMsg(const Message &msg) {
    Symbol key = msg.Type();
    // ain't no way this is how hmx wrote it
    std::list<Symbol>::iterator it = mCustomMsgs.begin();
    if (it != mCustomMsgs.end()) {
        for (; it != mCustomMsgs.end() && *it != key; ++it)
            ;
        if (it != mCustomMsgs.end())
            HandleMessage(key);
    }
    return DATA_UNHANDLED;
}

DataNode Automator::OnMsg(const UITransitionCompleteMsg &msg) {
    if (mScreenScripts && !mRecord)
        StartAuto(msg.GetNewScreen());
    return DATA_UNHANDLED;
}

void Automator::FillButtonMsg(ButtonDownMsg &msg, int idx) {
    MILO_ASSERT(mCurScript, 0x141);
    DataArray *b = mCurScript->Array(idx);
    static Symbol button_down("button_down");
    MILO_ASSERT(b->Sym(0) == button_down, 0x144);
    int padnum = b->Int(3);
    msg[0] = TheUserMgr->GetLocalUserFromPadNum(padnum);
    msg[1] = b->Int(1);
    msg[2] = b->Int(2);
    msg[3] = padnum;
}

void Automator::AdvanceScript(Symbol msg) {
    if (mCurScript) {
        if (mCurScript->Array(mCurMsgIndex)->Sym(0) == msg) {
            mFramesSinceAdvance = 0;
            mCurMsgIndex++;
            if (mCurMsgIndex >= mCurScript->Size()) {
                mCurScript = 0;
                if (mScreenScripts->Size() == mCurScreenIndex) {
                    static Message msg("auto_script_done");
                    mUIManager.Handle(msg, false);
                }
            }
        }
    }
}

char const *Automator::ToggleRecord() {
    if (mRecord != nullptr) {
        FinishRecord();

    } else {
        mSkipNextQuickCheat = true;
        mRecord = new DataArray(0);
    }

    if (mRecord != nullptr)
        return mRecordPath.c_str();
    else
        return "OFF";
}

Symbol Automator::CurScreenName() {
    UIScreen *curScreen = mUIManager.CurrentScreen();
    if (curScreen) {
        static Message msg(Symbol("is_system_cheat"));
        DataNode handled = curScreen->Handle(msg, false);
        bool unhandled = handled.Equal(DataNode(kDataUnhandled, 0), 0, 1) || handled.Int() == 0;
        if (unhandled) {
            return curScreen->Name();
        }
    }
    return gNullStr;
}

void Automator::Poll() {
    static Symbol button_down("button_down");
    static Symbol quick_cheat("quick_cheat");
    static ButtonDownMsg b_msg(nullptr, kPad_NumButtons, kAction_None, -1);
    if (!mCurScript)
        return;
    mFramesSinceAdvance++;
    DataArray *curEntry = mCurScript->Array(mCurMsgIndex);
    Symbol sym = curEntry->Sym(0);
    if (sym == button_down) {
        FillButtonMsg(b_msg, mCurMsgIndex);
        static Symbol button_down("button_down");
        AdvanceScript(button_down);
        mUIManager.Handle(b_msg, false);
    } else if (sym == quick_cheat) {
        DataArray *cheatArr = curEntry->Node(1).Array();
        AdvanceScript(quick_cheat);
        CallQuickCheat(cheatArr, nullptr);
    } else if (mCurMsgIndex > 1) {
        if (mFramesSinceAdvance > 0x1e) {
        int prevIdx = mCurMsgIndex - 1;
        DataArray *prevEntry = mCurScript->Array(prevIdx);
        if (prevEntry->Sym(0) == button_down) {
            FillButtonMsg(b_msg, prevIdx);
            mUIManager.Handle(b_msg, false);
        }
    }
    }
}

DataNode Automator::OnMsg(ButtonDownMsg const &msg) {
    Symbol screenName = CurScreenName();
    if (mRecord && !screenName.Null()) {
        static Symbol button_down("button_down");
        DataArrayPtr ptr(
            button_down,
            DataGetMacroByInt(msg.GetPadNum(), "kPad_"),
            DataGetMacroByInt(msg.GetAction(), "kAction_"),
            msg.GetButton()
        );
        AddRecord(screenName, ptr);
    }
    return DATA_UNHANDLED;
}

DataNode Automator::OnCheatInvoked(DataArray const *arr) {
    if (mRecord) {
        if (mSkipNextQuickCheat) {
            mSkipNextQuickCheat = false;
        } else if (arr->Int(2) != 0) {
            Symbol screen = CurScreenName();
            if (mUIManager.CurrentScreen()) {
                if (screen.Null()) {
                    screen = CurRecordScreen();
                }
            }
            if (!screen.Null()) {
                static Symbol quick_cheat("quick_cheat");
                DataArrayPtr ptr(quick_cheat, arr->Array(3));
                AddRecord(screen, ptr);
            }
        }
    }
    return DATA_UNHANDLED;
}

void Automator::HandleMessage(Symbol msgType) {
    if (!mUIManager.InTransition()) {
        if (mRecord) {
            Symbol screenName = CurScreenName();
            if (!screenName.Null()) {
                DataArrayPtr ptr(msgType);
                AddRecord(screenName, ptr);
            }
        } else if (mScreenScripts) {
            AdvanceScript(msgType);
        }
    }
}

DataNode Automator::OnMsg(const UIComponentSelectMsg &msg) {
    HandleMessage(msg.Data()->Sym(1));
    return DATA_UNHANDLED;
}

DataNode Automator::OnMsg(const UIComponentScrollMsg &msg) {
    Symbol s = msg.Data()->Sym(1);
    HandleMessage(s);
    return DATA_UNHANDLED;
}

DataNode Automator::OnMsg(const UIComponentFocusChangeMsg &msg) {
    HandleMessage(msg.Data()->Sym(1));
    return DATA_UNHANDLED;
}

DataNode Automator::OnMsg(const UIScreenChangeMsg &msg) {
    HandleMessage(msg.Data()->Sym(1));
    return DATA_UNHANDLED;
}

void Automator::AddMessageType(Hmx::Object *obj, Symbol sym) {
    obj->AddSink(this, sym);
    mCustomMsgs.push_back(sym);
}

BEGIN_HANDLERS(Automator)
    HANDLE_EXPR(toggle_auto, DataNode(ToggleAuto()))
    HANDLE_EXPR(auto_script, DataNode((mScreenScripts && !mRecord) ? mAutoPath.c_str() : "OFF"))
    HANDLE_EXPR(toggle_record, DataNode(ToggleRecord()))
    HANDLE_EXPR(record_script, DataNode(mRecord ? mRecordPath.c_str() : "OFF"))
    HANDLE_ACTION(set_auto_script, (mAutoPath = _msg->Str(2)))
    HANDLE_ACTION(set_record_script, (mRecordPath = _msg->Str(2)))
    HANDLE_ACTION(add_message_type, (AddMessageType(_msg->GetObj(2), _msg->Sym(3))))
    if (!mScreenScripts && !mRecord)
        return DataNode(kDataUnhandled);
    HANDLE_MESSAGE(UITransitionCompleteMsg)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(UIComponentSelectMsg)
    HANDLE_MESSAGE(UIComponentScrollMsg)
    HANDLE_MESSAGE(UIComponentFocusChangeMsg)
    HANDLE_MESSAGE(UIScreenChangeMsg)
    HANDLE(cheat_invoked, OnCheatInvoked)
    _HANDLE_CHECKED(OnCustomMsg(Message(_msg)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

#pragma endregion Automator
