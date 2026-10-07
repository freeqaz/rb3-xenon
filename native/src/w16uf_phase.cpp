// rb3-xenon native -- W16-UF: the rest of lever 2's in-scope unentered rows.
//
// docs/decomp/CAMPAIGN_STATE_2026-10-07c.md section 6, lever 2, continued from
// W16-TW (docs/decomp/W16TW_UNENTERED_ROWS_SHIPPED_DATA_GATES_2026-10-07.md):
// the 40 rows below TW's #22 that native compiles but no target entered, plus
// TW's five deferred rows. Each row taken is driven here, in rb3-render's
// default mode, on shipped data where shipped data reaches it.
//
// THE REFERENCE. Expected values come from the shipped files read directly, or
// from the retail body read off the retail asm and written down here, never
// from the code under test. A fixture gate checks each fixture first, so a
// later failure points at the function under test. The doc
// (docs/decomp/W16UF_UNENTERED_ROWS_SHIPPED_DATA_GATES_2026-10-07.md) records,
// per row, the gate or the reason it has none.

#include "bandobj/BandCamShot.h"
#include "bandobj/CharKeyHandMidi.h"
#include "beatmatch/JoypadController.h"
#include "game/BandUser.h"
#include "meta_band/AppLabel.h"
#include "meta_band/StoreMainPanel.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "os/File.h"
#include "os/Joypad.h"
#include "os/JoypadClient.h"
#include "os/JoypadMsgs.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "rndobj/Env.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"

#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <thread>
#include <vector>

extern DataArray *gSystemConfig;

// W16-TW's scripted pad back end (native/src/w16tw_phase.cpp): strong
// ReadSingleJoypad / requestBreedWrite that stay inert unless gScripted is set.
namespace w16tw {
struct PadFrame {
    int type = 0;
    unsigned int buttons = 0;
    signed char lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;
};
extern PadFrame gFrame[4];
extern bool gScripted;
}

typedef void (*GateFn)(const char *, bool, const char *);

