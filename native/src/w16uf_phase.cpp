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
#include "bandobj/BandCharacter.h"
#include "bandobj/BandCrowdMeter.h"
#include "bandobj/BandFaceDeform.h"
#include "bandobj/BandTrack.h"
#include "bandobj/BandWardrobe.h"
#include "char/CharDriver.h"
#include "char/CharWeightable.h"
#include "char/Waypoint.h"
#include "bandobj/CrowdMeterIcon.h"
#include "bandobj/GemTrackDir.h"
#include "bandobj/TrackPanelDir.h"
#include "bandobj/UnisonIcon.h"
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
#include "obj/Task.h"
#include "rndobj/EventTrigger.h"
#include "rndobj/Env.h"
#include "rndobj/Group.h"
#include "rndobj/MeshAnim.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include "utl/FilePath.h"
#include "utl/BeatMap.h"
#include "utl/Loader.h"
#include "utl/TempoMap.h"

#include <chrono>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
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


// ======================================================= wave 2 helpers ==
// A deterministic generator so every run drives the same cases.
struct Lcg {
    unsigned int s;
    explicit Lcg(unsigned int seed) : s(seed) {}
    unsigned int Next() {
        s = s * 1664525u + 1013904223u;
        return s >> 8;
    }
    int Range(int n) { return (int)(Next() % (unsigned int)n); }
};

