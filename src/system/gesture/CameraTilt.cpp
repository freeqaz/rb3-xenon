#include "gesture/CameraTilt.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "ui/UI.h"
#include "xdk/NUI.h"
#include "xdk/XAPILIB.h"
#include "xdk/nui/nuidetroit.h"
#include "xdk/xapilibi/winerror.h"

CameraTilt *TheCameraTilt;

CameraTilt::CameraTilt()
    : mScanActive(0), mElapsedMs(0), mCycles(0), mConsecutiveErrors(0), mState(0), mPrevState(0), mDelayBetweenStates(0),
      mDelayBetweenRetry(0), mUpDownCyclesPerScan(1), mAngleWiggleRoom(3),
      mErrorRepeatedTimes(0), mCycleSafetyTimeout(4), mTiltMovingFlags(0) {
    memset(&mOverlapped, 0, sizeof(XOVERLAPPED));
    memset(&mTiltObjects, 0, sizeof(NUI_TILT_OBJECTS));
    DataArray *camArr = SystemConfig()->FindArray("camera_tilt", false);
    if (camArr) {
        mDelayBetweenStates =
            camArr->FindInt("delay_between_states", mDelayBetweenStates);
        mDelayBetweenRetry = camArr->FindInt("delay_between_retry", mDelayBetweenRetry);
        mUpDownCyclesPerScan =
            camArr->FindInt("up_down_cycles_per_scan", mUpDownCyclesPerScan);
        mAngleWiggleRoom = camArr->FindInt("angle_wiggle_room", mAngleWiggleRoom);
        mErrorRepeatedTimes =
            camArr->FindInt("error_repeated_times", mErrorRepeatedTimes);
        mCycleSafetyTimeout =
            camArr->FindFloat("cycle_safety_timeout", mCycleSafetyTimeout);
    }
}

BEGIN_HANDLERS(CameraTilt)
    HANDLE_ACTION(camera_scan, StartCameraScan())
    HANDLE_MESSAGE(UIChangedMsg)
END_HANDLERS

BEGIN_PROPSYNCS(CameraTilt)
    SYNC_PROP(angle, mAngle)
END_PROPSYNCS

void CameraTilt::UpdateTiltingToInital() {
    if (mOverlapped.InternalLow != ERROR_IO_PENDING) {
        MILO_LOG("NuiCameraAdjustTilt - completed tilt to inital\n");
        mState = 8;
        mElapsedMs = 0;
    }
}

void CameraTilt::UpdateGetInitialTiltData() {
    if (mOverlapped.InternalLow != ERROR_IO_PENDING) {
        MILO_LOG("NuiCameraAdjustTilt - got initial tilt data\n");
        mState = 4;
        mElapsedMs = 0;
    }
}

void CameraTilt::Init() {
    MILO_ASSERT(!TheCameraTilt, 0x36);
    TheCameraTilt = new CameraTilt();
    TheCameraTilt->SetName("camera_tilt", ObjectDir::Main());
}

void CameraTilt::StartCameraScan() {
    if (mState != 0) {
        MILO_LOG(
            "StartCameraScan: ERROR : Scan is trying to be initiated while in a scan sequence. Ignoring Scan Request!!!"
        );
    } else {
        mState = 1;
        mScanActive = true;
        mCycles = 0;
        mElapsedMs = 0;
        mTimer.Start();
        ThePlatformMgr.AddSink(TheCameraTilt);
    }
}

void CameraTilt::StartGetInitialTiltData() {
    DWORD ret =
        NuiCameraAdjustTilt(0x20, 0, 2.1336f, 2.1336f, &mTiltObjects, &mOverlapped);
    mElapsedMs = 0;
    if (ret == ERROR_SUCCESS) {
        mState = 3;
        MILO_LOG(
            "NuiCameraAdjustTilt completed immediately - camera tilt already optimal?\n"
        );
    } else if (ret == ERROR_IO_PENDING) {
        mState = 3;
        if (mPrevState == 1) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("NuiCameraAdjustTilt - Camera is getting initial camera data\n");
        }
        mPrevState = 1;
    } else if (ret == ERROR_RETRY) {
        mState = 2;
        if (mPrevState == 2) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("NuiCameraAdjustTilt called too soon after previous call\n");
        }
        mPrevState = 2;
    } else if (ret == ERROR_BUSY) {
        mState = 2;
        if (mPrevState == 3) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("NuiCameraAdjustTilt failed because camera was busy\n");
        }
        mPrevState = 3;
    } else if (ret == ERROR_TOO_MANY_CMDS) {
        mState = 2;
        if (mPrevState == 4) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("NuiCameraAdjustTilt failed to find player candidate; waiting\n");
        }
        mPrevState = 4;
    } else {
        mState = 0;
        if (mPrevState == 5) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("Unexpected result from NuiCameraAdjustTilt - %x\n", ret);
        }
        mPrevState = 5;
    }
}

