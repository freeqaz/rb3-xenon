#include "os/Joypad_Xbox.h"
#include "obj/Data.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/Joypad_Xinput.h"
#include "os/System.h"
#include "xdk/XAPILIB.h"
#include <cstring>

// XInput2 ("sample") surface: retail imports these unmangled (XInput2Sample
// 0x8284E388, XInput2GetDeviceId 0x8284E6A0, XInput2GetDWord 0x8284E730,
// XInput2BeginUpdate 0x8284E768, XInput2EndUpdate 0x8284EBE0, XInput2SetDWord
// 0x8284EE28). Ids are passed by value -- two `ld`s into r4/r5.
extern "C" {
typedef void *XINPUT2_HANDLE;
typedef struct _XINPUT2_ID {
    ULONGLONG Lo;
    ULONGLONG Hi;
} XINPUT2_ID;
typedef struct _XINPUT2_DEVICE_ID {
    BYTE Id[16];
} XINPUT2_DEVICE_ID;
BOOL XInput2Sample(DWORD dwUserIndex, XINPUT2_HANDLE *phSample, DWORD *pdwFlags);
BOOL XInput2BeginUpdate(XINPUT2_HANDLE hSample);
BOOL XInput2EndUpdate(XINPUT2_HANDLE hSample, BOOL fCancel);
BOOL XInput2SetDWord(XINPUT2_HANDLE hSample, XINPUT2_ID Id, DWORD dwValue);
BOOL XInput2GetDWord(XINPUT2_HANDLE hSample, XINPUT2_ID Id, DWORD *pdwValue);
BOOL XInput2GetDeviceId(XINPUT2_HANDLE hSample, XINPUT2_DEVICE_ID *pDeviceId);
extern const XINPUT2_ID XINPUTID_OUT_UNSPECIFIED_DWORD_0;
extern const XINPUT2_ID XINPUTID_OUT_UNSPECIFIED_DWORD_1;
extern const XINPUT2_ID XINPUTID_UNSPECIFIED_DWORD_0;
extern const XINPUT2_ID XINPUTID_UNSPECIFIED_DWORD_1;
extern const XINPUT2_ID XINPUTID_UNSPECIFIED_DWORD_2;
extern const XINPUT2_ID XINPUTID_UNSPECIFIED_DWORD_3;
extern const XINPUT2_DEVICE_ID XINPUTID_0F_CONTROLLER;
extern const XINPUT2_DEVICE_ID XINPUTID_19_CONTROLLER;
void __emit(unsigned int);
}
#pragma intrinsic(__emit)

// .bss ORDER AND STORAGE CLASS ARE LOAD-BEARING -- read before touching.
// Retail RB3's InitXinputJoypadThreadData (fn_82529890) opens
//   lis r11,0x82cd ; addi r11,r11,-0x4728   => r11 = tInputStates
//   stb r10,0x98(r11) .. stb r10,0x9b(r11)  => tNeedCaps = tInputStates + 0x98
//   addi r7,r9,-0x46e8                      => tBreed   = tInputStates + 0x40
// A BAKED-IN DISPLACEMENT like that is only emitted between INTERNAL-LINKAGE
// statics; MSVC makes anonymous-namespace variables EXTERNAL, and each external
// gets its own relocation instead. So the whole block below is file-scope
// `static`, except the two the target actually names with anon decoration
// (tBreed, tCritSection).
// MSVC lays .bss out in REVERSE declaration order, so the run reads back to
// front: first declared here = highest address. The resulting ascending run is
//   tInputStates +0x00, tBreed +0x40, sThreadData +0x70, tButtonStatesCurr
//   +0x78, tButtonStatesPrev +0x88, tNeedCaps +0x98, tCritSection +0x9c
// which is byte-for-byte the run DC3's Joypad_Xbox.cpp documents -- verified
// here against RETAIL bytes.
namespace {
    CriticalSection tCritSection;
}
// Pad needs its XInput capabilities re-queried before it can be read.
__declspec(align(8)) static bool tNeedCaps[kNumJoypads];
static unsigned int tButtonStatesPrev[kNumJoypads];
static unsigned int tButtonStatesCurr[kNumJoypads];
// Thread handle and termination flag grouped for proper codegen
// The struct layout is required to match the original binary's data layout
static struct {
    HANDLE tThread;
    bool tNoHandle;
} sThreadData;
namespace {
    BreedData tBreed[kNumJoypads];
}
static XINPUT_STATE tInputStates[kNumJoypads];
static unsigned char tRawData[kNumJoypads][16];
static unsigned char tUpstreamData[kNumJoypads][16];
// Set when a pad has an unread upstream response waiting in tUpstreamData.
static bool tRawPending[kNumJoypads];
// Downstream packet staged by SendRawData: report id + seven payload bytes.
static unsigned char tRawOutput[8];