float Dist3(const Vector3 &a, const Vector3 &b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// ============================================== CharKeyHandMidi::Poll ==
// Retail fn_822D1980, read off the asm.
//   Layout (when the reset flag is set): F = first spot world xfm, S = second.
//     keyDir = S.v - F.v, keyDist = |keyDir|, keyDir normalized;
//     up = normalize(F.m.z); tipOff = -F.m.y;
//     cur = F.v + up * -0.4;
//     white = keyDir * (keyDist * 0.0714285746f)      (1/14)
//     black = (tipOff * 2 + keyDir * (keyDist * 0.0357142873f)) + up * 0.5
//     key 1: pos cur, tip cur + tipOff; keys 2..25: a black key
//     (2 4 7 9 11 14 16 19 21 23) is at cur + black, a white key first moves
//     cur += white and sits at cur; its tip is pos + tipOff. Flag cleared.
//   Fingering: now = real seconds; now < last -> every finger 0..4 released
//   (each release adds one to the free count, keyed or not), keys cleared,
//   last = now, done. Otherwise last = now; more than 5 keys -> cleared, done;
//   sort ascending (right hand) / descending (left); none -> done. With all
//   five fingers free: 1 key -> preferred finger; 2 -> first key's preferred
//   finger capped at the middle, second key's preferred; 3 -> thumb, then the
//   preferred finger with pinky turned to ring, then preferred; 4 -> fingers
//   0 1 2 4; 5 -> 0..4. Otherwise every key takes its preferred finger
//   (computed from the hand's last key/finger, not updated between keys)
//   unless one is "none", already holds a key, or repeats, in which case each
//   key goes to the first free of fingers 1..4, then the thumb. Keys cleared.
//   KeyFinger(f, key): f outside 0..4 or key outside 1..25 or key already
//   held by a finger -> nothing; else finger f holds key, last finger = f,
//   last key = key, one fewer free finger.
struct RefHand {
    int fing[5] = { 0, 0, 0, 0, 0 };
    int lastKey = 0, lastFinger = 5, freeCount = 5;
    bool right = true;
    bool Key(int f, int key) {
        if (f < 0 || f >= 5 || key <= 0 || key > 0x19)
            return false;
        for (int i = 0; i < 5; i++)
            if (fing[i] == key)
                return false;
        fing[f] = key;
        lastFinger = f;
        lastKey = key;
        freeCount--;
        return true;
    }
    void Unkey(int f) {
        fing[f] = 0;
        freeCount++;
    }
    int Pref(int key) { return RefPreferredFinger(key, lastKey, lastFinger, right); }
    void DefaultSelect(int key) {
        for (int i = 1; i < 5; i++)
            if (fing[i] == 0) {
                Key(i, key);
                return;
            }
        if (fing[0] == 0)
            Key(0, key);
    }
    void Poll(std::vector<int> keys, bool timeWentBack) {
        if (timeWentBack) {
            for (int i = 0; i < 5; i++)
                Unkey(i);
            return;
        }
        int n = keys.size();
        if (n > 5 || n <= 0)
            return;
        if (right)
            std::sort(keys.begin(), keys.end());
        else
            std::sort(keys.begin(), keys.end(), std::greater<int>());
        if (freeCount == 5) {
            switch (n) {
            case 1: {
                int f = Pref(keys[0]);
                if (f != 5)
                    Key(f, keys[0]);
                break;
            }
            case 2: {
                int f0 = Pref(keys[0]);
                if (f0 > 2)
                    f0 = 2;
                Key(f0, keys[0]);
                Key(Pref(keys[1]), keys[1]);
                break;
            }
            case 3: {
                Key(0, keys[0]);
                int f1 = Pref(keys[1]);
                Key(f1 == 4 ? 3 : f1, keys[1]);
                Key(Pref(keys[2]), keys[2]);
                break;
            }
            case 4:
                Key(0, keys[0]);
                Key(1, keys[1]);
                Key(2, keys[2]);
                Key(4, keys[3]);
                break;
            default:
                for (int i = 0; i < 5; i++)
                    Key(i, keys[i]);
                break;
            }
            return;
        }
        std::vector<int> used;
        bool ok = true;
        for (int i = 0; i < n; i++) {
            int f = RefPreferredFinger(keys[i], lastKey, lastFinger, right);
            if (f == 5 || fing[f] != 0
                || std::find(used.begin(), used.end(), f) != used.end()) {
                ok = false;
                break;
            }
            used.push_back(f);
        }
        if (ok)
            for (int i = 0; i < n; i++)
                Key(used[i], keys[i]);
        else
            for (int i = 0; i < n; i++)
                DefaultSelect(keys[i]);
    }
};

bool IsRefBlackKey(int k) {
    static const int b[] = { 2, 4, 7, 9, 11, 14, 16, 19, 21, 23 };
    for (int x : b)
        if (x == k)
            return true;
    return false;
}

void KeyHandPollChecks() {
    printf("\n=== W16-UF: CharKeyHandMidi::Poll on the shipped keyboard rig ===\n");
    CharKeyHandMidi *hand[2] = { FindByName<CharKeyHandMidi>(gKeyboard.Ptr(), "left_hand.keyhand"),
                                 FindByName<CharKeyHandMidi>(gKeyboard.Ptr(), "right_hand.keyhand") };
    bool fixtureOk = hand[0] && hand[1];
    for (int h = 0; h < 2 && fixtureOk; h++)
        fixtureOk = hand[h]->mFirstSpot && hand[h]->mSecondSpot && hand[h]->mIKObject
            && hand[h]->unk4c.size() == 26 && hand[h]->unk54.size() == 26;
    Gate("uf-keyhand-poll-fixture", fixtureOk,
         "keyboard.milo hands: first_spot, second_spot and ik_object bound on both: %s",
         fixtureOk ? "yes" : "NO");
    if (!fixtureOk)
        return;

    // ---- layout ----
    float worst = 0;
    int keysChecked = 0;
    for (int h = 0; h < 2; h++) {
        CharKeyHandMidi *k = hand[h];
        k->unk7c = nullptr; // no character: the teleport test is not this check
        k->unk5c.clear();
        k->unk78 = true;
        k->unk88 = -1e30f;
        k->Poll();
        const Transform &F = k->mFirstSpot->WorldXfm();
        const Transform &S = k->mSecondSpot->WorldXfm();
        Vector3 keyDir(S.v.x - F.v.x, S.v.y - F.v.y, S.v.z - F.v.z);
        float keyDist = sqrtf(keyDir.x * keyDir.x + keyDir.y * keyDir.y + keyDir.z * keyDir.z);
        keyDir.Set(keyDir.x / keyDist, keyDir.y / keyDist, keyDir.z / keyDist);
        Vector3 up = F.m.z;
        float ul = sqrtf(up.x * up.x + up.y * up.y + up.z * up.z);
        up.Set(up.x / ul, up.y / ul, up.z / ul);
        Vector3 tip(-F.m.y.x, -F.m.y.y, -F.m.y.z);
        Vector3 cur(F.v.x - 0.4f * up.x, F.v.y - 0.4f * up.y, F.v.z - 0.4f * up.z);
        float w = keyDist * 0.0714285746f, b = keyDist * 0.0357142873f;
        Vector3 white(keyDir.x * w, keyDir.y * w, keyDir.z * w);
        Vector3 black(tip.x * 2 + keyDir.x * b + up.x * 0.5f, tip.y * 2 + keyDir.y * b + up.y * 0.5f,
                      tip.z * 2 + keyDir.z * b + up.z * 0.5f);
        for (int key = 1; key <= 0x19; key++) {
            Vector3 p;
            if (key == 1)
                p = cur;
            else if (IsRefBlackKey(key))
                p.Set(cur.x + black.x, cur.y + black.y, cur.z + black.z);
            else {
                cur.Set(cur.x + white.x, cur.y + white.y, cur.z + white.z);
                p = cur;
            }
            Vector3 t(p.x + tip.x, p.y + tip.y, p.z + tip.z);
            worst = std::max(worst, Dist3(k->unk4c[key], p) / keyDist);
            worst = std::max(worst, Dist3(k->unk54[key], t) / keyDist);
            keysChecked++;
        }
        worst = std::max(worst, k->unk78 ? 1.0f : 0.0f);
    }
    // 1e-5 of the keyboard's width: float noise from the reciprocal multiply
    // and the order of the adds, three orders of magnitude below any wrong
    // constant or a key on the wrong colour.
    Gate("uf-keyhand-poll-layout", worst < 1e-5f,
         "%d keys on 2 hands: key and tip positions from the spots' world transforms, worst "
         "error %.2e of the keyboard width (limit 1e-5)",
         keysChecked, worst);

    // ---- fingering ----
    // The rig alone carries no finger bones: CharIKFingers binds them by name
    // from the character the rig is merged into. KeyFinger hands each target
    // to CharIKFingers::SetFinger, which reads finger 1's bone, so for this run
    // every unbound finger bone points at the rig's own shipped hand bone. The
    // check reads the hand's fingering state, not the IK.
    int stoodIn = 0;
    bool handBones = true;
    for (int h = 0; h < 2; h++) {
        CharIKFingers *ik = hand[h]->mIKObject;
        handBones = handBones && ik->mHand && ik->mFingers.size() == 5;
        for (int f = 0; handBones && f < 5; f++) {
            CharIKFingers::FingerDesc &d = ik->mFingers[f];
            if (!d.mFinger01) {
                RndTransformable *hb = ik->mHand.Ptr();
                d.mFinger01 = hb;
                d.mFinger02 = hb;
                d.mFinger03 = hb;
                d.mFingertip = hb;
                stoodIn++;
            }
        }
    }
    if (!handBones) {
        Gate("uf-keyhand-poll-fingers", false, "a hand's ik_object has no hand bone");
        return;
    }
    int steps = 0, wrong = 0;
    std::string first;
    for (int h = 0; h < 2; h++) {
        CharKeyHandMidi *k = hand[h];
        for (int i = 0; i < 5; i++)
            if (k->unk6c[i])
                k->UnkeyFinger((CharIKFingers::FingerNum)i);
        k->unk74 = 5;
        k->unk64 = 0;
        k->unk68 = 5;
        RefHand ref;
        ref.right = k->mIsRightHand;
        Lcg rng(0x5eed + h);
        for (int step = 0; step < 3000; step++) {
            int op = rng.Range(10);
            bool back = false;
            std::vector<int> keys;
            if (op < 3) {
                // lift a random subset of fingers on both
                for (int f = 0; f < 5; f++)
                    if (rng.Range(2)) {
                        if (k->unk6c[f])
                            k->UnkeyFinger((CharIKFingers::FingerNum)f);
                        if (ref.fing[f])
                            ref.Unkey(f);
                    }
            } else if (op == 9 && rng.Range(4) == 0) {
                back = true; // the clock went backwards since the last poll
            }
            int n = op < 3 ? 0 : rng.Range(op == 8 ? 8 : 4) + (op == 8 ? 0 : 1);
            for (int i = 0; i < n; i++)
                keys.push_back(1 + rng.Range(0x19));
            k->unk5c.clear();
            for (int key : keys)
                k->unk5c.push_back((CharKeyHandMidi::KeyboardKey)key);
            float before = TheTaskMgr.Seconds(TaskMgr::kRealTime);
            k->unk88 = back ? before + 1000.0f : before - 1.0f;
            k->Poll();
            float after = TheTaskMgr.Seconds(TaskMgr::kRealTime);
            ref.Poll(keys, back);
            steps++;
            bool same = k->unk5c.empty() && k->unk64 == ref.lastKey && k->unk68 == ref.lastFinger
                && k->unk74 == ref.freeCount && k->unk88 >= before && k->unk88 <= after;
            for (int f = 0; f < 5; f++)
                same = same && k->unk6c[f] == ref.fing[f];
            if (!same) {
                if (!wrong) {
                    std::string ks;
                    for (int key : keys)
                        ks += MakeString(" %d", key);
                    char fb[512];
                    snprintf(fb, sizeof(fb),
                        " first: %s hand step %d, keys [%s ]%s: fingers %d %d %d %d %d last %d/%d "
                        "free %d, want %d %d %d %d %d last %d/%d free %d",
                        h ? "right" : "left", step, ks.c_str(), back ? " after the clock went back" : "",
                        k->unk6c[0], k->unk6c[1], k->unk6c[2], k->unk6c[3], k->unk6c[4], k->unk64,
                        k->unk68, k->unk74, ref.fing[0], ref.fing[1], ref.fing[2], ref.fing[3],
                        ref.fing[4], ref.lastKey, ref.lastFinger, ref.freeCount);
                    first = fb;
                }
                wrong++;
                // resync so one slip does not cascade
                for (int f = 0; f < 5; f++)
                    ref.fing[f] = k->unk6c[f];
                ref.lastKey = k->unk64;
                ref.lastFinger = k->unk68;
                ref.freeCount = k->unk74;
            }
        }
        // leave the hand as the rig had it: nothing keyed, all fingers free
        for (int i = 0; i < 5; i++)
            if (k->unk6c[i])
                k->UnkeyFinger((CharIKFingers::FingerNum)i);
        k->unk74 = 5;
        k->unk64 = 0;
        k->unk68 = 5;
        k->unk88 = 0;
        k->unk78 = true;
        CharIKFingers *ik = k->mIKObject;
        for (int f = 0; f < 5; f++) {
            CharIKFingers::FingerDesc &d = ik->mFingers[f];
            if (d.mFinger01.Ptr() == ik->mHand.Ptr()) {
                d.mFinger01 = nullptr;
                d.mFinger02 = nullptr;
                d.mFinger03 = nullptr;
                d.mFingertip = nullptr;
            }
        }
    }
    Gate("uf-keyhand-poll-fingers", wrong == 0,
         "%d polls on 2 hands (1-4 keys, 0-7 keys, lifted fingers, the clock going back; %d "
         "unbound finger bone sets stood in by the hand bone): fingers, last key/finger, free "
         "count and poll time vs the retail rules: %d wrong%s",
         steps, stoodIn, wrong, first.c_str());
}

// ========================================================= crowd meter ==
// An EventTrigger that only counts. Installed in place of the shipped
// triggers a row fires, so the row's choice of trigger is observable.
class ProbeTrigger : public EventTrigger {
public:
    int n = 0;
    void Trigger() override { n++; }
};

// The game-mode object the crowd rows read by name ("gamemode").
class CrowdGameMode : public Hmx::Object {
public:
    int practice = 0, show = 1, update = 1;
    bool SyncProperty(DataNode &n, DataArray *prop, int i, PropOp op) override {
        if (op == kPropGet && i == prop->Size() - 1 && prop->Type(i) == kDataSymbol) {
            const char *s = prop->Sym(i).Str();
            if (!strcmp(s, "is_practice")) {
                n = practice;
                return true;
            }
            if (!strcmp(s, "show_crowd_meter")) {
                n = show;
                return true;
            }
            if (!strcmp(s, "update_crowd_meter")) {
                n = update;
                return true;
            }
        }
        return Hmx::Object::SyncProperty(n, prop, i, op);
    }
};

ObjDirPtr<ObjectDir> gTrackPanel;
CrowdGameMode *gCrowdMode = nullptr; // kept alive: SetupCrowdMeter caches it in a static

// Retail fn_8227DF58 (BandCrowdMeter::Poll), read off the asm. Skipped when
// disabled. peak = 1.0, or the meter's peak value when the ordered list is
// non-empty and some used icon has value >= 1.0. For each used icon whose
// value changed since the last poll (flag 0x20): value >= peak -> if not yet
// a peak, mark it, link its group at the FRONT of the ordered list, hide its
// arrow; value < peak -> if a peak, unmark, unlink, show its arrow; then set
// its group's frame to the value. Clear the changed flag. Then the ordered
// groups ease toward frame index+2: f + max(1, gap) * 0.1 (fmadds) capped at
// the target from below, f - max(1, gap) * 0.1 floored at it from above.
// A change in the list's size fires show_peak_arrow (non-empty) or
// hide_peak_arrow (empty).
struct RefMeter {
    float frame[5];
    bool peak[5];
    bool pend[5];
    std::vector<int> order;
};

void CrowdMeterPollChecks(BandCrowdMeter *m, ProbeTrigger *show, ProbeTrigger *hide,
                          ProbeTrigger *arrowShow[5], ProbeTrigger *arrowHide[5]) {
    RefMeter r;
    m->mOrderedPeaks.clear();
    for (int i = 0; i < 5; i++) {
        BandCrowdMeter::IconData &d = m->mIconData[i];
        d.unk19 = false;
        d.unk20 = false;
        d.unk1c = 0;
        d.SetUsed(i < 4);
        d.unkc->SetShowing(true); // a hidden group ignores SetFrame
        d.unkc->SetFrame(0, 1);
        r.frame[i] = 0;
        r.peak[i] = false;
        r.pend[i] = false;
    }
    m->mDisabled = false;
    m->mPeakValue = 0.9f;
    int bad = 0, polls = 0;
    std::string first;
    auto poll = [&](const char *what, std::vector<std::pair<int, float> > vals, bool disabled) {
        int showN = show->n, hideN = hide->n, aS[5], aH[5];
        for (int i = 0; i < 5; i++) {
            aS[i] = arrowShow[i]->n;
            aH[i] = arrowHide[i]->n;
        }
        m->mDisabled = disabled;
        for (auto &v : vals) {
            if (v.second != m->mIconData[v.first].unk1c)
                r.pend[v.first] = true;
            m->SetPlayerValue(v.first, v.second);
        }
        m->Poll();
        polls++;
        int wantShow = 0, wantHide = 0, wS[5] = { 0 }, wH[5] = { 0 };
        if (!disabled) {
            int oldSize = r.order.size();
            float pk = 1.0f;
            if (!r.order.empty())
                for (int i = 0; i < 5; i++)
                    if (m->mIconData[i].Used() && m->mIconData[i].unk1c >= 1.0f) {
                        pk = m->mPeakValue;
                        break;
                    }
            for (int i = 0; i < 5; i++) {
                if (!m->mIconData[i].Used() || !r.pend[i])
                    continue;
                r.pend[i] = false;
                float val = m->mIconData[i].unk1c;
                if (val >= pk) {
                    if (!r.peak[i]) {
                        r.peak[i] = true;
                        r.order.insert(r.order.begin(), i);
                        wH[i]++;
                    }
                } else {
                    if (r.peak[i]) {
                        r.peak[i] = false;
                        r.order.erase(std::find(r.order.begin(), r.order.end(), i));
                        wS[i]++;
                    }
                    r.frame[i] = val;
                }
            }
            for (int idx = 0; idx < (int)r.order.size(); idx++) {
                float tgt = idx + 2.0f, &f = r.frame[r.order[idx]];
                if (f < tgt)
                    f = std::min(tgt, fmaf(std::max(1.0f, tgt - f), 0.1f, f));
                else if (f > tgt)
                    f = std::max(tgt, fmaf(std::max(1.0f, f - tgt), -0.1f, f));
            }
            if ((int)r.order.size() != oldSize)
                (r.order.empty() ? wantHide : wantShow)++;
        }
        std::string why;
        std::vector<int> got;
        for (ObjPtrList<RndGroup>::iterator it = m->mOrderedPeaks.begin();
             it != m->mOrderedPeaks.end(); ++it)
            for (int i = 0; i < 5; i++)
                if (*it == m->mIconData[i].unkc)
                    got.push_back(i);
        if (got != r.order)
            why += " order";
        for (int i = 0; i < 5; i++) {
            BandCrowdMeter::IconData &d = m->mIconData[i];
            if (d.unk19 != r.peak[i])
                why += MakeString(" peak%d", i);
            if (d.unk20 != r.pend[i])
                why += MakeString(" changed%d", i);
            if (fabsf(d.unkc->GetFrame() - r.frame[i]) > 1e-5f)
                why += MakeString(" frame%d=%g(want %g)", i, d.unkc->GetFrame(), r.frame[i]);
            if (arrowShow[i]->n - aS[i] != wS[i] || arrowHide[i]->n - aH[i] != wH[i])
                why += MakeString(" arrow%d", i);
        }
        if (show->n - showN != wantShow || hide->n - hideN != wantHide)
            why += MakeString(" peak_arrow(show %d hide %d, want %d %d)", show->n - showN,
                              hide->n - hideN, wantShow, wantHide);
        if (!why.empty()) {
            if (!bad)
                first = MakeString(" first: %s:%s", what, why.c_str());
            bad++;
        }
    };
    poll("two players at the top", { { 0, 1.0f }, { 1, 0.5f }, { 2, 1.0f }, { 3, 0.3f } }, false);
    poll("no change, easing", {}, false);
    poll("no change, easing again", {}, false);
    poll("player 0 drops below the peak value", { { 0, 0.6f } }, false);
    poll("disabled: a change is ignored", { { 1, 1.0f } }, true);
    poll("re-enabled with that change pending", {}, false);
    poll("player 2 drops below 1.0", { { 2, 0.95f } }, false);
    Lcg rng(0xc40d);
    for (int i = 0; i < 300; i++) {
        std::vector<std::pair<int, float> > v;
        int n = rng.Range(3);
        for (int j = 0; j < n; j++) {
            int who = rng.Range(5);
            bool dup = false;
            for (auto &x : v)
                dup |= x.first == who;
            if (!dup)
                v.push_back({ who, rng.Range(5) == 0 ? 1.0f : rng.Range(1000) / 900.0f });
        }
        poll("random", v, false);
    }
    m->mDisabled = false;
    Gate("uf-crowd-poll", bad == 0,
         "%d polls of the shipped crowd meter (5 icons, 4 used): peak list order, peak flags, "
         "group frames (easing to index+2, frame = value off the list), arrow and peak-arrow "
         "triggers: %d wrong%s",
         polls, bad, first.c_str());
}

void CrowdChecks() {
    printf("\n=== W16-UF: the crowd meter on the shipped track panel ===\n");
    gTrackPanel.LoadFile(FilePath("ui/track/gen/trackpanel.milo_xbox"), false, false, kLoadFront,
                         false);
    TrackPanelDir *tp = dynamic_cast<TrackPanelDir *>(gTrackPanel.Ptr());
    BandCrowdMeter *m = tp ? tp->mCrowdMeter.Ptr() : nullptr;
    GemTrackDir *gt = nullptr;
    if (tp)
        for (ObjDirItr<GemTrackDir> it(tp, true); it != nullptr; ++it) {
            gt = it;
            break;
        }
    bool icons = m && m->mIconData.size() == 5;
    for (int i = 0; icons && i < 5; i++)
        icons = m->mIconData[i].unk0 && m->mIconData[i].unkc && m->mIconData[i].unk0->mIconLabel
            && m->mIconData[i].unk0->mArrowShowTrig && m->mIconData[i].unk0->mArrowHideTrig;
    RndAnimatable *warn = gt ? gt->Find<RndAnimatable>("warning_anims.grp", false) : nullptr;
    bool fixtureOk = tp && m && icons && m->mShowPeakArrowTrig && m->mHidePeakArrowTrig && gt
        && gt->Dir() == tp && warn;
    Gate("uf-crowd-fixture", fixtureOk,
         "trackpanel.milo: TrackPanelDir %s, crowd meter %s, 5 icons with groups/labels/arrow "
         "triggers %s, a GemTrackDir in the panel %s, its warning_anims.grp %s",
         tp ? "loaded" : "MISSING", m ? "bound" : "MISSING", icons ? "yes" : "NO",
         gt ? "found" : "MISSING", warn ? "found" : "MISSING");
    if (!fixtureOk)
        return;

    // Probe triggers in place of the shipped ones.
    ProbeTrigger *show = new ProbeTrigger, *hide = new ProbeTrigger;
    ProbeTrigger *aS[5], *aH[5], *stN[5], *stF[5];
    EventTrigger *savedShow = m->mShowPeakArrowTrig, *savedHide = m->mHidePeakArrowTrig;
    EventTrigger *savedAS[5], *savedAH[5], *savedN[5], *savedF[5];
    m->mShowPeakArrowTrig = show;
    m->mHidePeakArrowTrig = hide;
    for (int i = 0; i < 5; i++) {
        CrowdMeterIcon *ic = m->mIconData[i].unk0;
        savedAS[i] = ic->mArrowShowTrig;
        savedAH[i] = ic->mArrowHideTrig;
        savedN[i] = ic->mStateNormalTrig;
        savedF[i] = ic->mStateFailedTrig;
        ic->mArrowShowTrig = aS[i] = new ProbeTrigger;
        ic->mArrowHideTrig = aH[i] = new ProbeTrigger;
        ic->mStateNormalTrig = stN[i] = new ProbeTrigger;
        ic->mStateFailedTrig = stF[i] = new ProbeTrigger;
    }

    CrowdMeterPollChecks(m, show, hide, aS, aH);

    // ---- TrackPanelDir::ConfigureCrowdMeter ----
    // Retail fn_8228E6B8: practice (gamemode is_practice), or a panel with no
    // crowd meter / resumed without score -> Disable + hide, return.
    // Otherwise UpdatePlayers(instruments), Enable, show = gamemode
    // show_crowd_meter. UpdatePlayers: icon i used = instrument i is neither
    // none nor pending and the icon's label has a default text.
    if (!gCrowdMode) {
        gCrowdMode = new CrowdGameMode;
        gCrowdMode->SetName("gamemode", ObjectDir::Main());
    }
    TrackPanelInterface *savedPanel = tp->mTrackPanel;
    std::vector<TrackInstrument> savedInst = tp->mInstruments;
    tp->mTrackPanel = nullptr;
    std::vector<TrackInstrument> inst = { kInstGuitar, kInstNone, kInstDrum, kInstPending,
                                          kInstVocals };
    int cfgBad = 0;
    std::string cfgWhy;
    struct Case {
        int practice, show;
    } cases[] = { { 1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 } };
    for (auto &c : cases) {
        gCrowdMode->practice = c.practice;
        gCrowdMode->show = c.show;
        for (int i = 0; i < 5; i++) {
            m->mIconData[i].SetUsed(false);
            // Icons 0-2 carry a player icon (as SetupCrowdMeter leaves them);
            // 3 and 4 none, as the shipped labels load.
            m->mIconData[i].unk0->mIconLabel->mIcon = i < 3 ? "G" : "";
        }
        m->mOrderedPeaks.clear();
        for (int i = 0; i < 5; i++)
            m->mIconData[i].unk19 = false;
        m->mDisabled = !c.practice; // the opposite of what each case should leave
        m->SetShowing(!c.practice ? false : true);
        tp->mInstruments = inst;
        tp->ConfigureCrowdMeter();
        bool wantDisabled = c.practice;
        bool wantShowing = !c.practice && c.show;
        std::string why;
        if (m->mDisabled != wantDisabled)
            why += " disabled";
        if (m->Showing() != wantShowing)
            why += " showing";
        for (int i = 0; i < 5; i++) {
            bool want = !c.practice && inst[i] != kInstNone && inst[i] != kInstPending
                && strcmp(m->mIconData[i].unk0->mIconLabel->GetDefaultText(), gNullStr) != 0;
            if (m->mIconData[i].Used() != want)
                why += MakeString(" used%d", i);
        }
        if (!why.empty()) {
            if (!cfgBad)
                cfgWhy = MakeString(" first: practice %d show %d:%s", c.practice, c.show, why.c_str());
            cfgBad++;
        }
    }
    Gate("uf-crowd-configure", cfgBad == 0,
         "4 game-mode cases (practice on/off x show_crowd_meter on/off), instruments guitar none "
         "drum pending vocals, player icons on icons 0-2: disabled, shown and used flags (want "
         "used 1 0 1 0 0 when not practice): %d case(s) wrong%s",
         cfgBad, cfgWhy.c_str());

    // ---- BandTrack::SetupCrowdMeter / SetCrowdRating ----
    // Retail fn_822D7E08 (SetupCrowdMeter): show flag = gamemode
    // update_crowd_meter; icon text = the parent track's icon, else "G"; when
    // the meter exists (show flag on), is enabled and the track index is >= 0:
    // icon[index] label icon = text, icon[index] panel = this track's panel;
    // the unison icon (if any) takes the same text.
    // Retail fn_822D9C50 (SetCrowdRating): with a meter, enabled, index >= 0:
    // player value = rating; state != invalid -> icon state set (normal fires
    // the normal trigger, failed the failed trigger, warning animates); then
    // when (state == warning) differs from the track's warning flag and state
    // is not failed, the flag follows, and warning_anims.grp is set to frame 0
    // and looped (warning on) or played to its end (off).
    BandTrack *bt = gt;
    int savedIdx = bt->mTrackIdx;
    bool savedShowMeter = bt->mShowCrowdMeter, savedWarn = bt->unk1c;
    TempoMap *savedTempo = TheTempoMap;
    BeatMap *savedBeat = TheBeatMap;
    SimpleTempoMap tempo(500.0f);
    BeatMap beats;
    if (!TheTempoMap)
        TheTempoMap = &tempo; // GetPulseAnimStartDelay reads the song's beat
    if (!TheBeatMap)
        TheBeatMap = &beats;
    m->Enable();
    m->SetShowing(true);
    int setupBad = 0;
    std::string setupWhy;
    {
        gCrowdMode->update = 1;
        bt->mTrackIdx = 2;
        bt->mParent = nullptr;
        for (int i = 0; i < 5; i++) {
            m->mIconData[i].unk0->mIconLabel->SetIcon('X');
            m->mIconData[i].unk0->unk240 = nullptr;
        }
        bt->SetupCrowdMeter();
        std::string why;
        if (!bt->mShowCrowdMeter)
            why += " show_flag";
        for (int i = 0; i < 5; i++) {
            CrowdMeterIcon *ic = m->mIconData[i].unk0;
            bool mine = i == 2;
            if (strcmp(ic->mIconLabel->mIcon.c_str(), mine ? "G" : "X") != 0)
                why += MakeString(" icon%d='%s'", i, ic->mIconLabel->mIcon.c_str());
            if ((ic->unk240 == (TrackPanelDirBase *)tp) != mine)
                why += MakeString(" panel%d", i);
        }
        if (bt->mUnisonIcon && strcmp(bt->mUnisonIcon->mIconLabel->mIcon.c_str(), "G") != 0)
            why += " unison";
        // update_crowd_meter off: no meter, nothing touched.
        gCrowdMode->update = 0;
        m->mIconData[2].unk0->mIconLabel->SetIcon('X');
        bt->SetupCrowdMeter();
        if (bt->mShowCrowdMeter)
            why += " show_flag_off";
        if (strcmp(m->mIconData[2].unk0->mIconLabel->mIcon.c_str(), "X") != 0)
            why += " icon_touched_without_meter";
        // and on again, for the rating checks
        gCrowdMode->update = 1;
        bt->SetupCrowdMeter();
        if (!why.empty()) {
            setupBad++;
            setupWhy = why;
        }
    }
    Gate("uf-crowd-setup", setupBad == 0,
         "a GemTrackDir of the panel as track 2, no parent (icon \"G\"), update_crowd_meter on "
         "then off: show flag, icon 2's label icon and panel, the other icons untouched%s: %s%s",
         bt->mUnisonIcon ? ", the unison icon" : "", setupBad ? "WRONG" : "ok", setupWhy.c_str());

    int rateBad = 0, rateSteps = 0;
    std::string rateWhy;
    {
        CrowdMeterIcon *ic = m->mIconData[2].unk0;
        bt->unk1c = false;
        ic->mState = kCrowdMeterInvalidState;
        ic->mQuarantined = false;
        warn->SetFrame(7.0f, 1.0f);
        struct Step {
            float rating;
            CrowdMeterState st;
            bool disabled;
            int idx;
            bool wantWarn;
            int wantNormal, wantFailed;
            bool wantFrame0;
        } steps[] = {
            { 0.40f, kCrowdMeterNormal, false, 2, false, 1, 0, false },
            { 0.41f, kCrowdMeterNormal, false, 2, false, 0, 0, false },
            { 0.20f, kCrowdMeterWarning, false, 2, true, 0, 0, true },
            { 0.10f, kCrowdMeterFailed, false, 2, true, 0, 1, false },
            { 0.50f, kCrowdMeterNormal, false, 2, false, 1, 0, false },
            { 0.60f, kCrowdMeterInvalidState, false, 2, false, 0, 0, false },
            { 0.70f, kCrowdMeterWarning, true, 2, false, 0, 0, false },
            { 0.80f, kCrowdMeterWarning, false, -1, false, 0, 0, false },
        };
        float lastVal = m->mIconData[2].unk1c;
        for (auto &s : steps) {
            m->mDisabled = s.disabled;
            bt->mTrackIdx = s.idx;
            int n0 = stN[2]->n, f0 = stF[2]->n;
            CrowdMeterState st0 = ic->mState;
            warn->SetFrame(7.0f, 1.0f);
            m->mIconData[2].unk20 = false;
            bt->SetCrowdRating(s.rating, s.st);
            rateSteps++;
            bool applies = !s.disabled && s.idx >= 0;
            float wantVal = applies ? s.rating : lastVal;
            CrowdMeterState wantSt = applies && s.st != kCrowdMeterInvalidState ? s.st : st0;
            std::string why;
            if (m->mIconData[2].unk1c != wantVal)
                why += " value";
            if (applies && wantVal != lastVal && !m->mIconData[2].unk20)
                why += " changed_flag";
            if (ic->mState != wantSt)
                why += " icon_state";
            if (stN[2]->n - n0 != s.wantNormal || stF[2]->n - f0 != s.wantFailed)
                why += " state_trigger";
            if (bt->unk1c != s.wantWarn)
                why += " warning_flag";
            if ((warn->GetFrame() == 0.0f) != s.wantFrame0)
                why += MakeString(" warning_anims frame %g", warn->GetFrame());
            if (applies)
                lastVal = wantVal;
            if (!why.empty()) {
                if (!rateBad)
                    rateWhy = MakeString(" first: rating %.2f state %d%s%s:%s", s.rating, (int)s.st,
                                         s.disabled ? " (meter disabled)" : "",
                                         s.idx < 0 ? " (track index -1)" : "", why.c_str());
                rateBad++;
            }
        }
    }
    Gate("uf-crowd-rating", rateBad == 0,
         "%d ratings on track 2 (normal, normal again, warning, failed, normal, invalid state, "
         "meter disabled, no track index): player value, icon state and its trigger, the "
         "track's warning flag, warning_anims.grp reset: %d wrong%s",
         rateSteps, rateBad, rateWhy.c_str());

    // ---- restore ----
    TheTempoMap = savedTempo;
    TheBeatMap = savedBeat;
    bt->mTrackIdx = savedIdx;
    bt->mShowCrowdMeter = savedShowMeter;
    bt->unk1c = savedWarn;
    tp->mTrackPanel = savedPanel;
    tp->mInstruments = savedInst;
    m->mShowPeakArrowTrig = savedShow;
    m->mHidePeakArrowTrig = savedHide;
    for (int i = 0; i < 5; i++) {
        CrowdMeterIcon *ic = m->mIconData[i].unk0;
        ic->mArrowShowTrig = savedAS[i];
        ic->mArrowHideTrig = savedAH[i];
        ic->mStateNormalTrig = savedN[i];
        ic->mStateFailedTrig = savedF[i];
        delete aS[i];
        delete aH[i];
        delete stN[i];
        delete stF[i];
    }
    delete show;
    delete hide;
}

// ========================================= DeltaArray::AppendDeltas ==
// Retail fn_822C7298, read off the asm, and its quantizer fn_822C7040: a
// vertex's delta is pos - base, each component clamped to [-2, 2] by two fsel
// (a NaN passes through), fmadd by 63.5 and 0.5 in double, converted toward
// zero to 64 bits (fctidz) and its low byte stored; a NaN converts to
// 0x8000000000000000, byte 0. Runs of vertices whose three bytes are
// not all zero become records appended to the array: u16 first vertex, u16
// count, 3 bytes per vertex. Mismatched point counts are a MILO_FAIL in the
// dev build and nothing in retail; not driven.
signed char RefQuant(float d) {
    float c = d < -2.0f ? -2.0f : d > 2.0f ? 2.0f : d;
    double x = (double)c * 63.5 + 0.5;
    long long i = std::isnan(x) ? LLONG_MIN : (long long)x; // fctidz
    return (signed char)(unsigned char)(i & 0xff);
}

std::vector<unsigned char> RefAppend(const std::vector<Vector3> &pos, const std::vector<Vector3> &base) {
    std::vector<unsigned char> out;
    int n = pos.size(), i = 0;
    auto q = [&](int v, signed char *d) {
        d[0] = RefQuant(pos[v].x - base[v].x);
        d[1] = RefQuant(pos[v].y - base[v].y);
        d[2] = RefQuant(pos[v].z - base[v].z);
        return d[0] || d[1] || d[2];
    };
    signed char d[3];
    while (i < n) {
        while (i < n && !q(i, d))
            i++;
        if (i >= n)
            break;
        int last = i + 1;
        while (last < n && q(last, d))
            last++;
        unsigned short hdr[2] = { (unsigned short)i, (unsigned short)(last - i) };
        out.insert(out.end(), (unsigned char *)hdr, (unsigned char *)hdr + 4);
        for (int v = i; v < last; v++) {
            q(v, d);
            out.insert(out.end(), (unsigned char *)d, (unsigned char *)d + 3);
        }
        i = last;
    }
    return out;
}

ObjDirPtr<ObjectDir> gHead;

void AppendDeltasChecks() {
    printf("\n=== W16-UF: BandFaceDeform::DeltaArray::AppendDeltas on the shipped head ===\n");
    gHead.LoadFile(FilePath("char/main/shared/gen/head_female.milo_xbox"), false, false,
                   kLoadFront, false);
    RndMeshAnim *base = FindByName<RndMeshAnim>(gHead.Ptr(), "base.msnm");
    std::vector<BandFaceDeform *> defs;
    for (ObjDirItr<BandFaceDeform> it(gHead.Ptr(), true); it != nullptr; ++it)
        defs.push_back(it);
    int nv = base && base->VertPointsKeys().size() ? base->VertPointsKeys()[0].value.size() : 0;
    int frames = 0, maxVert = 0;
    for (BandFaceDeform *d : defs)
        for (auto &f : d->mFrames) {
            frames++;
            for (Delta *p = (Delta *)f.mData; (char *)p < (char *)f.mData + f.mSize;
                 p = (Delta *)p->next())
                maxVert = std::max(maxVert, (int)*(unsigned short *)p + p->num);
        }
    bool fixtureOk = base && defs.size() == 6 && nv > 0 && frames > 0 && maxVert <= nv;
    Gate("uf-deltas-fixture", fixtureOk,
         "head_female.milo: base.msnm %s (%d points in key 0), %d BandFaceDeform(s) with %d "
         "shipped frames, highest vertex a frame touches %d",
         base ? "found" : "MISSING", nv, (int)defs.size(), frames, maxVert);
    if (!fixtureOk)
        return;
    const std::vector<Vector3> &b = base->VertPointsKeys()[0].value;
    int arrays = 0, bad = 0, sameAsShipped = 0;
    long bytes = 0;
    std::string first;
    // Each shipped frame decoded onto the base (delta = byte / 63.5), then
    // re-appended; plus the same frame appended twice into one array.
    for (BandFaceDeform *d : defs)
        for (auto &f : d->mFrames) {
            std::vector<Vector3> pos(b);
            for (Delta *p = (Delta *)f.mData; (char *)p < (char *)f.mData + f.mSize;
                 p = (Delta *)p->next()) {
                int v0 = *(unsigned short *)p;
                signed char *q = (signed char *)(p + 1);
                for (int k = 0; k < p->num; k++)
                    pos[v0 + k].Set(b[v0 + k].x + q[k * 3] / 63.5f, b[v0 + k].y + q[k * 3 + 1] / 63.5f,
                                    b[v0 + k].z + q[k * 3 + 2] / 63.5f);
            }
            std::vector<unsigned char> want = RefAppend(pos, b);
            BandFaceDeform::DeltaArray a;
            a.AppendDeltas(pos, b);
            arrays++;
            bytes += a.mSize;
            bool ok = a.mSize == (int)want.size() && !memcmp(a.mData, want.data(), want.size());
            sameAsShipped += a.mSize == f.mSize && !memcmp(a.mData, f.mData, f.mSize);
            a.AppendDeltas(pos, b);
            std::vector<unsigned char> twice(want);
            twice.insert(twice.end(), want.begin(), want.end());
            ok = ok && a.mSize == (int)twice.size() && !memcmp(a.mData, twice.data(), twice.size());
            if (!ok) {
                if (!bad)
                    first = MakeString(" first: %s frame %d: %d bytes, want %d", d->Name(),
                                       (int)(&f - &d->mFrames[0]), a.mSize, (int)want.size() * 2);
                bad++;
            }
        }
    // Edge values: clamping, rounding at the half steps, negative halves, NaN.
    std::vector<Vector3> eb(12, Vector3(0, 0, 0)), ep(12, Vector3(0, 0, 0));
    float edge[] = { 2.5f, -2.5f, 2.0f, -2.0f, 0.5f / 63.5f, -0.5f / 63.5f, 1.0f / 63.5f,
                     -1.0f / 63.5f, 0.0078f, -0.0078f, NAN, 1.5f / 63.5f };
    for (int i = 0; i < 12; i++)
        ep[i].Set(edge[i], i % 3 == 0 ? edge[i] : 0.0f, 0.0f);
    std::vector<unsigned char> ew = RefAppend(ep, eb);
    BandFaceDeform::DeltaArray ea;
    ea.AppendDeltas(ep, eb);
    bool edgeOk = ea.mSize == (int)ew.size() && !memcmp(ea.mData, ew.data(), ew.size());
    std::string edgeTxt;
    if (!edgeOk)
        for (int i = 0; i < std::max(ea.mSize, (int)ew.size()); i++)
            edgeTxt += MakeString(" %02x/%02x", i < ea.mSize ? ((unsigned char *)ea.mData)[i] : 0xff,
                                  i < (int)ew.size() ? ew[i] : 0xff);
    Gate("uf-deltas", bad == 0 && edgeOk,
         "%d shipped frames decoded onto base.msnm and re-appended (once, and twice into one "
         "array; %ld bytes, %d byte-identical to the shipped frame): %d wrong; 12 edge points "
         "(clamp, half steps, NaN): %s%s%s",
         arrays, bytes, sameAsShipped, bad, edgeOk ? "ok" : "WRONG", edgeTxt.c_str(), first.c_str());
}

// ============================================ chars.milo: closet rows ==
ObjDirPtr<ObjectDir> gChars;
ObjDirPtr<ObjectDir> gClosetClips;

bool SameXfm(const Transform &a, const Transform &b) { return !memcmp(&a, &b, sizeof(Transform)); }

// Retail fn_82281D50 (BandCharacter::OnClosetTeleport), read off the asm:
// dirty the closet waypoint (0x7c0) if it is clean, copy the character's
// local transform (0xf0, 0x40 bytes) into the waypoint's local transform,
// Teleport(waypoint) through the vtable (+0x2c), clear 0x5fe, return
// DataNode(0) (int 0).
void ClosetTeleportChecks(BandCharacter *chars[4]) {
    int bad = 0, n = 0;
    std::string first;
    for (int c = 0; c < 4; c++) {
        BandCharacter *bc = chars[c];
        Waypoint *wp = bc->unk734;
        Transform savedChar = bc->LocalXfm(), savedWp = wp->LocalXfm();
        Transform x;
        x.m.Set(0, 1, 0, -1, 0, 0, 0, 0, 1);
        x.v.Set(10.0f + c, -20.0f, 3.5f);
        bc->SetLocalXfm(x);
        Transform other;
        other.Reset();
        other.v.Set(-99, -99, -99);
        wp->SetLocalXfm(other);
        bc->unk5a2 = true;
        bc->SetTeleported(false);
        DataArray *msg = new DataArray(2);
        msg->Node(0) = Symbol("closet_teleport");
        DataNode ret = bc->OnClosetTeleport(msg);
        msg->Release();
        n++;
        std::string why;
        if (!SameXfm(wp->LocalXfm(), x))
            why += " waypoint_xfm";
        Transform want = wp->WorldXfm();
        Normalize(want.m, want.m);
        if (!SameXfm(bc->LocalXfm(), want))
            why += " teleported_xfm";
        if (!bc->Teleported())
            why += " teleported_flag";
        if (bc->unk5a2)
            why += " flag_5fe";
        if (ret.Type() != kDataInt || ret.Int() != 0)
            why += " return";
        if (!why.empty()) {
            if (!bad)
                first = MakeString(" first: %s:%s", bc->Name(), why.c_str());
            bad++;
        }
        wp->SetLocalXfm(savedWp);
        bc->SetLocalXfm(savedChar);
    }
    Gate("uf-closet-teleport", bad == 0,
         "%d shipped band characters: waypoint takes the character's transform, the character "
         "is teleported onto the waypoint's world transform, flag 0x5fe cleared, returns 0: %d "
         "wrong%s",
         n, bad, first.c_str());
}

// Retail fn_8232F1C0 (BandWardrobe::OnEnterCloset), read off the asm: dir =
// arg 2, i = arg 3; current names = the closet names (0x40); every target
// takes context "closet"; when target i has a driver: driver clips = dir's
// "clips" (no fail), closet name j = "closet_character" for j == i else "",
// SetDir(dir), target j showing iff j == i. Returns DataNode(0).
void EnterClosetChecks(BandWardrobe *w, ObjectDir *closet) {
    ObjectDir *clips = closet->Find<ObjectDir>("clips", false);
    BandWardrobe::TargetNames *savedCur = w->mCurNames;
    ObjectDir *savedVenue = w->mVenueDir;
    ObjectDir *savedClips[4];
    for (int i = 0; i < 4; i++)
        savedClips[i] = w->mTargets[i]->Driver() ? w->mTargets[i]->Driver()->ClipDir() : nullptr;
    int bad = 0;
    std::string first;
    for (int t = 0; t < 4; t++) {
        w->mCurNames = &w->mVenueNames;
        for (int i = 0; i < 4; i++) {
            w->mClosetNames.names[i] = "w16uf_stale";
            w->mTargets[i]->SetShowing(true);
            if (w->mTargets[i]->Driver())
                w->mTargets[i]->Driver()->SetClips(nullptr);
        }
        w->mVenueDir = nullptr;
        DataArray *msg = new DataArray(4);
        msg->Node(0) = Symbol("enter_closet");
        msg->Node(1) = Symbol("enter_closet");
        msg->Node(2) = DataNode(closet);
        msg->Node(3) = DataNode(t);
        DataNode ret = w->OnEnterCloset(msg);
        msg->Release();
        std::string why;
        if (w->mCurNames != &w->mClosetNames)
            why += " current_names";
        for (int i = 0; i < 4; i++) {
            CharWeightable *cw = w->mTargets[i]->Find<CharWeightable>("closet.weight", false);
            CharWeightable *vw = w->mTargets[i]->Find<CharWeightable>("venue.weight", false);
            if ((cw && cw->Weight() != 1.0f) || (vw && vw->Weight() != 0.0f))
                why += MakeString(" context%d", i);
            if (strcmp(w->mClosetNames.names[i].Str(), i == t ? "closet_character" : "") != 0)
                why += MakeString(" name%d", i);
            if (w->mTargets[i]->Showing() != (i == t))
                why += MakeString(" showing%d", i);
        }
        if (w->mTargets[t]->Driver()->ClipDir() != clips)
            why += " clips";
        if (w->mVenueDir != closet)
            why += " venue_dir";
        if (ret.Type() != kDataInt || ret.Int() != 0)
            why += " return";
        if (!why.empty()) {
            if (!bad)
                first = MakeString(" first: target %d:%s", t, why.c_str());
            bad++;
        }
    }
    w->mCurNames = savedCur;
    w->mVenueDir = savedVenue;
    for (int i = 0; i < 4; i++) {
        w->mTargets[i]->SetShowing(true);
        if (w->mTargets[i]->Driver())
            w->mTargets[i]->Driver()->SetClips(savedClips[i]);
    }
    Gate("uf-enter-closet", bad == 0,
         "the shipped wardrobe entering portrait_clips_shared.milo for each of its 4 targets: "
         "closet names current, closet/venue weights, names (\"closet_character\" on the "
         "target), the target's driver clips = the dir's clips, the dir set, only the target "
         "shown, returns 0: %d of 4 wrong%s",
         bad, first.c_str());
}

void ClosetChecks() {
    printf("\n=== W16-UF: closet rows on the shipped world/shared chars ===\n");
    gChars.LoadFile(FilePath("world/shared/gen/chars.milo_xbox"), false, false, kLoadFront, false);
    gClosetClips.LoadFile(FilePath("world/meta/closet/gen/portrait_clips_shared.milo_xbox"), false,
                          false, kLoadFront, false);
    BandCharacter *chars[4];
    bool ok = gChars.Ptr() != nullptr;
    for (int i = 0; i < 4; i++) {
        chars[i] = FindByName<BandCharacter>(gChars.Ptr(), MakeString("player%d", i));
        ok = ok && chars[i] && chars[i]->unk734;
    }
    BandWardrobe *w = FindByName<BandWardrobe>(gChars.Ptr(), "BandWardrobe");
    bool wardrobeOk = w && gClosetClips.Ptr() && gClosetClips->Find<ObjectDir>("clips", false);
    int drivers = 0, targets = 0;
    for (int i = 0; w && i < 4; i++) {
        targets += w->mTargets[i] != nullptr;
        drivers += w->mTargets[i] && w->mTargets[i]->Driver();
    }
    wardrobeOk = wardrobeOk && targets == 4 && drivers == 4;
    Gate("uf-closet-fixture", ok && wardrobeOk,
         "chars.milo: player0-3 with closet waypoints %s; BandWardrobe %s with %d/4 targets, "
         "%d/4 with a driver; portrait_clips_shared.milo 'clips' %s",
         ok ? "yes" : "NO", w ? "found" : "MISSING", targets, drivers,
         gClosetClips.Ptr() && gClosetClips->Find<ObjectDir>("clips", false) ? "found" : "MISSING");
    if (ok)
        ClosetTeleportChecks(chars);
    if (ok && wardrobeOk)
        EnterClosetChecks(w, gClosetClips.Ptr());
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
    BandFaceDeform::Init();
    REGISTER_OBJ_FACTORY(AppLabel);
    KeyHandFingerChecks();
    CymbalChecks();
    JoypadClientChecks();
    TargetCopyChecks();
    StoreMainPanelChecks();
    KeyHandPollChecks();
    CrowdChecks();
    AppendDeltasChecks();
    ClosetChecks();
    Hmx::Object::sFactories = savedFactories;
    return gRan;
}