namespace {

GateFn gGate = nullptr;
char gBuf[1024];
int gRan = 0;

void Gate(const char *name, bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Gate(const char *name, bool ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gBuf, sizeof(gBuf), fmt, ap);
    va_end(ap);
    gGate(name, ok, gBuf);
    gRan++;
}

template <class T> T *FindByName(ObjectDir *dir, const char *name) {
    // Walk the loaded dir ourselves (not ObjectDir::Find, which some rows call).
    if (!dir)
        return nullptr;
    for (ObjDirItr<T> it(dir, true); it != nullptr; ++it)
        if (strcmp(it->Name(), name) == 0)
            return it;
    return nullptr;
}

// ============================================ CharKeyHandMidi fingering ==
// Retail fn_822CF888, read off the asm:
//   setTo == lastKey                      -> lastFinger
//   lastKey == 0 or lastFinger == 5       -> 2 (middle)
//   d = |setTo - lastKey|; the finger moves toward the pinky (4) when
//   (setTo > lastKey) == mIsRightHand (lbz 0x9c), toward the thumb (0) otherwise:
//     at the end already (4 / 0)         -> stays there
//     d <= 2 -> 1 step, d <= 5 -> 2, d <= 7 -> 3, else straight to the end
//     a step past the end clamps to it.
int RefPreferredFinger(int setTo, int lastKey, int lastFinger, bool right) {
    if (setTo == lastKey)
        return lastFinger;
    if (lastKey == 0 || lastFinger == 5)
        return 2;
    int d = std::abs(setTo - lastKey);
    bool towardPinky = (setTo > lastKey) == right;
    int step = d <= 2 ? 1 : d <= 5 ? 2 : d <= 7 ? 3 : 99;
    if (towardPinky) {
        if (lastFinger == 4 || step == 99)
            return 4;
        int f = lastFinger + step;
        return f > 4 ? 4 : f;
    }
    if (lastFinger == 0 || step == 99)
        return 0;
    int f = lastFinger - step;
    return f < 0 ? 0 : f;
}

ObjDirPtr<ObjectDir> gKeyboard;

void KeyHandFingerChecks() {
    printf("\n=== W16-UF: CharKeyHandMidi::FindPreferredFinger on the shipped keyboard rig ===\n");
    gKeyboard.LoadFile(FilePath("char/main/rigging/gen/keyboard.milo_xbox"), false, false,
                       kLoadFront, false);
    CharKeyHandMidi *hand[2] = { FindByName<CharKeyHandMidi>(gKeyboard.Ptr(), "left_hand.keyhand"),
                                 FindByName<CharKeyHandMidi>(gKeyboard.Ptr(), "right_hand.keyhand") };
    bool fixtureOk = hand[0] && hand[1] && !hand[0]->mIsRightHand && hand[1]->mIsRightHand;
    Gate("uf-keyhand-fixture", fixtureOk,
         "keyboard.milo: left_hand.keyhand %s (is_right_hand %d), right_hand.keyhand %s "
         "(is_right_hand %d)",
         hand[0] ? "found" : "MISSING", hand[0] ? (int)hand[0]->mIsRightHand : -1,
         hand[1] ? "found" : "MISSING", hand[1] ? (int)hand[1]->mIsRightHand : -1);
    if (!fixtureOk)
        return;
    // Every key Poll addresses (0 = none, 1..0x19) as the target and as the last
    // key, every finger 0..5 (5 = none), both hands.
    int cases = 0, wrong = 0;
    std::string first;
    for (int h = 0; h < 2; h++)
        for (int setTo = 0; setTo <= 0x19; setTo++)
            for (int last = 0; last <= 0x19; last++)
                for (int f = 0; f <= 5; f++) {
                    int got = hand[h]->FindPreferredFinger(
                        (CharKeyHandMidi::KeyboardKey)setTo, (CharKeyHandMidi::KeyboardKey)last,
                        (CharIKFingers::FingerNum)f);
                    int want = RefPreferredFinger(setTo, last, f, h == 1);
                    cases++;
                    if (got != want) {
                        if (!wrong)
                            first = MakeString(" first: %s hand, key %d after key %d with finger "
                                               "%d -> %d, want %d",
                                               h ? "right" : "left", setTo, last, f, got, want);
                        wrong++;
                    }
                }
    Gate("uf-keyhand-finger", wrong == 0, "%d cases (2 hands x 26 x 26 keys x 6 fingers): %d wrong%s",
         cases, wrong, first.c_str());
}

// =================================================== JoypadController ==
// Retail fn_8279B390 (IsCymbal), read off the asm. b(n) = button n held, with
// n == kPad_NumButtons (24) never held (the slw lands on bit 24, which no pad
// reports). cym/pad are the controller's cymbal_shift_button and
// pad_shift_button from the joypad config:
//   no local user -> false
//   2: b(cym) && (b(DUp) || !b(pad))
//   3: b(cym) && (b(DDown) || !b(pad))
//   4: cym == 24 ? b(R1) : b(cym) && !((b(DUp) || b(DDown)) && b(pad))
//   any other slot -> false
bool RefIsCymbal(int slot, unsigned int btn, int cym, int pad) {
    auto b = [&](int n) { return n >= 0 && n < 24 && (btn >> n) & 1; };
    switch (slot) {
    case 2:
        return b(cym) && (b(kPad_DUp) || !b(pad));
    case 3:
        return b(cym) && (b(kPad_DDown) || !b(pad));
    case 4:
        if (cym == kPad_NumButtons)
            return b(kPad_R1);
        return b(cym) && !((b(kPad_DUp) || b(kPad_DDown)) && b(pad));
    default:
        return false;
    }
}

// The joypad config's controllers entry whose (detect (type N)) is padType.
DataArray *JoyEntryFor(DataArray *joy, int padType) {
    DataArray *ctl = joy ? joy->FindArray("controllers", false) : nullptr;
    for (int i = 1; ctl && i < ctl->Size(); i++) {
        if (ctl->Type(i) != kDataArray)
            continue;
        DataArray *e = ctl->Array(i);
        DataArray *det = e->FindArray("detect", false);
        DataArray *ty = det ? det->FindArray("type", false) : nullptr;
        if (ty && ty->Size() > 1 && ty->Int(1) == padType)
            return e;
    }
    return nullptr;
}

int EntryInt(DataArray *e, const char *key, int def) {
    DataArray *a = e ? e->FindArray(key, false) : nullptr;
    return a && a->Size() > 1 ? a->Int(1) : def;
}

void ScriptPad(int type, unsigned int buttons) {
    w16tw::gFrame[0] = w16tw::PadFrame();
    w16tw::gFrame[0].type = type;
    w16tw::gFrame[0].buttons = buttons;
    JoypadPollCommon();
}

void CymbalChecks() {
    printf("\n=== W16-UF: JoypadController::IsCymbal over the shipped controller config ===\n");
    DataArray *joy = gSystemConfig->FindArray("joypad", false);
    DataArray *bmc = DataReadFile("config/beatmatch_controller.dta", true);
    // Two shipped Xbox pad types: the drum kit (cymbal and pad shift buttons) and
    // the guitar (whose entry carries NO_DRUM_SHIFT_BUTTONS, i.e. 24 / 24).
    struct Kit {
        int type;
        const char *bmEntry;
    } kits[] = { { kJoypadXboxDrums, "hx_drums_xbox" },
                 { kJoypadXboxHxGuitarRb2, "joypad" } };
    DataArray *ent[2] = { JoyEntryFor(joy, kits[0].type), JoyEntryFor(joy, kits[1].type) };
    int cym[2], pad[2];
    for (int k = 0; k < 2; k++) {
        cym[k] = EntryInt(ent[k], "cymbal_shift_button", -1);
        pad[k] = EntryInt(ent[k], "pad_shift_button", -1);
    }
    bool fixtureOk = joy && bmc && ent[0] && ent[1] && cym[0] >= 0 && cym[0] < 24 && pad[0] >= 0
        && pad[0] < 24 && cym[1] == kPad_NumButtons && bmc->FindArray(kits[0].bmEntry, false)
        && bmc->FindArray(kits[1].bmEntry, false);
    Gate("uf-cymbal-fixture", fixtureOk,
         "joypad config: drum type %d -> '%s' (cymbal_shift_button %d, pad_shift_button %d); "
         "guitar type %d -> '%s' (cymbal_shift_button %d); beatmatch_controller.dta %s",
         kits[0].type, ent[0] ? ent[0]->Sym(0).Str() : "MISSING", cym[0], pad[0], kits[1].type,
         ent[1] ? ent[1]->Sym(0).Str() : "MISSING", cym[1], bmc ? "read" : "MISSING");
    if (!fixtureOk) {
        if (bmc)
            bmc->Release();
        return;
    }
    // The joypad library as retail boot brings it up (JoypadInit ->
    // JoypadInitCommon over SystemConfig joypad), so a scripted pad connects
    // with its controller type.
    JoypadData savedPad = *JoypadGetPadData(0);
    JoypadInitCommon(joy);
    NullLocalBandUser *user = BandUser::NewNullLocalBandUser();
    LocalUser *savedUser = JoypadGetUserFromPadNum(0);
    AssociateUserAndPad(user, 0);
    w16tw::gScripted = true;
    int cases = 0, wrong = 0, userWrong = 0;
    std::string first, ctorWhy;
    for (int k = 0; k < 2; k++) {
        ScriptPad(0, 0); // unplug, so the next poll detects the new type
        ScriptPad(kits[k].type, 0); // connect the pad as this controller type
        JoypadController *c = new JoypadController(
            user, bmc->FindArray(kits[k].bmEntry, false), nullptr, true, false
        );
        if (c->mLocalUser != user || c->mCymbalShiftButton != cym[k]
            || c->mPadShiftButton != pad[k]) {
            if (!userWrong)
                ctorWhy = MakeString(" (%s: pad type '%s', cymbal %d pad %d, want %d %d)",
                                     kits[k].bmEntry, JoypadControllerTypePadNum(0).Str(),
                                     c->mCymbalShiftButton, c->mPadShiftButton, cym[k], pad[k]);
            userWrong++;
        }
        // Every subset of the five buttons the rule reads, plus an unrelated one.
        int bits[5] = { cym[k] < 24 ? cym[k] : kPad_R1, pad[k] < 24 ? pad[k] : kPad_R3,
                        kPad_DUp, kPad_DDown, kPad_R1 };
        for (int m = 0; m < 64; m++) {
            unsigned int btn = 0;
            for (int i = 0; i < 5; i++)
                if (m >> i & 1)
                    btn |= 1u << bits[i];
            if (m & 32)
                btn |= 1u << kPad_X;
            JoypadGetPadData(0)->mButtons = btn;
            for (int slot = 0; slot <= 5; slot++) {
                bool got = c->IsCymbal(slot);
                bool want = RefIsCymbal(slot, btn, cym[k], pad[k]);
                cases++;
                if (got != want) {
                    if (!wrong)
                        first = MakeString(" first: %s, buttons 0x%x, slot %d -> %d, want %d",
                                           kits[k].bmEntry, btn, slot, got, want);
                    wrong++;
                }
            }
        }
        delete c;
    }
    // A controller with no local user (a remote player) reads false everywhere.
    int remoteWrong = 0;
    {
        JoypadGetPadData(0)->mButtons = 0xFFFFFF;
        JoypadController *c = new JoypadController(
            user, bmc->FindArray(kits[0].bmEntry, false), nullptr, true, false
        );
        c->mLocalUser = nullptr; // the ctor's remote arm stores exactly this
        for (int slot = 0; slot <= 5; slot++)
            remoteWrong += c->IsCymbal(slot);
        delete c;
    }
    JoypadGetPadData(0)->mButtons = 0;
    ScriptPad(0, 0); // unplug
    w16tw::gScripted = false;
    AssociateUserAndPad(savedUser, 0);
    JoypadTerminateCommon();
    *JoypadGetPadData(0) = savedPad;
    bmc->Release();
    Gate("uf-cymbal", wrong == 0 && userWrong == 0 && remoteWrong == 0,
         "%d cases (2 controller types x 64 button sets x slots 0..5): %d wrong; ctor read the "
         "config's shift buttons: %s%s; no local user: %d slot(s) read true (want 0)%s",
         cases, wrong, userWrong ? "NO" : "yes", ctorWhy.c_str(), remoteWrong, first.c_str());
}

// ======================================================= JoypadClient ==
// Retail fn_82529270, read off the asm: for each of the 4 pads in order, when
// ThePlatformMgr's guide is showing (byte 0x24) both of that pad's repeat
// timers are Reset (and so stop); otherwise JoypadRepeat::Poll(mHoldMs,
// mRepeatMs, mSink, pad). The hold and repeat times are the config's
// (joypad hold_ms / repeat_ms; the game's config/joypad.dta wins the merge).
struct RepeatSink : public Hmx::Object {
    struct Ev {
        int btn, act, pad;
    };
    std::vector<Ev> ev;
    DataNode Handle(DataArray *m, bool) override {
        if (m && m->Size() >= 6 && m->Type(1) == kDataSymbol
            && strcmp(m->Sym(1).Str(), "button_down") == 0)
            ev.push_back({ m->Int(3), m->Int(4), m->Int(5) });
        return DataNode(kDataUnhandled, 0);
    }
};

void SleepMs(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

void JoypadClientChecks() {
    printf("\n=== W16-UF: JoypadClient::Poll with the shipped hold/repeat times ===\n");
    DataArray *game = DataReadFile("config/joypad.dta", true);
    float wantHold = -1, wantRepeat = -1;
    if (game) {
        game->FindData("hold_ms", wantHold, false);
        game->FindData("repeat_ms", wantRepeat, false);
        game->Release();
    }
    RepeatSink *sink = new RepeatSink();
    JoypadClient *client = new JoypadClient(sink);
    bool fixtureOk = wantHold > 0 && wantRepeat > 0 && client->mHoldMs == wantHold
        && client->mRepeatMs == wantRepeat && wantRepeat < wantHold;
    Gate("uf-repeat-fixture", fixtureOk,
         "config/joypad.dta: hold_ms %g, repeat_ms %g; the client took hold %g, repeat %g", wantHold,
         wantRepeat, client->mHoldMs, client->mRepeatMs);
    if (!fixtureOk) {
        delete client;
        delete sink;
        return;
    }
    const int hold = (int)wantHold, rep = (int)wantRepeat;
    bool savedGuide = ThePlatformMgr.mGuideShowing;
    ThePlatformMgr.mGuideShowing = false;
    // Two held buttons, on pads 2 and 0 (started in that order).
    client->mRepeats[2].Start(kPad_DDown, kAction_Down, 2);
    client->mRepeats[0].Start(kPad_DUp, kAction_Up, 0);
    std::string log;
    auto step = [&](const char *what, size_t wantN, bool ordered) {
        sink->ev.clear();
        client->PollClient();
        bool ok = sink->ev.size() == wantN;
        if (ok && ordered && wantN == 2)
            ok = sink->ev[0].pad == 0 && sink->ev[0].btn == kPad_DUp
                && sink->ev[0].act == kAction_Up && sink->ev[1].pad == 2
                && sink->ev[1].btn == kPad_DDown && sink->ev[1].act == kAction_Down;
        log += MakeString("%s%s %d%s", log.empty() ? "" : "; ", what, (int)sink->ev.size(),
                          ok ? "" : " WRONG");
        return ok;
    };
    int bad = 0;
    bad += !step("at once", 0, false);
    SleepMs(hold + 60);
    bad += !step("after hold", 2, true);
    bad += !step("again at once", 0, false);
    SleepMs(rep + 30);
    bad += !step("after repeat", 2, true);
    ThePlatformMgr.mGuideShowing = true;
    SleepMs(rep + 30);
    bad += !step("guide up", 0, false);
    int running = 0;
    for (int i = 0; i < 4; i++)
        running += client->mRepeats[i].mHoldTimer.Running() + client->mRepeats[i].mRepeatTimer.Running();
    ThePlatformMgr.mGuideShowing = false;
    SleepMs(hold + 60);
    bad += !step("guide down, after hold", 0, false);
    ThePlatformMgr.mGuideShowing = savedGuide;
    delete client;
    delete sink;
    Gate("uf-repeat", bad == 0 && running == 0,
         "two held buttons (pads 0 and 2), repeats seen: %s; timers left running after the guide "
         "poll: %d (want 0)",
         log.c_str(), running);
}

// =========================================== BandCamShot::Target copy ==
// Retail fn_822B1140 copies every field: the target symbol, the 0x40-byte
// transform, the anim group, fast-forward, forward event, the environ pointer
// (ObjPtr assignment), the 3-bit force LOD and the six flag bits.
bool SameTarget(const BandCamShot::Target &a, const BandCamShot::Target &b, std::string &why) {
    why.clear();
    if (a.mTarget != b.mTarget)
        why += " target";
    if (memcmp(&a.mXfm, &b.mXfm, sizeof(Transform)) != 0)
        why += " xfm";
    if (a.mAnimGroup != b.mAnimGroup)
        why += " anim_group";
    if (a.mFastForward != b.mFastForward)
        why += " fast_forward";
    if (a.mForwardEvent != b.mForwardEvent)
        why += " forward_event";
    if (a.mEnvOverride.Ptr() != b.mEnvOverride.Ptr())
        why += " env_override";
    if (a.mForceLod != b.mForceLod)
        why += " force_lod";
    if (a.mTeleport != b.mTeleport || a.mReturn != b.mReturn || a.mSelfShadow != b.mSelfShadow
        || a.unk1 != b.unk1 || a.unk2 != b.unk2 || a.mHide != b.mHide)
        why += " flags";
    return why.empty();
}

ObjDirPtr<ObjectDir> gPortrait;

void TargetCopyChecks() {
    printf("\n=== W16-UF: BandCamShot::Target::operator= on the shipped closet portrait shot ===\n");
    gPortrait.LoadFile(FilePath("world/meta/closet/gen/portrait_space.milo_xbox"), false, false,
                       kLoadFront, false);
    BandCamShot *shot = FindByName<BandCamShot>(gPortrait.Ptr(), "portrait.shot");
    int n = shot ? (int)shot->mTargets.size() : -1;
    Gate("uf-target-fixture", shot && n > 0, "portrait_space.milo: portrait.shot %s, %d target(s)",
         shot ? "found" : "MISSING", n);
    if (!shot || n <= 0)
        return;
    RndEnviron *env = FindByName<RndEnviron>(gPortrait.Ptr(), "portrait.env");
    if (!env)
        for (ObjDirItr<RndEnviron> it(gPortrait.Ptr(), true); it != nullptr; ++it) {
            env = it;
            break;
        }
    int bad = 0, copies = 0;
    std::string why, first;
    for (ObjList<BandCamShot::Target>::iterator it = shot->mTargets.begin();
         it != shot->mTargets.end(); ++it) {
        // Into a fresh target, and into one holding the opposite of every field.
        BandCamShot::Target fresh(shot);
        fresh = *it;
        copies++;
        if (!SameTarget(fresh, *it, why)) {
            bad++;
            if (first.empty())
                first = MakeString(" first: '%s' into a fresh target:%s", it->mTarget.Str(), why.c_str());
        }
        BandCamShot::Target other(shot);
        other.mTarget = "w16uf_other";
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 3; c++)
                ((float *)&other.mXfm.m)[r * 3 + c] = 7.0f + r + c;
        other.mXfm.v.Set(-3, -4, -5);
        other.mAnimGroup = "w16uf_group";
        other.mFastForward = it->mFastForward + 9.5f;
        other.mForwardEvent = "w16uf_event";
        other.mEnvOverride = it->mEnvOverride.Ptr() ? nullptr : env;
        other.mForceLod = it->mForceLod == 2 ? -3 : 2;
        other.mTeleport = !it->mTeleport;
        other.mReturn = !it->mReturn;
        other.mSelfShadow = !it->mSelfShadow;
        other.unk1 = !it->unk1;
        other.unk2 = !it->unk2;
        other.mHide = !it->mHide;
        other = *it;
        copies++;
        if (!SameTarget(other, *it, why)) {
            bad++;
            if (first.empty())
                first = MakeString(" first: '%s' over an all-different target:%s", it->mTarget.Str(), why.c_str());
        }
    }
    Gate("uf-target-copy", bad == 0 && copies == 2 * n,
         "%d copies of the shot's %d shipped target(s) (fresh, and over an all-different target): "
         "%d differ%s",
         copies, n, bad, first.c_str());
}

// ======================================== StoreMainPanel::FinishLoad ==
// The panel as ui/store/store.dta declares it, {new StoreMainPanel
// store_banner_panel (file "../store/store_main.milo") (display_rate 4.0)
// (crossfade_duration 1.0)}, loaded through UIPanel::Load and the loader, then
// finished. Retail fn_8263AB70: base FinishLoad; mNoneTex =
// cover_art_none.tex; for i in 1..6 mats cover_art_0i.mat, diffuse = mNoneTex,
// dirty |= 2; three labels text_line_1..3.lbl; album_scroll.anim played from
// its end frame to its end frame; mCurrentEntry = -1; label 1 and 2 tokens
// cleared (gNullStr); display_rate and crossfade_duration from the type def.
DataArray *FindNewCommand(DataArray *a, const char *cls) {
    for (int i = 0; a && i < a->Size(); i++) {
        if (a->Type(i) != kDataCommand)
            continue;
        DataArray *c = a->Node(i).UncheckedArray();
        if (c->Size() > 2 && c->Type(0) == kDataSymbol && strcmp(c->Sym(0).Str(), "new") == 0
            && c->Type(1) == kDataSymbol && strcmp(c->Sym(1).Str(), cls) == 0)
            return c;
    }
    return nullptr;
}

void StoreMainPanelChecks() {
    printf("\n=== W16-UF: StoreMainPanel::FinishLoad over the shipped store banner panel ===\n");
    DataArray *script = DataReadFile("ui/store/store.dta", true);
    DataArray *def = FindNewCommand(script, "StoreMainPanel");
    float rate = -1, xfade = -1;
    if (def) {
        DataArray *a = def->FindArray("display_rate", false);
        DataArray *b = def->FindArray("crossfade_duration", false);
        rate = a ? a->Float(1) : -1;
        xfade = b ? b->Float(1) : -1;
    }
    StoreMainPanel *p = new StoreMainPanel();
    if (def) {
        p->SetName(def->Str(2), ObjectDir::Main());
        p->SetTypeDef(def);
    }
    if (def)
        p->UIPanel::Load(); // StoreMainPanel::Load also subscribes to the store panel
    int pumps = 0;
    while (def && !p->UIPanel::IsLoaded() && pumps < 2000) {
        TheLoadMgr.Poll();
        p->PollForLoading();
        pumps++;
    }
    ObjectDir *dir = def ? p->DataDir() : nullptr;
    RndTex *none = FindByName<RndTex>(dir, "cover_art_none.tex");
    RndMat *mats[6];
    int matsFound = 0;
    for (int i = 0; i < 6; i++) {
        mats[i] = FindByName<RndMat>(dir, MakeString("cover_art_%02d.mat", i + 1));
        matsFound += mats[i] != nullptr;
    }
    AppLabel *lbl[3];
    for (int i = 0; i < 3; i++)
        lbl[i] = FindByName<AppLabel>(dir, MakeString("text_line_%d.lbl", i + 1));
    RndAnimatable *scroll = FindByName<RndAnimatable>(dir, "album_scroll.anim");
    bool fixtureOk = def && rate > 0 && xfade > 0 && dir && none && matsFound == 6 && lbl[0]
        && lbl[1] && lbl[2] && scroll;
    Gate("uf-storemain-fixture", fixtureOk,
         "store.dta: %s (display_rate %g, crossfade_duration %g); store_main.milo loaded in %d "
         "pump(s): %s; cover_art_none.tex %s, %d/6 cover mats, labels %d%d%d, album_scroll.anim %s",
         def ? "{new StoreMainPanel ...} found" : "NO StoreMainPanel", rate, xfade, pumps,
         dir ? dir->Name() : "NOT LOADED", none ? "found" : "MISSING", matsFound, lbl[0] != 0,
         lbl[1] != 0, lbl[2] != 0, scroll ? "found" : "MISSING");
    if (!fixtureOk)
        return;
    // Pre-state that FinishLoad must overwrite.
    Symbol tok3 = lbl[2]->GetTextToken();
    for (int i = 0; i < 6; i++)
        mats[i]->mDirty &= ~2;
    p->mCurrentEntry = 7;
    p->FinishLoad();
    int bad = 0;
    std::string why;
    auto chk = [&](bool ok, const char *what) {
        if (!ok) {
            bad++;
            why += MakeString(" %s", what);
        }
    };
    chk(p->mNoneTex == none, "none_tex");
    for (int i = 0; i < 6; i++) {
        chk(p->mCoverArtMats[i] == mats[i], MakeString("mat%d", i + 1));
        chk(mats[i]->GetDiffuseTex() == none, MakeString("mat%d_diffuse", i + 1));
        chk((mats[i]->mDirty & 2) != 0, MakeString("mat%d_dirty", i + 1));
    }
    chk(p->mLabel1 == lbl[0] && p->mLabel2 == lbl[1] && p->mLabel3 == lbl[2], "labels");
    chk(lbl[0]->GetTextToken() == gNullStr && lbl[1]->GetTextToken() == gNullStr, "label_tokens");
    chk(lbl[2]->GetTextToken() == tok3, "label3_untouched");
    chk(p->mScrollAnim == scroll, "scroll_anim");
    chk(p->mCurrentEntry == -1, "current_entry");
    chk(p->mDisplayRate == rate && p->mCrossfadeDuration == xfade, "rates");
    chk(p->GetState() == UIPanel::kDown, "state");
    Gate("uf-storemain", bad == 0,
         "after FinishLoad: none tex, 6 cover mats (diffuse none, dirty bit 2), 3 labels (1 and "
         "2 cleared, 3 untouched), scroll anim, entry -1, display_rate %g, crossfade %g, state "
         "down: %d wrong%s",
         p->mDisplayRate, p->mCrossfadeDuration, bad, why.c_str());
}

} // namespace

int RunW16UFPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== W16-UF phase: lever 2's remaining in-scope rows on shipped data ===\n");
    // The factories retail boot registers before any of these milos load:
    // BandInit (CharKeyHandMidi, BandCamShot) and the meta init, which
    // registers AppLabel under its base name "BandLabel" (MetaPanel.cpp). rb3-render
    // registers only BandInit's track subset, so the phase adds these and puts
    // the table back afterwards.
    std::map<Symbol, ObjectFunc *> savedFactories = Hmx::Object::sFactories;
    CharKeyHandMidi::Init();
    BandCamShot::Init();
    REGISTER_OBJ_FACTORY(AppLabel);
    KeyHandFingerChecks();
    CymbalChecks();
    JoypadClientChecks();
    TargetCopyChecks();
    StoreMainPanelChecks();
    Hmx::Object::sFactories = savedFactories;
    return gRan;
}
