#include "os/StageKit.h"

#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "os/Timer.h"

// Ported from retail's bytes; the TU has no symbols and no source in any oracle
// (DC3 dropped the Stage Kit). Retail extent 0x82521B30..0x825227E8. Every name
// below is ours; each body cites the retail function it reproduces.
//
// State (retail address, initial value):
//   sRawWrite/sRawRead  0x82CCB18C / 0x82CCB188  ring cursors over sRaw
//   sRaw[32]            0x82CCB1E8  queued (left, right) raw commands
//   sLedSent[4]         0x82CCB190  LED byte last sent per bank
//   sLedOut[4]          0x82CCB194  LED byte to send per bank
//   sBankPattern[4]     0x82CCB198  8-step LED pattern per bank
//   sBankMode[4]        0x82CCB19C  bank mode (stagekit_set_bank_mode)
//   sStrobe             0x82CCB1AC  strobe setting
//   sFog                0x82CCB1B0  fog state, re-sent every 2 s
//   sTimer              0x82CCB1B8  fog re-send timer
//   sBankOn[4]          0x82C71AA4  {1, 1, 1, 1}
//   sPatternId[4]       0x82C71AA8  {6, 6, 6, 6}
//   sLastBank           0x82C71AB8  3

extern "C" int JoypadStageKitPadNum(); // os/Joypad.cpp, retail 0x82524D40
void JoypadStageKitSetRaw(int left, int right); // os/Joypad.cpp, retail 0x82524DE0

namespace {
    struct RawCmd {
        unsigned char left;
        unsigned char right;
    };

    const int kRawSize = 32;
    const int kNumBanks = 4;

    int sRawWrite;
    int sRawRead;
    RawCmd sRaw[kRawSize];
    unsigned char sLedSent[kNumBanks];
    unsigned char sLedOut[kNumBanks];
    unsigned char sBankPattern[kNumBanks];
    int sBankMode[kNumBanks];
    int sStrobe;
    bool sFog;
    Timer sTimer;
    bool sBankOn[kNumBanks] = { true, true, true, true };
    int sPatternId[kNumBanks] = { 6, 6, 6, 6 };
    int sLastBank = 3;

    // 0x82521B30: queue one raw command; a full ring drops its oldest entry.
    void SendRaw(unsigned char left, unsigned char right) {
        sRaw[sRawWrite].left = left;
        sRaw[sRawWrite].right = right;
        sRawWrite = (sRawWrite + 1) % kRawSize;
        if (sRawWrite == sRawRead)
            sRawRead = (sRawRead + 1) % kRawSize;
    }

    // 0x82521DC8: the raw strobe command for a strobe setting.
    int StrobeCmd(int setting) {
        switch (setting) {
        case 0:
            return 7;
        case 6:
            return 3;
        case 8:
            return 4;
        case 10:
            return 5;
        case 12:
            return 6;
        default:
            return 7;
        }
    }

    // One step of an 8-step pattern: rotate right by one bit.
    unsigned char RotateStep(unsigned char b) { return (b >> 1) | ((b & 1) << 7); }

    // The LED bytes for the four banks of `leds`, low byte first, written to
    // sLedOut (the loop retail inlines in 0x82521E20 and 0x82522130).
    void SetLedOut(unsigned int leds) {
        for (int i = 0; i < kNumBanks; i++) {
            unsigned char b = (leds >> (i * 8)) & 0xff;
            if (b != sLedOut[i])
                sLedOut[i] = b;
        }
    }
}

bool StageKitConnected() { return JoypadStageKitPadNum() != -1; }

void StageKitSetFog(bool on) {
    SendRaw(0, on ? 1 : 2);
    sFog = on;
}

void StageKitSetStrobe(int setting) {
    if (setting != sStrobe) {
        sStrobe = setting;
        SendRaw(0, StrobeCmd(setting));
    }
}

// Modes 1 and 8..13 light the bank, 0 and 2..7 darken it; any other value is
// recorded but leaves the bank's on/off flag alone (retail compares unsigned).
void StageKitSetLedState(int bank, int state) {
    if (sBankMode[bank] == state)
        return;
    sBankMode[bank] = state;
    unsigned int s = state;
    bool on;
    if (s < 1)
        on = false;
    else if (s == 1)
        on = true;
    else if (s < 8)
        on = false;
    else if (s < 14)
        on = true;
    else
        return;
    sBankOn[bank] = on;
}

// Patterns 0..6 select a fixed 8-step bitmask; any other id is recorded and
// the bank's pattern left as it was.
void StageKitSetLedPattern(int bank, int pattern) {
    if (sPatternId[bank] == pattern)
        return;
    sPatternId[bank] = pattern;
    unsigned char bits;
    switch ((unsigned int)pattern) {
    case 0:
        bits = 0x01;
        break;
    case 1:
        bits = 0x11;
        break;
    case 2:
        bits = 0x55;
        break;
    case 3:
        bits = 0x10;
        break;
    case 4:
        bits = 0x44;
        break;
    case 5:
        bits = 0xaa;
        break;
    case 6:
        bits = 0xff;
        break;
    default:
        return;
    }
    sBankPattern[bank] = bits;
}

void StageKitUpdateLeds() {
    unsigned int leds = 0;
    for (int i = 0; i < kNumBanks; i++) {
        if (sBankOn[i])
            leds |= (unsigned int)sBankPattern[i] << (i * 8);
    }
    SetLedOut(leds);
}