// Macros to access thread data - required for matching symbol offsets
#define tThread sThreadData.tThread
#define tNoHandle sThreadData.tNoHandle

// Retrieves XInput state and button changes since last frame
// Combines translated buttons with accumulated button state, then resets current state
void GetXinputSinceLastFrame(int pad, XINPUT_STATE *state, unsigned int *buttons) {
    CritSecTracker tracker(&tCritSection);
    unsigned int translatedButtons;
    *state = tInputStates[pad];
    TranslateButtons(&translatedButtons, tInputStates[pad].Gamepad.wButtons);
    *buttons = tButtonStatesCurr[pad] | translatedButtons;
    tButtonStatesPrev[pad] = tButtonStatesCurr[pad];
    tButtonStatesCurr[pad] = 0;
}

// Cleanly terminates the XInput polling thread
void XinputJoypadThreadDestruction() {
    tNoHandle = true;
    WaitForSingleObject(tThread, INFINITE);
    CloseHandle(tThread);
    tThread = 0;
}

void JoypadReset() { JoypadResetXboxPC(4); }

void JoypadTerminate() {
    XinputJoypadThreadDestruction();
    JoypadTerminateCommon();
}

void JoypadPoll() { JoypadPollCommon(); }

// Retail 0x82529af0: a single `b XamInputSendStayAliveRequest` (import stub
// fn_82C4BC7C). The map used to name this address
// ReceiveUpstreamCalbertResponse, which cannot be right -- that body logs.
void JoypadSendKeepAlive(int pad_mask) { XamInputSendStayAliveRequest(pad_mask); }

// Retail 0x82529AF8 (extern "C" in Joypad.h): hands back the raw HID report
// last parked by ParseRawData, clears the pending flag, then defers to the
// shared XInput reader.
int ReadSingleJoypad(
    int pad,
    unsigned int *buttons,
    char *lx,
    char *ly,
    char *rx,
    char *ry,
    char *lt,
    char *rt,
    float *sensors,
    float *pressures,
    unsigned char *pro_guitar
) {
    if (pad >= kNumJoypads)
        return kJoypadNone;
    for (int i = 0; i < 16; i++) {
        pro_guitar[i] = tRawData[pad][i];
    }
    if (tRawPending[pad]) {
        tRawPending[pad] = false;
    }
    return ReadSingleXinputJoypad(
        pad, pad, buttons, lx, ly, rx, ry, lt, rt, sensors, pressures, pro_guitar
    );
}

JoypadType SetupHXKeytar(int, const XINPUT_CAPABILITIES &c) {
    if ((c.Gamepad.sThumbLY & 0xFFF0U) == 0x1730) {
        return kJoypadXboxMidiBoxKeyboard;
    } else
        return kJoypadXboxKeytar;
}

void ReceiveUpstreamLowPriorityOutputResponse(int pad, unsigned char *data) {
    MILO_LOG("Low Priority Output Report for controller %d:\n", pad);
    MILO_LOG("0x%02x 0x%02x 0x%02x\n", data[1], data[2], data[3]);
}