void CameraTilt::StartCameraTiltingUp() {
    HRESULT res = NuiCameraElevationSetAngle(27);
    if (res == ERROR_SUCCESS) {
        MILO_LOG("NuiCameraElevationSetAngle - Camera is tilting to Up\n");
        mState = 15;
        mElapsedMs = 0;
    } else if (res == E_INVALIDARG) {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 6) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the input angle is outside the accepted range\n"
            );
        }
        mPrevState = 6;
    } else if (res == E_NUI_DEVICE_NOT_CONNECTED) {
        mState = 0;
        mElapsedMs = 0;
        if (mPrevState == 7) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the Kinect sensor array is not attached\n"
            );
        }
        mPrevState = 7;
    } else if (res == E_NUI_SYSTEM_UI_PRESENT) {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 8) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the Xbox Guide UI is active so elevation will not be changed\n"
            );
        }
        mPrevState = 8;
    } else {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 5) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("Unexpected result from NuiCameraElevationSetAngle - %x\n", res);
        }
        mPrevState = 5;
    }
}

void CameraTilt::StartCameraTiltingDown() {
    HRESULT res = NuiCameraElevationSetAngle(-27);
    if (res == ERROR_SUCCESS) {
        MILO_LOG("NuiCameraElevationSetAngle - Camera is tilting to Down\n");
        mState = 11;
        mElapsedMs = 0;
    } else if (res == E_INVALIDARG) {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 6) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the input angle is outside the accepted range\n"
            );
        }
        mPrevState = 6;
    } else if (res == E_NUI_DEVICE_NOT_CONNECTED) {
        mState = 0;
        mElapsedMs = 0;
        if (mPrevState == 7) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the Kinect sensor array is not attached\n"
            );
        }
        mPrevState = 7;
    } else if (res == E_NUI_SYSTEM_UI_PRESENT) {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 8) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the Xbox Guide UI is active so elevation will not be changed\n"
            );
        }
        mPrevState = 8;
    } else {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 5) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("Unexpected result from NuiCameraElevationSetAngle - %x\n", res);
        }
        mPrevState = 5;
    }
}

void CameraTilt::StartCameraTiltingToInital() {
    DWORD res = NuiCameraAdjustTilt(0, 0, 0, 2.1336f, &mTiltObjects, &mOverlapped);
    mElapsedMs = 0;
    if (res == ERROR_SUCCESS) {
        mState = 7;
        MILO_LOG(
            "NuiCameraAdjustTilt completed immediately - camera tilt already optimal?\n"
        );
    } else if (res == ERROR_IO_PENDING) {
        mState = 7;
        if (mPrevState == 1) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("NuiCameraAdjustTilt - Camera is getting initial camera data\n");
        }
        mPrevState = 1;
    } else if (res == ERROR_RETRY) {
        mState = 2;
        if (mPrevState == 2) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("NuiCameraAdjustTilt called too soon after previous call\n");
        }
        mPrevState = 2;
    } else if (res == ERROR_BUSY) {
        mState = 2;
        if (mPrevState == 3) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("NuiCameraAdjustTilt failed because camera was busy\n");
        }
        mPrevState = 3;
    } else if (res == ERROR_TOO_MANY_CMDS) {
        mState = 2;
        if (mPrevState == 4) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("NuiCameraAdjustTilt failed to find player candidate; waiting\n");
        }
        mPrevState = 4;
    } else {
        mState = 0;
        if (mPrevState == 5) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("Unexpected result from NuiCameraAdjustTilt - %x\n", res);
        }
        mPrevState = 5;
    }
}

void CameraTilt::UpdateTiltingUp() {
    LONG lAngleDegrees;
    HRESULT res = NuiCameraElevationGetAngle(&lAngleDegrees, &mTiltMovingFlags);
    if (res == ERROR_SUCCESS) {
        mAngle = (float)lAngleDegrees * 0.018518519f + 0.5f;
        if (lAngleDegrees > 27 - mAngleWiggleRoom) {
            MILO_LOG("NuiCameraElevationGetAngle : Up : Normal angle end\n");
            mState = 16;
            mElapsedMs = 0;
        } else if (mElapsedMs > mCycleSafetyTimeout) {
            MILO_LOG("NuiCameraElevationGetAngle : Up : Safety timer end\n");
            mState = 16;
            mElapsedMs = 0;
        }
    } else if (res == E_INVALIDARG) {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 6) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the input angle is outside the accepted range\n"
            );
        }
        mPrevState = 6;
    } else if (res == E_NUI_DEVICE_NOT_CONNECTED) {
        mState = 0;
        mElapsedMs = 0;
        if (mPrevState == 7) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the Kinect sensor array is not attached\n"
            );
        }
        mPrevState = 7;
    } else if (res == E_NUI_SYSTEM_UI_PRESENT) {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 8) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the Xbox Guide UI is active so elevation will not be changed\n"
            );
        }
        mPrevState = 8;
    } else {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 5) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("Unexpected result from NuiCameraElevationSetAngle - %x\n", res);
        }
        mPrevState = 5;
    }
}