// Retail leaves sPatternId at 6 while it clears sBankPattern, so a reset bank
// shows nothing until a pattern other than 6 is set.
void StageKitReset() {
    SendRaw(0, 0xff);
    for (int i = 0; i < kNumBanks; i++) {
        sLedSent[i] = 0;
        sBankMode[i] = 0;
        sBankPattern[i] = 0;
        sLedOut[i] = 0;
        sBankOn[i] = true;
        sPatternId[i] = 6;
    }
    sFog = false;
    sStrobe = 0;
}

// Re-sends the fog state every two seconds, then sends ONE command: the oldest
// queued raw command if there is one, else the next bank (round robin from the
// last one sent) whose LED byte changed.
void StageKitPoll() {
    if (sTimer.SplitMs() > 2000.0f) {
        StageKitSetFog(sFog);
        sTimer.Restart();
    }
    if (sRawWrite != sRawRead) {
        JoypadStageKitSetRaw(sRaw[sRawRead].left, sRaw[sRawRead].right);
        sRawRead = (sRawRead + 1) % kRawSize;
        return;
    }
    static const unsigned char kBankCmd[kNumBanks] = { 0x20, 0x40, 0x60, 0x80 };
    int start = sLastBank;
    int i = start;
    do {
        i = (i + 1) % kNumBanks;
        if (sLedSent[i] != sLedOut[i]) {
            sLedSent[i] = sLedOut[i];
            JoypadStageKitSetRaw(sLedOut[i], kBankCmd[i]);
            break;
        }
    } while (i != start);
    sLastBank = i;
}

namespace {
    // The thirteen script functions, in StageKitInit's registration order.

    DataNode OnSetFog(DataArray *a) { // 0x82522068
        StageKitSetFog(a->Int(1) != 0);
        return 0;
    }

    DataNode OnSetStrobe(DataArray *a) { // 0x825220C0
        StageKitSetStrobe(a->Int(1));
        return 0;
    }

    DataNode OnSetLeds(DataArray *a) { // 0x82522130
        SetLedOut(a->Int(1));
        return 0;
    }

    DataNode OnLedShift(DataArray *a) { // 0x82522338: (stagekit_led_shift bank steps)
        int steps = a->Int(2);
        int bank = a->Int(1);
        if (steps < 0)
            steps += 8;
        for (; steps > 0; steps--)
            sBankPattern[bank] = RotateStep(sBankPattern[bank]);
        StageKitUpdateLeds();
        return 0;
    }

    DataNode OnSetBankMode(DataArray *a) { // 0x82522218
        int bank = a->Int(1);
        StageKitSetLedState(bank, a->Int(2));
        StageKitUpdateLeds();
        return 0;
    }

    DataNode OnSetBankPattern(DataArray *a) { // 0x82522278 (reads the pattern first)
        int pattern = a->Int(2);
        StageKitSetLedPattern(a->Int(1), pattern);
        StageKitUpdateLeds();
        return 0;
    }

    DataNode OnSetBankState(DataArray *a) { // 0x825222D0
        int bank = a->Int(1);
        sBankOn[bank] = a->Int(2) != 0;
        StageKitUpdateLeds();
        return 0;
    }

    DataNode OnGetBankMode(DataArray *a) { // 0x825221B8
        return sBankMode[a->Int(1)];
    }

    DataNode OnPause(DataArray *a) { // 0x82522408
        StageKitSetFog(false);
        if (a->Int(1))
            StageKitReset();
        else
            StageKitUpdateLeds();
        return 0;
    }

    DataNode OnReset(DataArray *) { // 0x825223C0
        StageKitReset();
        return 0;
    }

    DataNode OnSetModeState(DataArray *a) { // 0x82522460: every bank in `mode` on/off
        int mode = a->Int(1);
        bool on = a->Int(2) != 0;
        for (int i = 0; i < kNumBanks; i++) {
            if (sBankMode[i] == mode)
                sBankOn[i] = on;
        }
        StageKitUpdateLeds();
        return 0;
    }

    DataNode OnPresent(DataArray *) { // 0x825224F0
        return (int)(JoypadStageKitPadNum() != -1);
    }

    // 0x82522540: banks in mode `left` step their pattern one way (seven
    // right-rotations, i.e. one left), banks in mode `right` the other.
    DataNode OnLeftRight(DataArray *a) {
        int left = a->Int(1);
        int right = a->Int(2);
        for (int i = 0; i < kNumBanks; i++) {
            if (sBankMode[i] == left) {
                for (int n = 0; n < 7; n++)
                    sBankPattern[i] = RotateStep(sBankPattern[i]);
            } else if (sBankMode[i] == right) {
                sBankPattern[i] = RotateStep(sBankPattern[i]);
            }
        }
        StageKitUpdateLeds();
        return 0;
    }
}

void StageKitInit() {
    sRawWrite = 0;
    sRawRead = 0;
    sTimer.Restart();
    DataRegisterFunc("stagekit_set_fog", OnSetFog);
    DataRegisterFunc("set_stagekit_strobe", OnSetStrobe);
    DataRegisterFunc("set_stagekit_leds", OnSetLeds);
    DataRegisterFunc("stagekit_led_shift", OnLedShift);
    DataRegisterFunc("stagekit_set_bank_mode", OnSetBankMode);
    DataRegisterFunc("stagekit_set_bank_pattern", OnSetBankPattern);
    DataRegisterFunc("stagekit_set_bank_state", OnSetBankState);
    DataRegisterFunc("stagekit_get_bank_mode", OnGetBankMode);
    DataRegisterFunc("stagekit_pause", OnPause);
    DataRegisterFunc("stagekit_reset", OnReset);
    DataRegisterFunc("stagekit_set_mode_state", OnSetModeState);
    DataRegisterFunc("stagekit_present", OnPresent);
    DataRegisterFunc("stagekit_left_right", OnLeftRight);
}