void ReceiveUpstreamBreedDataResponse(int pad, unsigned char *data) {
    // retail (0x825299B8) has no JoypadGetPadData/mConnected test: the logging
    // block compiled away whole, leaving only the tBreed stores + the handler.
#ifdef HX_NATIVE
    if (JoypadGetPadData(pad)->mConnected) {
        MILO_LOG("Breed Data Response for controller %d\n", pad);
        MILO_LOG(
            "Vendor:      0x%02x\nProject:     0x%02x\nPeriph Type: 0x%02x\nPlatform:    0x%02x\nFactory:     0x%02x\nDesign Iter: 0x%02x\nManu Date(1):0x%02x\nManu Date(2):0x%02x\nIdent. v(1): 0x%02x\nIdent. v(2): 0x%02x\n",
            data[1],
            data[2],
            data[3],
            data[4],
            data[5],
            data[6],
            data[7],
            data[8],
            data[9],
            data[10]
        );
    }
#endif
    tBreed[pad].mVendor = data[1];
    tBreed[pad].mProject = data[2];
    tBreed[pad].mPeripheralType = data[3];
    tBreed[pad].mPlatform = data[4];
    tBreed[pad].mFactory = data[5];
    tBreed[pad].mDesignIter = data[6];
    tBreed[pad].mManuDate = data[8] * 0x100 + data[7];
    tBreed[pad].mIdent = data[10] * 0x100 + data[9];
    tBreed[pad].mPending = 0;
    JoypadHandleBreedDataResponse(pad);
}

void ReceiveUpstreamCalbertResponse(int pad, unsigned char *data) {
    MILO_LOG("Calbert Response for controller %d\n", pad);
    MILO_LOG("Sensor Output Mode: 0x%02x\n", data[1]);
}

void ReceiveUpstreamAccelerometerResponse(int pad, unsigned char *data) {
    MILO_LOG("Accelerometer Mode Response for controller %d\n", pad);
    MILO_LOG(
        "Accelerometer Output Mode: 0x%02x\nX axis resolution:         0x%02x\nY axis resolution:         0x%02x\nZ axis resolution:         0x%02x\n",
        data[1],
        data[2],
        data[3],
        data[4]
    );
}

void ReceiveUpstreamOutputModeResponse(int pad, unsigned char *data) {
    MILO_LOG("Output Mode Switch Response for controller %d\n", pad);
    MILO_LOG("Output Mode: 0x%02x\n", data[1]);
}

void ReceiveUpstreamDeviceStateResponse(int pad, unsigned char *data) {
    MILO_LOG("Device State Response for controller %d\n", pad);
    MILO_LOG("Battery Level: 0x%02x\nOutput Mode:   0x%02x\n", data[1], data[2]);
}

void ReceiveUpstreamEEPROMReadResponse(int pad, unsigned char *data) {
    MILO_LOG("EEPROM Read Response for controller %d\n", pad);
    MILO_LOG(
        "Offset (low):      0x%02x\nOffset (high):     0x%02x\nData Length:       0x%02x\n",
        data[1],
        data[2],
        data[3]
    );
    MILO_LOG(
        "Packet Payload Len:0x%02x\nEEPROM Data(1):    0x%02x\nEEPROM Data(2):    0x%02x\nEEPROM Data(3):    0x%02x\nEEPROM Data(4):    0x%02x\nEEPROM Data(5):    0x%02x\nEEPROM Data(6):    0x%02x\nEEPROM Data(7):    0x%02x\nEEPROM Data(8):    0x%02x\n",
        data[5],
        data[6],
        data[7],
        data[8],
        data[9],
        data[10],
        data[11],
        data[12],
        data[13]
    );
}

void ReceiveUpstreamEEPROMWriteResponse(int pad, unsigned char *data) {
    MILO_LOG("EEPROM Write Response for controller %d\n", pad);
    MILO_LOG(
        "Offset (low):       0x%02x\nOffset (high):      0x%02x\nData Length:        0x%02x\nStatus:             0x%02x\n",
        data[1],
        data[2],
        data[3],
        data[4]
    );
    MILO_LOG(
        "Packet Payload Len: 0x%02x\nEEPROM Data Echo(1):0x%02x\nEEPROM Data Echo(2):0x%02x\nEEPROM Data Echo(3):0x%02x\nEEPROM Data Echo(4):0x%02x\nEEPROM Data Echo(5):0x%02x\nEEPROM Data Echo(6):0x%02x\nEEPROM Data Echo(7):0x%02x\nEEPROM Data Echo(8):0x%02x\n",
        data[5],
        data[6],
        data[7],
        data[8],
        data[9],
        data[10],
        data[11],
        data[12],
        data[13]
    );
    JoypadHandleEepromWriteResponse(pad, (JoypadBreedDataStatus)(data[4] != 0));
}

