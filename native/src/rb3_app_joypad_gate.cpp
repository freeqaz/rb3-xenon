// rb3-app joypad gate (lane A14, 2026-11-22). Test code, native only, rb3-app
// only; inert unless RB3_APP_JOYPAD_GATE=1 (and then only with RB3_APP_RUN=1,
// since the scripted press needs App::Run's frames).
//
// Claim: a scripted Start on pad 0 reaches the game as ONE button_down message
// for button kPad_Start, pad 0, from local user 0 -- the user retail's
// JoypadResetXboxPC(4) (0x825323C0) binds to pad 0, through JoypadReset
// (0x82529AB0) when SetTheUserMgr runs (0x825231E0). JoypadPollCommon
// (0x82526A00) sends the message with the pad's bound user (JoypadData::mUser),
// so a missing binding shows up as a null user.
//
// The gate writes its own script (wait for splash_screen, then press Start 30
// frames later), subscribes to the joypad message source right after
// JoypadInitCommon -- before the UI does, so it sees the press first -- and
// ends the run with its verdict at that first button_down:
//   [PASS] ... + `RESULT: JOYPAD GATE PASSED`, rc 0
//   [FAIL] ... + `RESULT: GATE FAILED (joypad ...)`, rc 7
// The pad's joypad_connect message (pad 0 is the only pad that connects) is
// checked too, because it comes first: with no user bound, UIStats::OnMsg
// (JoypadConnectionMsg) dereferences the null user and the run segfaults before
// any button_down, which would leave no verdict at all. So an unbound pad 0
// fails here, at the connect, rc 7.
// A run that reaches poll 280 without a button_down also fails (rc 7), so a
// press that never arrives cannot read as a pass.
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "os/UserMgr.h"
#include "os/User.h"
#include "obj/Object.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <vector>

namespace {
bool gGateOn = false;
char gScriptPath[64];
const int kGateDeadlinePolls = 280;

[[noreturn]] void Verdict(bool pass, const char *what) {
    printf("  [%s] rb3-app joypad: %s\n", pass ? "PASS" : "FAIL", what);
    printf(pass ? "RESULT: JOYPAD GATE PASSED\n" : "RESULT: GATE FAILED (joypad start -> user 0 / pad 0)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(pass ? 0 : 7);
}

class JoypadGateSink : public Hmx::Object {
public:
    virtual DataNode Handle(DataArray *msg, bool warn) {
        Symbol type = msg->Sym(1);
        if (type == JoypadConnectionMsg::Type()) CheckConnect(JoypadConnectionMsg(msg));
        else if (type == ButtonDownMsg::Type()) Check(ButtonDownMsg(msg));
        return DataNode(kDataUnhandled, 0);
    }

private:
    static LocalUser *User0(int *count) {
        std::vector<LocalUser *> users;
        if (TheUserMgr) TheUserMgr->GetLocalUsers(users);
        *count = (int)users.size();
        return users.empty() ? nullptr : users[0];
    }
    void CheckConnect(const JoypadConnectionMsg &m) {
        int n;
        LocalUser *user0 = User0(&n);
        LocalUser *got = m.GetUser();
        if (m.Connected() && user0 && got == user0 && JoypadGetPadData(0)->mUser == user0) {
            printf("rb3-app joypad gate: pad 0 connected for local user 0 (%p)\n", (void *)got);
            fflush(stdout);
            return;
        }
        char what[256];
        snprintf(what, sizeof what,
                 "joypad_connect: connected %d, user %p (want local user 0 %p, %d local "
                 "users), pad 0 bound to %p",
                 (int)m.Connected(), (void *)got, (void *)user0, n,
                 (void *)JoypadGetPadData(0)->mUser);
        Verdict(false, what);
    }
    void Check(const ButtonDownMsg &m) {
        int n;
        LocalUser *user0 = User0(&n);
        LocalUser *got = m.GetUser();
        char what[256];
        snprintf(what, sizeof what,
                 "first button_down: button %d (want kPad_Start %d), pad %d (want 0), user %p "
                 "(want local user 0 %p, %d local users), pad 0 bound to %p",
                 (int)m.GetButton(), (int)kPad_Start, m.GetPadNum(), (void *)got, (void *)user0,
                 n, (void *)JoypadGetPadData(0)->mUser);
        bool ok = m.GetButton() == kPad_Start && m.GetPadNum() == 0 && user0 && got == user0
            && JoypadGetPadData(0)->mUser == user0;
        Verdict(ok, what);
    }
};
} // namespace

void RB3AppJoypadGateInstall() {
    const char *v = getenv("RB3_APP_JOYPAD_GATE");
    if (!v || !*v || !strcmp(v, "0")) return;
    gGateOn = true;
    // The engine's scripted input loads MILO_INPUT_SCRIPT at the first poll.
    char *path = gScriptPath;
    snprintf(path, sizeof gScriptPath, "/tmp/rb3-app-joypad-gate-%d.txt", (int)getpid());
    FILE *f = fopen(path, "w");
    if (!f) Verdict(false, "cannot write the gate's input script");
    fputs("# rb3-app joypad gate: Start on the splash screen\nwait_screen splash_screen\n+30 start\n", f);
    fclose(f);
    setenv("MILO_INPUT_SCRIPT", path, 1);
    JoypadSubscribe(new JoypadGateSink());
    printf("rb3-app joypad gate: armed (script %s, deadline poll %d)\n", path, kGateDeadlinePolls);
    fflush(stdout);
}

void RB3AppJoypadGatePoll(int poll) {
    if (!gGateOn) return;
    if (poll == 0) unlink(gScriptPath); // read at poll 0; leave nothing behind
    if (poll >= kGateDeadlinePolls)
        Verdict(false, "no button_down by the deadline poll");
}
