#include "gesture/WaveToTurnOnLight.h"
#include "flow/PropertyEventProvider.h"
#include "hamobj/HamDirector.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "xdk/NUI.h"
#include "xdk/xapilibi/winerror.h"

WaveToTurnOnLight *TheWaveToTurnOnLight;

WaveToTurnOnLight::WaveToTurnOnLight()
    : mWaveProgress(0), mWaveStateEnabled(0), mPaused(0), mSkeletonID(0) {}

BEGIN_HANDLERS(WaveToTurnOnLight)
    HANDLE_ACTION(switch_wave_state, SwitchWaveState())
    HANDLE_ACTION(enable_wave_state, EnableWaveState())
    HANDLE_ACTION(disable_wave_state, DisableWaveState())
END_HANDLERS

void WaveToTurnOnLight::Init() {
    MILO_ASSERT(!TheWaveToTurnOnLight, 0x1C);
    TheWaveToTurnOnLight = new WaveToTurnOnLight();
    TheWaveToTurnOnLight->SetName("wave_to_turn_on_light", ObjectDir::Main());
}

void WaveToTurnOnLight::PrintError(long err, const char *msg) {
    switch (err) {
    case E_NUI_ALREADY_INITIALIZED:
        MILO_LOG("%s failed with E_NUI_ALREADY_INITIALIZED\n", msg);
        break;
    case E_OUTOFMEMORY:
        MILO_LOG("%s failed with E_OUTOFMEMORY\n", msg);
        break;
    case E_NUI_DEVICE_NOT_READY:
        MILO_LOG("%s failed with E_NUI_DEVICE_NOT_READY\n", msg);
        break;
    case E_INVALIDARG:
        MILO_LOG("%s failed with E_INVALIDARG\n", msg);
        break;
    case E_NUI_DEVICE_NOT_CONNECTED:
        MILO_LOG("%s failed with E_NUI_DEVICE_NOT_CONNECTED\n", msg);
        break;
    case E_NUI_IMAGE_STREAM_IN_USE:
        MILO_LOG("%s failed with E_NUI_IMAGE_STREAM_IN_USE\n", msg);
        break;
    case E_NUI_FEATURE_NOT_INITIALIZED:
        MILO_LOG("%s failed with E_NUI_FEATURE_NOT_INITIALIZED\n", msg);
        break;
    case E_NUI_DATABASE_NOT_FOUND:
        MILO_LOG("%s failed with E_NUI_DATABASE_NOT_FOUND\n", msg);
        break;
    case E_NUI_DATABASE_VERSION_MISMATCH:
        MILO_LOG("%s failed with E_NUI_DATABASE_VERSION_MISMATCH\n", msg);
        break;
    default:
        long err_unknown = err;
        MILO_LOG("%s failed with 0x%x\n", msg, err_unknown);
        break;
    }
}

void WaveToTurnOnLight::SetPaused(bool paused) {
    mPaused = paused;
    if (mPaused) {
        mTimer.Pause();
    } else {
        mTimer.Resume();
    }
}

void WaveToTurnOnLight::EnableWaveState() {
    if (!mWaveStateEnabled) {
#ifdef HX_NATIVE
        // No Kinect on native/web — skip the wave gesture flow entirely.
        // On web, NuiWaveSetEnabled is an auto-generated Emscripten stub that
        // returns 0 (ERROR_SUCCESS), which would arm multi-intro mode and
        // permanently block StartGame() via mGameStartHold.
        mPaused = false;
        return;
#endif
        HRESULT res = NuiWaveSetEnabled(true);
        if (res == ERROR_SUCCESS) {
            mWaveStateEnabled = true;
            MILO_LOG("Wave detection enabled\n");
            static Message cWaveGestureEnabledMsg("wave_gesture_enabled");
            Export(cWaveGestureEnabledMsg, true);
            mTimer.Restart();
            TheHamDirector->ArmMultiIntroMode();
        } else {
            PrintError(res, "NuiWaveSetEnabled");
        }
        mPaused = false;
    }
}

void WaveToTurnOnLight::DisableWaveState() {
    if (mWaveStateEnabled) {
        HRESULT res = NuiWaveSetEnabled(false);
        if (res == ERROR_SUCCESS) {
            mWaveStateEnabled = false;
            MILO_LOG("Wave detection disabled\n");
            mTimer.Reset();
            static Message cWaveGestureDisabledMsg("wave_gesture_disabled");
            Export(cWaveGestureDisabledMsg, true);
            if (TheHamDirector && TheHamDirector->GetWorld()) {
                TheHamDirector->TriggerNextIntro();
            }
        } else {
            PrintError(res, "NuiWaveSetEnabled");
        }
    }
}

void WaveToTurnOnLight::Poll() {
    bool success = false;
    if (mWaveStateEnabled) {
        HRESULT res = NuiWaveGetGestureOwnerProgress(&mSkeletonID, &mWaveProgress);
        if (SUCCEEDED(res)) {
            if (mWaveProgress >= 1) {
                MILO_LOG("You waved your hands!!!\n");
                success = true;
            }
        } else {
            mSkeletonID = 0;
            PrintError(res, "NuiWaveGetGestureOwnerProgress");
        }
    }
    if (TheHamDirector && !TheHamDirector->GetWorld()) {
        success = true;
    }
    if (mWaveStateEnabled && !success && mTimer.SplitMs() > 40000) {
        MILO_LOG("Wave gesture timed out!!!\n");
        success = true;
    }
    if (success && !mPaused) {
        DisableWaveState();
        static Message waveCompleteMsg("wave_gesture_complete", 0);
        TheHamProvider->Handle(waveCompleteMsg, false);
    }
}