// Retail 0x82529BC0: stages an eight byte downstream HID report (report id
// 0x11 plus seven payload bytes) and pushes it as two DWORD writes. The log
// calls survive only as their GetLastError() argument evaluations.
void SendRawData(
    int pad,
    unsigned char b1,
    unsigned char b2,
    unsigned char b3,
    unsigned char b4,
    unsigned char b5,
    unsigned char b6,
    unsigned char b7
) {
    XINPUT2_HANDLE sample;
    DWORD flags;
    if (!XInput2Sample(pad, &sample, &flags)) {
        MILO_LOG(
            "No sample available in SendRawData, error 0x%08\n",
            (unsigned int)GetLastError()
        );
        return;
    }
    tRawOutput[0] = 0x11;
    tRawOutput[1] = b1;
    tRawOutput[2] = b2;
    tRawOutput[3] = b3;
    tRawOutput[4] = b4;
    tRawOutput[5] = b5;
    tRawOutput[6] = b6;
    tRawOutput[7] = b7;
    XInput2BeginUpdate(sample);
    if (!XInput2SetDWord(
            sample, XINPUTID_OUT_UNSPECIFIED_DWORD_0, ((DWORD *)tRawOutput)[0]
        )) {
        MILO_LOG("Error 0x%08x writing data 0\n", (unsigned int)GetLastError());
    } else if (!XInput2SetDWord(
                   sample, XINPUTID_OUT_UNSPECIFIED_DWORD_1, ((DWORD *)tRawOutput)[1]
               )) {
        MILO_LOG("Error 0x%08x writing data 1\n", (unsigned int)GetLastError());
    }
    XInput2EndUpdate(sample, 0);
}

BreedData *GetBreedData(int pad) {
    if (tBreed[pad].mPending) {
        SendRawData(pad, 0x81, 0, 0, 0, 0, 0, 0);
        return nullptr;
    } else {
        return &tBreed[pad];
    }
}

bool requestBreedWrite(int pad, unsigned char *pBreedWritePacket) {
    MILO_ASSERT(pBreedWritePacket, 0x301);
    SendRawData(
        pad,
        0xF3,
        pBreedWritePacket[0],
        pBreedWritePacket[1],
        pBreedWritePacket[2],
        pBreedWritePacket[3],
        pBreedWritePacket[4],
        pBreedWritePacket[5]
    );
    return true;
}

JoypadType SetupHXRealGuitar(int pad, const XINPUT_CAPABILITIES &c) {
    unsigned short us = (unsigned short)c.Gamepad.sThumbLY & 0xFFF0;
    bool u1 = us == 0x1530;
    bool u2 = us == 0x1430;
    if (!u1 && !u2)
        u2 = true;
    if (u1) {
        return kJoypadXboxRealGuitar22Fret;
    } else if (u2) {
        return kJoypadXboxButtonGuitar;
    } else {
        MILO_LOG("sThymbLY = %d does not correspond to subtype x19\n", c.Gamepad.sThumbLY);
        return kJoypadAnalog;
    }
}

JoypadType SetupHXGuitar(int pad, const XINPUT_CAPABILITIES &c) {
    bool u5 = c.Flags & 0x2;
    bool u1 = c.Flags & 1;
    bool u4 = u5 && (u1 || c.Gamepad.sThumbRX >= 0x100);
    JoypadGetPadData(pad)->mIsWireless = u5; // wireless?
    JoypadGetPadData(pad)->mHasCapFlag1 = u1;
    if (c.Gamepad.sThumbLX == 0x1BAD) {
        GetBreedData(pad);
        return kJoypadXboxCoreGuitar;
    } else
        return u4 ? kJoypadXboxHxGuitarRb2 : kJoypadXboxHxGuitar;
}

