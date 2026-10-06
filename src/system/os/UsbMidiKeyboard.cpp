#include "os/UsbMidiKeyboard.h"
#include "decomp.h"
#include "os/Debug.h"
#include "os/System.h"
#include "os/Joypad.h"
#include "os/UsbMidiKeyboardMsgs.h"

UsbMidiKeyboard *TheKeyboard;
bool UsbMidiKeyboard::mUsbMidiKeyboardExists = false;

namespace {
    bool gUseMidiPort = false;
    bool gForceDetectKeytar = false;
}

// Retail 0x8251AB80: a function-static instance (its guard's unwind thunk is
// 0x8251ABE8; the ctor reduces to CriticalSection's, the dtor goes to atexit).
StaticCriticalSection *StaticCriticalSection::Instance() {
    static StaticCriticalSection gCrit;
    return &gCrit;
}

StaticCriticalSection::~StaticCriticalSection() {}

StaticCriticalSection::StaticCriticalSection() {}

// Retail 0x8251AAF0: mPadNum, the exists flag, then per pad the scalar slots
// and the 128 key/velocity pairs.
UsbMidiKeyboard::UsbMidiKeyboard() {
    mPadNum = 0;
    mUsbMidiKeyboardExists = true;
    int j;
    for (int i = 0; i < 4; i++) {
        mSustain[i] = false;
        mStompPedal[i] = false;
        mModVal[i] = 0;
        mExpressionPedal[i] = 0;
        mConnectedAccessories[i] = 0;
        for (j = 0; j < 128; ++j) {
            mKeyPressed[i][j] = false;
            mKeyVelocity[i][j] = 0;
        }
    }
}

// Inlined into Terminate (0x8251B230): an empty Enter/Exit of the shared lock.
UsbMidiKeyboard::~UsbMidiKeyboard() {
    CritSecTracker cst(StaticCriticalSection::Instance());
}

// Retail 0x8251B288. Both Symbols are function-local statics (one guard word,
// bits 1 and 2, in this order).
void UsbMidiKeyboard::Init() {
    static Symbol use_midi_mode("use_midi_mode");
    static Symbol joypad("joypad");
    MILO_ASSERT(TheKeyboard == NULL, 0x58);
    TheKeyboard = new UsbMidiKeyboard();
    TheDebug.AddExitCallback(UsbMidiKeyboard::Terminate);
    gUseMidiPort = SystemConfig(joypad)->FindInt(use_midi_mode);
    gForceDetectKeytar = false;
}

// Retail 0x8251B230.
void UsbMidiKeyboard::Terminate() {
    MILO_ASSERT(TheKeyboard != NULL, 0x65);
    RELEASE(TheKeyboard);
}

bool UsbMidiKeyboard::GetSustain(int pad) {
    return mSustain[pad];
}

int UsbMidiKeyboard::GetSlottedKeyVelocityFromExtended(int i, unsigned char *uc) {
    if (gUseMidiPort)
        return 0;
    if (i >= 1 && i <= 5) {
        switch (i) {
        case 1:
            return uc[3] & 0x7F;
        case 2:
            return uc[4] & 0x7F;
        case 3:
            return uc[5] & 0x7F;
        case 4:
            return uc[6] & 0x7F;
        case 5:
            return uc[7] & 0x7F;
        }
    }
    return 0;
}

