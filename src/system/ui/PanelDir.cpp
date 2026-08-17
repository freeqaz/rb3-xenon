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
END_HANDLERS

BEGIN_PROPSYNCS(PanelDir)
    SYNC_PROP(cam, mCam)
    SYNC_PROP(postprocs_before_draw, mCanEndWorld)
    SYNC_PROP(use_specified_cam, mUseSpecifiedCam)
    SYNC_PROP(focus_component, mFocusComponent)
    SYNC_PROP(owner_panel, mOwnerPanel)
#ifdef HX_NATIVE
    // DC3-era editor additions; RB3-360 retail's PanelDir chain ends at
    // `owner_panel`.  Arbitrated on RETAIL BYTES (lane CQ-3): the 564 B retail
    // body enumerates exactly cam / postprocs_before_draw / use_specified_cam /
    // focus_component / owner_panel -- five literals, ours emitted eight.
    // These three drive the DC3 edit-mode panel filters; native-only.
    {
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
#endif
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

// ── lane W35-CUSTOMIZE (2026-08-17): RB3-360 retail uses the rb3-Wii
// MUTABLE-GLOBAL rev dialect here, NOT DC3's BinStreamRev.  This is read off
// RETAIL BYTES, not off the oracle -- ?PreLoad@PanelDir@@ @0x82809520 and
// ?PostLoad@PanelDir@@ @0x828095C8:
//
//   * PreLoad splits the packed rev into two halves, stores them to a GLOBAL
//     (`sth r11, lbl_82E07938@l(r10)` / `sth r3, 0x4(r8)`), and then RE-PACKS
//     them with `rlwimi r3, r11, 16, 0, 15` before `bl PushRev`.  A re-pack is
//     meaningless unless the halves live in mutable globals -- DC3's
//     BinStreamRev would forward the already-packed `revs` unchanged.  That
//     single `rlwimi` is the decisive witness for the dialect.
//   * gRevAlt is at +0 and gRev at +4 (the `srwi` half is the one stored at
//     offset 0), 4 bytes apart => align(4) on a 2-byte type.
//   * PreLoad calls RndDir::PreLoad LAST (`subi r3,r31,0x5c; bl` is the final
//     call before the epilogue); the DC3 form calls it FIRST.
//   * PostLoad calls RndDir::PostLoad FIRST (`subi r3,r3,0x5c; bl` at
//     instructions 5/7, ahead of PopRev); the DC3 form calls PopRev first.
//   * retail has NO `rev < 7 && !mCam -> SetCurViewport(...)` block.  That is a
//     DC3-era addition: it is absent from rb3-Wii AND absent from retail, and
//     it accounted for a chunk of our base-only instructions.  This is oracle
//     failure mode 4 (the newer engine has statements RB3 never had).
//
// `!IsProxy()` is retained rather than rb3-Wii's `this == Dir()`: Dir.h defines
// `IsProxy() const { return this != Dir(); }`, so the two are the same inline
// expression and the spelling is not load-bearing.
//
// ── MEASURED RESULT (whole-binary A/B, from-dirty, same-ruler):
//     Δmatched=+2  Δhonest=+2  Δcode_bytes=+0  Δcode%=+0.000000pp
//     Δfuzzy=+0.003093pp, 1 unit improved (default/UISlider 68->70), 0 fell off.
//   PreLoad  17.18919 fuzzy / 19.75676 mpn  ->  99.86487 / 100.0
//   PostLoad 51.05608 fuzzy / 53.11215 mpn  ->  99.90654 / 100.0
//
// ⛔ BOTH ROWS LAND IN THE `mpn == 100, fuzzy < 100` POPULATION, SO THEY BUY +2
// FUNCTIONS AND EXACTLY ZERO BYTES.  Do not re-price this at 576 B off an
// objdiff "100.0% normalized / all instructions equal" reading -- that reading
// is instruction-level and CANNOT SEE relocation-name (`diff_arg`) charges.
// This lane pre-registered +576 B from exactly that mistake and measured +0.
// The `none` ruler DOES move +576 B, which is the signature of the class.
//
// The residual diff_scores are 5 and 10 == one and two PENALTY_REG_DIFF
// relocation-name charges.  All three are IDENTIFICATION, not body defects:
//   PreLoad  [30] tgt `?SyncProperty@RndSpline@@UAA_NAAVDataNode@@...`
//                 vs our `?PreLoad@RndDir@@UAAXAAVBinStream@@@Z`
//                 -- map[0x82406178] carries the RndSpline name and
//                 `?PreLoad@RndDir@@` is ABSENT from target_symbol_map.json.
//   PostLoad [83],[85] tgt `operator>>(BinStreamRev&, vector<FilePath>&)`
//                 vs our `operator>>(BinStream&, vector<FilePath>&)`
//                 -- BinStreamRev derives from BinStream, so the two template
//                 instantiations are prime ICF fold candidates.
// ⚠ Note the asymmetry that proves the mechanism: PostLoad's OTHER base call,
// `?PostLoad@RndDir@@`, resolves to an UNNAMED `fn_82404F80` and is FORGIVEN
// (placeholder targets cost nothing); only the NAMED one is charged.
// ⛔ NO ALIAS WAS INSTALLED.  An alias lifts the score BY CONSTRUCTION and the
// `none` control cannot catch a fabrication, so these need relocation-normalized
// body-identity proof (target names compared) before anyone adds them.  The
// characterisation above is the expensive half of that work; the adjudication
// is deliberately left undone rather than done on a guess.
// ⚠⚠ THE .bss ORDER OF THE TWO HALVES IS NOT CONTROLLABLE BY ORDINARY MEANS
// -- THREE LEVERS MEASURED INERT, so do not retry any of them:
//   * declaration order      -- byte-identical object either way
//   * assignment order       -- layout unmoved; it only flips the compute order
//                               (`clrlwi`/`srwi` swap) and cost PreLoad
//                               99.7% -> 97.4%
//   * renaming so the alt half sorts first (gAltRev -> gRevAlt) -- unmoved
// CAUSE, read out of the COFF symbol table rather than guessed: MSVC emits
// EXACTLY ONE .bss symbol for the pair (`gRev_PanelDir` at offset 0) and places
// the other half at an ANONYMOUS +4 addressed off that symbol.  Whichever half
// owns the symbol is therefore pinned to +0, and retail needs the ALT half
// there.  A single struct is the one spelling that makes both offsets explicit,
// and it reproduces retail's `lbl_82E07938` exactly: alt at +0, rev at +4.
// The LAYOUT is retail-evidenced; the STRUCT SPELLING is a choice.
static struct {
    unsigned short alt;
    unsigned short _pad0;
    unsigned short rev;
    unsigned short _pad1;
} gRevs;

void PanelDir::PreLoad(BinStream &bs) {
    int rev;
    bs >> rev;
    // ⚠ MEASURED NEGATIVE, do not retry: swapping these two assignments (to try
    // to move gRevs.alt to .bss +0) does NOT move the layout at all -- it only
    // flips the compute order (`clrlwi`/`srwi` swap) and cost PreLoad
    // 99.7% -> 97.4%.  Declaration order is equally inert (byte-identical obj).
    gRevs.rev = getHmxRev(rev);
    gRevs.alt = getAltRev(rev);
    BinStream::PushRev(packRevs(gRevs.alt, gRevs.rev), this);
    RndDir::PreLoad(bs);
}

void PanelDir::PostLoad(BinStream &bs) {
    RndDir::PostLoad(bs);
    int revs = BinStream::PopRev(this);
    gRevs.rev = getHmxRev(revs);
    gRevs.alt = getAltRev(revs);
    if (!IsProxy()) {
        if (gRevs.rev != 0) {
            bs >> mCam;
        }
        // ⚠ retail spells this DC3's way (`> 1 && < 3` -> `cmplwi 1; ble` +
        // `cmplwi 3; bge`), NOT rb3-Wii's `gRevs.rev == 2` (which emits
        // `cmplwi 2; bne`).  Measured, not assumed.
        if (gRevs.rev > 1 && gRevs.rev < 3) {
            Symbol s;
            bs >> s;
        }
    }
    // ⛔ THIS BLOCK IS IN RETAIL.  An earlier revision of this lane deleted it
    // because rb3-Wii has no such block -- that was WRONG, and retail bytes
    // refuted it: instructions [50]-[64] of ?PostLoad@PanelDir@@ are literally
    // `cmplwi r10,0x7 / bge` + `lwz r10,-0x50(r30)` (mCam) + `lis
    // lbl_82C721F0` (TheUI) + `lwz r11,0x40(r11)` (GetCam).  RB3-360 retail is a
    // HYBRID: rb3-Wii's rev DIALECT and call ordering, but DC3's BODY.  Do not
    // re-delete this on the strength of the Wii oracle.
    if (gRevs.rev < 7 && !mCam) {
        SetCurViewport(kNumViewports, TheUI->GetCam());
    }
    if (gRevs.rev > 3) {
        bs >> mCanEndWorld;
    }
    if (gRevs.rev > 4) {
        bs >> mBackFilenames >> mFrontFilenames;
    }
    if (gRevs.rev > 5) {
        bs >> mShowEditModePanels;
    }
    if (gRevs.rev > 7) {
        if (gLoadingProxyFromDisk) {
            bool b;
            bs >> b;
        } else {
            bs >> mUseSpecifiedCam;
        }
    }
    // ⛔ NO trailing SyncEditModePanels() here -- BOTH oracles have one and
    // RETAIL DOES NOT.  Retail's ?PostLoad@PanelDir@@ ends
    // `.L_8280976C: addi r1,r1,0x80 / b __restgprlr_28` with no call after the
    // `gRevs.rev > 7` block's `bl`, and the diff shows our `mr r3,r29 / bl
    // ?SyncEditModePanels@PanelDir@@AAAXXZ` as the ONLY two base-only
    // instructions in the function (it also costs 16 bytes of frame:
    // target `stwu -0x80` vs our `-0x90`).  Behavioural note: the sync still
    // runs on every other path that had it (SYNC_PROP_MODIFY(show_view_only_panels),
    // the PropSync setters, and the two other call sites in this file); only the
    // post-load call is dropped, which is what retail does.
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
    else if (newComponent != mFocusComponent) {
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
    bool overloaded = TheUI->OverloadHorizontalNav(act, btn, controller_type);
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


// COMDAT-scatter owner-TU includes (sw scatter-scan): retail linker
// interleaved these owners' COMDATs into this TU's .text span.
#define gRev gRev_PropSync
#define gAltRev gAltRev_PropSync
#include "obj/PropSync.cpp"
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/PanelDir <- ui/UIListWidget.cpp)
#define gRev gRev_UIListWidget
#define gAltRev gAltRev_UIListWidget
#include "ui/UIListWidget.cpp"
#undef gRev
#undef gAltRev

// laneAE: retail parked the ObjList<BandCamShot::Target> assignment COMDATs in
// this unit (0x824D1070 list::operator=, ObjList::operator=).  Reference them so
// our obj emits the same COMDATs here.
#include "bandobj/BandCamShot.h"
void sw_BandCamShotTargetListAssign(
    ObjList<BandCamShot::Target> &a, const ObjList<BandCamShot::Target> &b
) {
    a = b;
}