// Identifies drum controller type based on XInput capabilities.
// RB2 drums are identified by flag 1 or a high sThumbRX value; Rock of Ages
// drums by flag 2 without flag 1.
JoypadType SetupHXDrums(int pad, const XINPUT_CAPABILITIES &c) {
    bool hasFlag1 = c.Flags & 1;
    bool hasFlag2 = c.Flags & 0x2;
    bool isRb2Drums = hasFlag1 || c.Gamepad.sThumbRX >= 0x100;
    bool isRockOfAgesDrums = hasFlag2 && !hasFlag1;
    JoypadGetPadData(pad)->mIsWireless = hasFlag2;
    JoypadGetPadData(pad)->mHasCapFlag1 = hasFlag1;
    if (c.Gamepad.sThumbLX == 0x1BAD) {
        GetBreedData(pad);
        return kJoypadXboxMidiBoxDrums;
    }
    if (isRb2Drums) {
        return kJoypadXboxDrumsRb2;
    }
    if (isRockOfAgesDrums) {
        return kJoypadXboxRoDrums;
    }
    return kJoypadXboxDrums;
}

bool ReceiveUpstreamResponse(int pad, unsigned char *data) {
    switch (data[0]) {
    case 0x80:
        ReceiveUpstreamLowPriorityOutputResponse(pad, data);
        break;
    case 0x82:
        ReceiveUpstreamBreedDataResponse(pad, data);
        break;
    case 0x84:
        ReceiveUpstreamCalbertResponse(pad, data);
        break;
    case 0x86:
        ReceiveUpstreamAccelerometerResponse(pad, data);
        break;
    case 0x8A:
        ReceiveUpstreamOutputModeResponse(pad, data);
        break;
    case 0xC4:
        ReceiveUpstreamDeviceStateResponse(pad, data);
        break;
    case 0xF2:
        ReceiveUpstreamEEPROMReadResponse(pad, data);
        break;
    case 0xF4:
        ReceiveUpstreamEEPROMWriteResponse(pad, data);
        break;
    default:
        return false;
    }
    return true;
}

// Retail 0x8252A000: stashes the 16-byte HID report the pad just sent. A report
// with bit 7 of byte 14 set is an upstream response to a downstream command:
// it goes to the upstream mailbox (behind an lwsync) and is dispatched at once.
// Everything else is the per-frame raw state ReadSingleJoypad hands back.
bool ParseRawData(int pad, unsigned char *data) {
    if ((data[14] & 0x80) == 0x80) {
        for (int i = 0; i < 16; i++) {
            tUpstreamData[pad][i] = data[i];
        }
        __emit(0x7c2004ac); // lwsync
        tRawPending[pad] = true;
        if (ReceiveUpstreamResponse(pad, data))
            return true;
    }
    for (int i = 0; i < 16; i++) {
        tRawData[pad][i] = data[i];
    }
    return false;
}

namespace {
    // Puts every pad into the "nothing known yet" state the polling loop
    // expects: capabilities must be re-queried, the breed data is stale, and
    // no XInput packet has been seen. Retail unrolls the first loop into four
    // stb at 0x98(r11)..0x9b(r11) and runs the second as a stbu/stwu pair.
    void InitXinputJoypadThreadData() {
        for (int i = 0; i < kNumJoypads; i++) {
            tNeedCaps[i] = true;
        }
        for (int i = 0; i < kNumJoypads; i++) {
            tBreed[i].mPending = true;
            tInputStates[i].dwPacketNumber = 0;
        }
    }

    void RunXinputJoypadLoop();

    DWORD XinputJoypadThreadEntry(HANDLE) {
        InitXinputJoypadThreadData();
        RunXinputJoypadLoop();
        return 0;
    }