void CameraTilt::UpdateTiltingDown() {
    LONG lAngleDegrees;
    HRESULT res = NuiCameraElevationGetAngle(&lAngleDegrees, &mTiltMovingFlags);
    if (res == ERROR_SUCCESS) {
        mAngle = (float)lAngleDegrees * 0.018518519f + 0.5f;
        if (lAngleDegrees < mAngleWiggleRoom - 27) {
            MILO_LOG("NuiCameraElevationGetAngle : Down : Normal angle end\n");
            mState = 12;
            mElapsedMs = 0;
        } else if (mElapsedMs > mCycleSafetyTimeout) {
            MILO_LOG("NuiCameraElevationGetAngle : down : Safety timer end\n");
            mState = 12;
            mElapsedMs = 0;
        }
    } else if (res == E_INVALIDARG) {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 6) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the input angle is outside the accepted range\n"
            );
        }
        mPrevState = 6;
    } else if (res == E_NUI_DEVICE_NOT_CONNECTED) {
        mState = 0;
        mElapsedMs = 0;
        if (mPrevState == 7) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the Kinect sensor array is not attached\n"
            );
        }
        mPrevState = 7;
    } else if (res == E_NUI_SYSTEM_UI_PRESENT) {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 8) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG(
                "NuiCameraElevationSetAngle failed because the Xbox Guide UI is active so elevation will not be changed\n"
            );
        }
        mPrevState = 8;
    } else {
        mState = 5;
        mElapsedMs = 0;
        if (mPrevState == 5) {
            mConsecutiveErrors++;
        } else {
            mConsecutiveErrors = 0;
        }
        if (mConsecutiveErrors <= mErrorRepeatedTimes) {
            MILO_LOG("Unexpected result from NuiCameraElevationSetAngle - %x\n", res);
        }
        mPrevState = 5;
    }
}

void CameraTilt::Poll() {
    if (!mScanActive)
        return;

    switch (mState) {
    case 0:
        mScanActive = false;
        mCycles = 0;
        mElapsedMs = 0;
        mTimer.Stop();
        ThePlatformMgr.RemoveSink(TheCameraTilt);
        break;
    case 1:
        StartGetInitialTiltData();
        break;
    case 2:
        if (mElapsedMs > mDelayBetweenRetry) {
            mState = 1;
            mElapsedMs = 0;
        }
        break;
    case 3:
        UpdateGetInitialTiltData();
        break;
    case 4:
        if (mElapsedMs > mDelayBetweenStates) {
            mState = 9;
            mElapsedMs = 0;
        }
        break;
    case 5:
        StartCameraTiltingToInital();
        break;
    case 6:
        if (mElapsedMs > mDelayBetweenRetry) {
            mState = 5;
            mElapsedMs = 0;
        }
        break;
    case 7:
        UpdateTiltingToInital();
        break;
    case 8:
        if (mElapsedMs > mDelayBetweenStates) {
            mState = 0;
            mElapsedMs = 0;
        }
        break;
    case 9:
        StartCameraTiltingDown();
        break;
    case 10:
        if (mElapsedMs > mDelayBetweenRetry) {
            mState = 9;
            mElapsedMs = 0;
        }
        break;
    case 11:
        UpdateTiltingDown();
        break;
    case 12:
        if (mElapsedMs > mDelayBetweenStates) {
            mState = 13;
            mElapsedMs = 0;
        }
        break;
    case 13:
        StartCameraTiltingUp();
        break;
    case 14:
        if (mElapsedMs > mDelayBetweenRetry) {
            mState = 13;
            mElapsedMs = 0;
        }
        break;
    case 15:
        UpdateTiltingUp();
        break;
    case 16:
        if (mElapsedMs > mDelayBetweenStates) {
            mCycles++;
            mElapsedMs = 0;
            if (mCycles < mUpDownCyclesPerScan) {
                mState = 9;
            } else {
                mState = 5;
            }
        }
        break;
    }
    mElapsedMs += Timer::CyclesToMs(mTimer.Stop());
    mTimer.Start();
}

DataNode CameraTilt::OnMsg(const UIChangedMsg &msg) {
    if (mState != 0) {
        if (!msg.Showing()) {
            mScanActive = true;
            mTimer.Start();
        } else {
            mScanActive = false;
            mTimer.Stop();
        }
    }
    return 0;
}
