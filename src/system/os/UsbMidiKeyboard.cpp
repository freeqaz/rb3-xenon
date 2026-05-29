#include "os/UsbMidiKeyboard.h"
#include "decomp.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/UsbMidiKeyboardMsgs.h"

UsbMidiKeyboard *TheKeyboard;
bool UsbMidiKeyboard::mUsbMidiKeyboardExists = false;

namespace {
    bool gUseMidiPort = false;
    bool gForceDetectKeytar = false;
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

    for (int i = 0; (unsigned int)i < 4; i++) {
        JoypadType ty = JoypadGetPadData(i)->mType;
        if (ty == kJoypadXboxMidiBoxKeyboard || ty == kJoypadPs3MidiBoxKeyboard
            || ty == kJoypadWiiMidiBoxKeyboard || ty == kJoypadXboxKeytar
            || ty == kJoypadPs3Keytar || ty == kJoypadWiiKeytar
            || gForceDetectKeytar) {
            ProKeysData *proData =
                (ProKeysData *)&JoypadGetPadData(i)->mProGuitarData;

            int slotCounter = 1;
            for (int note = 0x30; note < 73; note++) {
                int keyIndex = note - 0x30;
                int byteIdx = keyIndex / 8;
                int bitIdx = 7 - (keyIndex % 8);
                bool pressed = (proData->unk0[byteIdx] >> bitIdx) & 1;

                bool storedPressed = TheKeyboard->GetKeyPressed(i, note);

                if (pressed != storedPressed) {
                    if (pressed) {
                        int extVel = TheKeyboard->GetSlottedKeyVelocityFromExtended(
                            slotCounter++, proData->unk0
                        );
                        TheKeyboard->SetKeyVelocity(i, note, extVel);
                        auto _tmp2 = TheKeyboard->GetKeyVelocity(i, note);
                        auto _tmp0 = KeyboardKeyPressedMsg(note, _tmp2, i);
                        SendMessage(
                            _tmp0
                        );
                    } else {
                        TheKeyboard->SetKeyVelocity(i, note, 0);
                        auto _tmp3 = KeyboardKeyReleasedMsg(note, i);
                        SendMessage(_tmp3);
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
                SendMessage(KeyboardSustainMsg(sus, i));
            }

            bool stomped = proData->mStompPedal;
            if (stomped != TheKeyboard->GetStompPedal(i)) {
                TheKeyboard->SetStompPedal(i, stomped);
                SendMessage(KeyboardStompBoxMsg(stomped, i));
            }

            int mod = proData->unkachar;
            if (mod != TheKeyboard->GetModVal(i)) {
                TheKeyboard->SetModVal(i, mod);
                SendMessage(KeyboardModMsg(mod, i));
            }

            int exp = proData->mExpressionPedal;
            if (exp != TheKeyboard->GetExpressionPedal(i)) {
                TheKeyboard->SetExpressionPedal(i, exp);
                SendMessage(KeyboardExpressionPedalMsg(exp, i));
            }

            int conn = proData->mConnectedAccessories;
            if (conn != TheKeyboard->GetConnectedAccessory(i)) {
                TheKeyboard->SetConnectedAccessories(i, conn);
                SendMessage(KeyboardConnectedAccessoriesMsg(conn, i));
            }

            int lowhand = proData->mLowHandPlacement;
            if (lowhand != TheKeyboard->GetLowHandPlacement(i)) {
                TheKeyboard->SetLowHandPlacement(i, lowhand);
                SendMessage(KeyboardLowHandPlacementMsg(lowhand, i));
            }

            int highhand = proData->unkbbool + (proData->unkcbool << 1)
                + (proData->unkdbool << 2) + (proData->unkemiddle << 3);
            if (highhand != TheKeyboard->GetHighHandPlacement(i)) {
                TheKeyboard->SetHighHandPlacement(i, highhand);
                SendMessage(KeyboardHighHandPlacementMsg(highhand, i));
            }

            int accelAxisVal0 = proData->unkachar;
            int accelAxisVal1 = proData->unkbchar;
            int accelAxisVal2 = proData->unkcchar;
            if (accelAxisVal0 != TheKeyboard->GetAccelAxisVal(i, 0)
                || accelAxisVal1 != TheKeyboard->GetAccelAxisVal(i, 1)
                || accelAxisVal2 != TheKeyboard->GetAccelAxisVal(i, 2)) {
                TheKeyboard->SetAccelerometer(i, accelAxisVal0, accelAxisVal1, accelAxisVal2);
                auto _tmp1 = KeysAccelerometerMsg(accelAxisVal0, accelAxisVal1, accelAxisVal2, i);
                SendMessage(
                    _tmp1
                );
            }
        }
    }
}