    // Retail 0x8252A0B8 (its unwind funclet 0x8252A384 releases the tracker).
    // Polls every pad until XinputJoypadThreadDestruction sets tNoHandle; the
    // whole sweep runs under tCritSection.
    void RunXinputJoypadLoop() {
        while (!tNoHandle) {
            {
                CritSecTracker tracker(&tCritSection);
                for (int pad = 0; pad < kNumJoypads; pad++) {
                    XINPUT_STATE state;
                    if (XInputGetState(pad, &state) != 0) {
                        // Nothing plugged in: force a capability re-query and a
                        // breed re-read for when it comes back.
                        tBreed[pad].mPending = true;
                        tInputStates[pad].dwPacketNumber = -1;
                        tNeedCaps[pad] = true;
                        continue;
                    }
                    if (state.dwPacketNumber == tInputStates[pad].dwPacketNumber
                        && tInputStates[pad].dwPacketNumber != -1
                        && JoypadGetPadData(pad)->mConnected) {
                        continue;
                    }
                    bool consumed = false;
                    XINPUT_CAPABILITIES caps;
                    if (JoypadGetCachedXInputCaps(pad, &caps, tNeedCaps[pad])) {
                        tNeedCaps[pad] = false;
                        XINPUT2_HANDLE sample;
                        DWORD flags;
                        if (!XInput2Sample(pad, &sample, &flags)) {
                            MILO_LOG("No sample available in RunXinputJoypadLoop\n");
                            continue;
                        }
                        XINPUT2_DEVICE_ID deviceId;
                        if (!XInput2GetDeviceId(sample, &deviceId)) {
                            MILO_LOG("Error getting device ID\n");
                            continue;
                        }
                        // Only the two Harmonix peripheral classes carry a raw
                        // HID payload worth reading.
                        if (memcmp(&deviceId, &XINPUTID_0F_CONTROLLER, 16) == 0
                            || memcmp(&deviceId, &XINPUTID_19_CONTROLLER, 16) == 0) {
                            unsigned char raw[16];
                            if (XInput2GetDWord(
                                    sample, XINPUTID_UNSPECIFIED_DWORD_0, (DWORD *)&raw[0]
                                )
                                && XInput2GetDWord(
                                    sample, XINPUTID_UNSPECIFIED_DWORD_1, (DWORD *)&raw[4]
                                )
                                && XInput2GetDWord(
                                    sample, XINPUTID_UNSPECIFIED_DWORD_2, (DWORD *)&raw[8]
                                )
                                && XInput2GetDWord(
                                    sample, XINPUTID_UNSPECIFIED_DWORD_3, (DWORD *)&raw[12]
                                )) {
                                consumed = ParseRawData(pad, raw);
                            } else {
                                MILO_LOG("Error reading data\n");
                            }
                        }
                    }
                    if (consumed) {
                        // The report was an upstream response, not pad state.
                        tInputStates[pad].dwPacketNumber = state.dwPacketNumber;
                        continue;
                    }
                    unsigned int translated;
                    TranslateButtons(&translated, state.Gamepad.wButtons);
                    tInputStates[pad] = state;
                    tButtonStatesCurr[pad] |=
                        (tButtonStatesPrev[pad] ^ translated) & translated;
                }
            }
            Sleep(4);
        }
    }
}

void XinputJoypadThreadStart() {
    tThread = CreateThread(nullptr, 0, XinputJoypadThreadEntry, nullptr, 4, nullptr);
    MILO_ASSERT(tThread, 0x266);
    SetThreadPriority(tThread, 2);
    // Retail pins the XInput poll thread to hardware thread 4 (fn_8252A3D8
    // emits `li r4, 0x4` here), not 1.
    XSetThreadProcessor(tThread, 4);
    ResumeThread(tThread);
}

// Retail 0x82529AE8 is a lone `b` into the XInput vibration setter.
void JoypadSetActuatorsImp(int pad, int left, int right) {
    JoypadSetXinputActuators(pad, left, right);
}

// Retail 0x82529C98 (extern "C" in Joypad.h): Calbert-capable pads (types
// 0x1e..0x2e) take the mode as a downstream 0x83 report; anything else
// approximates it with the rumble motors.
void JoypadSetCalbertMode(int pad, int mode) {
    int type = JoypadGetPadData(pad)->mType;
    if (type >= 0x1e && type <= 0x2e) {
        SendRawData(pad, 0x83, mode, 0, 0, 0, 0, 0);
    } else {
        JoypadSetXinputCalbertMode(pad, mode);
    }
}

void JoypadInit() {
    DataArray *cfg = SystemConfig("joypad");
    JoypadInitCommon(cfg);
    JoypadInitXboxPCDeadzone(cfg);
    JoypadReset();
    XinputJoypadThreadStart();
}