void UsbMidiKeyboard::Poll() {
    if (gUseMidiPort)
        return;
    if (!TheKeyboard)
        return;

    for (int i = 0; i < 4; i++) {
        JoypadType ty = JoypadGetPadData(i)->mType;
        if (ty == kJoypadXboxMidiBoxKeyboard || ty == kJoypadPs3MidiBoxKeyboard
            || ty == kJoypadWiiMidiBoxKeyboard || ty == kJoypadXboxKeytar
            || ty == kJoypadPs3Keytar || ty == kJoypadWiiKeytar
            || gForceDetectKeytar) {
            ProKeysData *proData =
                (ProKeysData *)&JoypadGetPadData(i)->mProGuitarData;
            // Residual (DC3 lane w7-r): retail emits slotCounter's `li 1` between
            // `bl JoypadGetPadData` and the proData `addi`; we emit it after the addi.
            // Declaring slotCounter first hoists the `li` above the call instead, so no
            // declaration order lands it in retail's slot.
            int slotCounter = 1;

            for (int note = 0x30; note - 0x30 < 25; note++) {
                bool pressed = (proData->unk0[(note - 0x30) / 8]
                                >> (7 - (note - 0x30) % 8))
                    & 1;

                bool storedPressed = TheKeyboard->GetKeyPressed(i, note);

                if (pressed != storedPressed) {
                    if (pressed) {
                        int extVel = TheKeyboard->GetSlottedKeyVelocityFromExtended(
                            slotCounter, proData->unk0
                        );
                        TheKeyboard->SetKeyVelocity(i, note, extVel);
                        slotCounter++;
                        KeyboardKeyPressedMsg msg(
                            note, TheKeyboard->GetKeyVelocity(i, note), i
                        );
                        SendMessage(msg);
                    } else {
                        TheKeyboard->SetKeyVelocity(i, note, 0);
                        KeyboardKeyReleasedMsg msg(note, i);
                        SendMessage(msg);
                    }
                    TheKeyboard->SetKeyPressed(i, note, pressed);
                } else {
                    if (pressed)
                        slotCounter++;
                }
            }

            bool sus = proData->mSustain;
            if (sus != TheKeyboard->GetSustain(i)) {
                TheKeyboard->SetSustain(i, sus);
                KeyboardSustainMsg msg(sus, i);
                SendMessage(msg);
            }

            bool stomped = proData->mStompPedal;
            if (stomped != TheKeyboard->GetStompPedal(i)) {
                TheKeyboard->SetStompPedal(i, stomped);
                KeyboardStompBoxMsg msg(stomped, i);
                SendMessage(msg);
            }

            int mod = proData->unkachar;
            if (mod != TheKeyboard->GetModVal(i)) {
                TheKeyboard->SetModVal(i, mod);
                KeyboardModMsg msg(mod, i);
                SendMessage(msg);
            }

            int exp = proData->mExpressionPedal;
            if (exp != TheKeyboard->GetExpressionPedal(i)) {
                TheKeyboard->SetExpressionPedal(i, exp);
                KeyboardExpressionPedalMsg msg(exp, i);
                SendMessage(msg);
            }

            int conn = proData->mConnectedAccessories;
            if (conn != TheKeyboard->GetConnectedAccessory(i)) {
                TheKeyboard->SetConnectedAccessories(i, conn);
                KeyboardConnectedAccessoriesMsg msg(conn, i);
                SendMessage(msg);
            }

            int lowhand = proData->mLowHandPlacement;
            if (lowhand != TheKeyboard->GetLowHandPlacement(i)) {
                TheKeyboard->SetLowHandPlacement(i, lowhand);
                KeyboardLowHandPlacementMsg msg(lowhand, i);
                SendMessage(msg);
            }

            // NOTE: the image emits four separate fused rlwinm extractions and a
            // flat add chain (d<<2) + (c<<1) + (e<<3) + b; MSVC here reassociates
            // any spelling of this sum into a Horner chain instead. Tried '|',
            // explicit sub-grouping and term reordering -- all identical or worse.
            int hhB = proData->unkbbool;
            int hhE = proData->unkemiddle << 3;
            int hhD = proData->unkdbool << 2;
            int hhC = proData->unkcbool << 1;
            int highhand = hhD + hhC + hhE + hhB;
            if (highhand != TheKeyboard->GetHighHandPlacement(i)) {
                TheKeyboard->SetHighHandPlacement(i, highhand);
                KeyboardHighHandPlacementMsg msg(highhand, i);
                SendMessage(msg);
            }

            int accelAxisVal0 = proData->unkachar;
            int accelAxisVal1 = proData->unkbchar;
            int accelAxisVal2 = proData->unkcchar;
            if (accelAxisVal0 != TheKeyboard->GetAccelAxisVal(i, 0)
                || accelAxisVal1 != TheKeyboard->GetAccelAxisVal(i, 1)
                || accelAxisVal2 != TheKeyboard->GetAccelAxisVal(i, 2)) {
                TheKeyboard->SetAccelerometer(i, accelAxisVal0, accelAxisVal1, accelAxisVal2);
                KeysAccelerometerMsg msg(
                    accelAxisVal0, accelAxisVal1, accelAxisVal2, i
                );
                SendMessage(msg);
            }
        }
    }
}
