#include "game/BandUser.h"
#include "game/Defines.h"
#include "math/Utl.h"
#include "meta_band/Calibration.h"
#include "meta_band/InputMgr.h"
#include "meta_band/ProfileMgr.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "os/PlatformMgr.h"
#include "os/UsbMidiKeyboardMsgs.h"
#include "os/User.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include "rndobj/Rnd.h"
#include "rndobj/TransAnim.h"
#include "synth/Synth.h"
#include "ui/UILabel.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIPanel.h"
#include "utl/Messages.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include <cmath>

float CalibrationPanel::kAnimPerceptualOffset = 0;

namespace {
    ControllerType GetControllerType() {
        BandUser *user = TheInputMgr->GetUser();
        ControllerType ct = kControllerNone;
        if (user) {
            ct = user->GetLocalBandUser()->ConnectedControllerType();
        }
        return ct;
    }
}

CalibrationPanel::CalibrationPanel()
    : mCycleTimeMs(755.0f), mStream(0), mFader(0), unk44(0), mHalfOffAnim(0),
      mEnableVideo(1), mNumHits(32), unk5c(0), mTestState(tsIdle), unk64(0),
      mHardwareMode(0), mAnimCycleFrames(50.0f), mAnimNumCycles(1), mMaxSlack(100),
      mRestingFrame(0), unk80(0), mVolDb(-6.0f), unk8c(0.275f), unka0(0), unkcc(0),
      unkd0(0), unkd4(0), unkd8(0), unkdc(0), unke4(0), mTopOutliers(3),
      mBottomOutliers(3) {}

CalibrationPanel::~CalibrationPanel() {
    RELEASE(mStream);
    RELEASE(mFader);
}

void CalibrationPanel::Poll() {
    if (mTestState == tsPostTest) {
        float ms = GetAudioTimeMs();
        if (ms - unk80 > mCycleTimeMs * 1.1f) {
            SetTestState(tsIdle);
            StopAudio();
        }
    }
    UpdateAnimation();
    UpdateLabel();
    UpdateStream();
    if (GetTestRep() >= unk90 && mTestState == tsTesting) {
        SetTestState(tsIdle);
    }
    if (mHardwareMode) {
        ScanHardwareModeInputs();
    }
    BandUser *user = TheInputMgr->GetUser();
    if (user && user->GetLocalBandUser()) {
        if (!user->GetLocalBandUser()->IsJoypadConnected()) {
            SetTestState(tsIdle);
        }
    }
    if (ThePlatformMgr.GuideShowing()) {
        SetTestState(tsIdle);
    }
    UIPanel::Poll();
}

void CalibrationPanel::UpdateAnimation() {
    if (mTestState != tsIdle) {
        if (mHardwareMode) {
            if (mTestState == tsPreRoll) {
                if (mEnableAudio) {
                    mFader->DoFade(0, mCycleTimeMs / 2.0f);
                }
                SetTestState(tsTesting);
                unk7c = 1;
                unk7c = GetTestRep();
            }
            float f1 = mCycleTimeMs;
            float f10 = GetAudioTimeMs();
            f10 = std::fmod(mCycleTimeMs / 2.0f + f10, f1);
            RndGroup *hwgrp = mDir->Find<RndGroup>("cal_hardware.grp", true);
            if (f10 > mCycleTimeMs / 2.0) {
                hwgrp->SetShowing(unk88 > 0);
                if (unk88 < 0)
                    unk88 = 0;
                unk88--;
            } else {
                unk88 = 9;
                hwgrp->SetShowing(false);
            }
        } else {
            float f1 = mCycleTimeMs;
            float f10 = GetAudioTimeMs();
            f10 = std::fmod(mCycleTimeMs / 2.0f + f10, f1);
            float recip = 1.0f / mCycleTimeMs;
            f10 = ReshapeTime(f10, recip);
            f10 = f10 * mAnimCycleFrames * recip;
            MILO_ASSERT(mAnimNumCycles == 1 || mAnimNumCycles == 2, 0xBE);
            // Retail branches AWAY on a non-zero remainder (`subf.` then `bne`),
            // i.e. the extra cycle is added on EVEN reps, not odd ones.
            if (mAnimNumCycles == 2 && GetTestRep() % 2 == 0) {
                f10 = f10 + mAnimCycleFrames;
            }
            if (mHalfOffAnim) {
                f10 = mAnimCycleFrames / 2.0f + f10;
            }
            f10 = std::fmod(f10, mAnimCycleFrames * (float)mAnimNumCycles);
            float frame = HandlePreAndPostTestAnim(f10);
            mDir->Find<RndGroup>("cal_visuals.grp", true)->SetFrame(frame, 1.0f);
        }
    } else if (mHardwareMode) {
        mDir->Find<RndGroup>("cal_hardware.grp", true)->SetShowing(false);
    }
}

float CalibrationPanel::HandlePreAndPostTestAnim(float f) {
    if (mTestState == tsPreRoll) {
        if (mRestingFrame == 0.0f) {
            if (unk64 > f) {
                if (mEnableAudio) {
                    mFader->DoFade(0, mCycleTimeMs / 2.0f);
                }
                SetTestState(tsTesting);
                unk7c = 1;
                unk7c = GetTestRep();
                goto finalize;
            }
        } else {
            if (f >= mRestingFrame && unk64 <= mRestingFrame) {
                if (mEnableAudio) {
                    mFader->DoFade(0, mCycleTimeMs / 2.0f);
                }
                SetTestState(tsTesting);
                unk7c = 1;
                unk7c = GetTestRep();
                goto finalize;
            }
        }
        unk64 = f;
        return mRestingFrame;
    } else if (mTestState == tsPostTest) {
        for (int i = 0; i < mAnimNumCycles; i++) {
            float testFrame = mAnimCycleFrames * (float)i + mRestingFrame;
            if (testFrame == 0.0f) {
                if (unk64 > f) {
                    f = testFrame;
                    break;
                }
            } else {
                if (f >= testFrame && unk64 <= testFrame) {
                    f = testFrame;
                    break;
                }
            }
            if (unk64 == testFrame) {
                f = testFrame;
                break;
            }
        }
    }
finalize:
    unk64 = f;
    return f;
}

void CalibrationPanel::UpdateLabel() {
    UILabel *countdownlabel = mDir->Find<UILabel>("cal_countdown.lbl", true);
    UILabel *repslabel = mDir->Find<UILabel>("cal_reps.lbl", true);
    RndGroup *progbargrp = mDir->Find<RndGroup>("prog_bar.grp", true);
    RndGroup *calvidlabelsgrp = mDir->Find<RndGroup>("cal_video_labels.grp", true);
    RndGroup *calhwvidgrp =
        mDir->Find<RndGroup>("cal_hardware_video_illustration.grp", true);
    RndGroup *calhwaudgrp =
        mDir->Find<RndGroup>("cal_hardware_audio_illustration.grp", true);
    RndGroup *calhwdarkgrp = mDir->Find<RndGroup>("cal_hardware_darkness.grp", true);
    if (mTestState == tsTesting) {
        if (mHardwareMode) {
            progbargrp->SetShowing(true);
            calhwaudgrp->SetShowing(false);
            calhwvidgrp->SetShowing(false);
            countdownlabel->SetTextToken(gNullStr);
            repslabel->SetTextToken(gNullStr);
            UpdateProgress(false);
            calvidlabelsgrp->SetShowing(false);
            calhwdarkgrp->SetShowing(true);
        } else {
            calhwdarkgrp->SetShowing(false);
            calvidlabelsgrp->SetShowing(true);
            float f14 = std::fmod(GetAudioTimeMs(), mCycleTimeMs);
            int i48 = GetAudioTimeMs() / mCycleTimeMs;
            if (i48 - unk7c >= 5) {
                progbargrp->SetShowing(true);
                repslabel->SetInt(mNumHits - mTestSamples.size(), false);
                repslabel->SetShowing(false);
                countdownlabel->SetTextToken(gNullStr);
                UpdateProgress(true);
            } else {
                progbargrp->SetShowing(false);
                if (unk5c > f14) {
                    if (4 - GetTestRep() == 0) {
                        static Symbol lag_go("lag_go");
                        countdownlabel->SetTextToken(lag_go);
                    } else {
                        countdownlabel->SetInt(4 - GetTestRep(), false);
                    }
                    repslabel->SetTextToken(gNullStr);
                }
                unk5c = f14;
            }
        }
    } else {
        calhwdarkgrp->SetShowing(false);
        calvidlabelsgrp->SetShowing(true);
        countdownlabel->SetTextToken(gNullStr);
        repslabel->SetTextToken(gNullStr);
        progbargrp->SetShowing(false);
        if (mHardwareMode) {
            if (mEnableVideo)
                calhwvidgrp->SetShowing(true);
            else
                calhwaudgrp->SetShowing(true);
        }
    }
}

void CalibrationPanel::UpdateProgress(bool b) {
    RndTransAnim *tabanim = mDir->Find<RndTransAnim>("prog_bar_tab.tnm", true);
    RndTransAnim *boneanim = mDir->Find<RndTransAnim>("bone_prog_bar.tnm", true);
    float progress = (float)mTestSamples.size();
    float maxProgress;
    if (b) {
        float a8 = unka4[1];
        float ac = unka4[2];
        float b0 = unka4[3];
        float b4 = unka4[4];
        float bc = unkb8[1];
        float c0 = unkb8[2];
        float c4 = unkb8[3];
        float c8 = unkb8[4];
        float sample = (float)((double)progress / 457453.4129);
        unka4[0] = a8;
        unka4[1] = ac;
        unka4[2] = b0;
        unka4[3] = b4;
        unka4[4] = sample;
        unkb8[0] = bc;
        unkb8[1] = c0;
        unkb8[2] = c4;
        unkb8[3] = c8;
        progress = (float)((6.0f * b0 + (4.0f * (ac + b4) + (a8 + sample)))
            + -0.7805914145 * bc + 3.3180408913 * c0
            + -5.2929307473 * c4 + 3.7554462943 * c8);
        unkb8[4] = progress;
    }
    int &_ref0 = mNumHits;
    progress = progress * (float)((_ref0 + 2) / _ref0);
    maxProgress = (float)_ref0;
    progress = *(maxProgress < progress ? &maxProgress : &progress);
    tabanim->SetFrame(24.0f * (progress / (float)_ref0), 1.0f);
    boneanim->SetFrame(24.0f * (progress / (float)_ref0), 1.0f);
}

void CalibrationPanel::Draw() { UIPanel::Draw(); }

void CalibrationPanel::UpdateStream() {
    if (mStream) {
        if (mStream->IsPlaying() && mTestState == tsIdle) {
            mStream->Stop();
        } else if (unk44 && mStream->IsReady()) {
            mStream->Play();
            unk44 = false;
        }
    }
}

DataNode CalibrationPanel::OnInitializeContent(DataArray *arr) {
    memset(unka4, 0, 0x14);
    memset(unkb8, 0, 0x14);
    RndTransAnim *tabanim = mDir->Find<RndTransAnim>("prog_bar_tab.tnm", true);
    RndTransAnim *boneanim = mDir->Find<RndTransAnim>("bone_prog_bar.tnm", true);
    tabanim->SetFrame(0, 1);
    boneanim->SetFrame(0, 1);
    static Symbol cal_video_desc_guitar("cal_video_desc_guitar");
    static Symbol cal_video_desc_drum("cal_video_desc_drum");
    static Symbol cal_video_desc_pad("cal_video_desc_pad");
    static Symbol cal_video_desc_calbert("cal_video_desc_calbert");
    static Symbol cal_audio_desc_guitar("cal_audio_desc_guitar");
    static Symbol cal_audio_desc_drum("cal_audio_desc_drum");
    static Symbol cal_audio_desc_pad("cal_audio_desc_pad");
    static Symbol cal_audio_desc_calbert("cal_audio_desc_calbert");
    static Symbol cal_hw_audio_title("cal_hw_audio_title");
    static Symbol cal_hw_video_title("cal_hw_video_title");
    static Symbol cal_audio_title("cal_audio_title");
    static Symbol cal_video_title("cal_video_title");
    ControllerType ty = GetControllerType();
    switch (ty) {
    case kControllerGuitar:
        mDir->Find<UILabel>("green1.lbl", true)->SetIcon('G');
        mDir->Find<UILabel>("green02.lbl", true)->SetIcon('G');
        mDir->Find<UILabel>("cal_video_instructions.lbl", true)
            ->SetTextToken(cal_video_desc_guitar);
        mDir->Find<UILabel>("cal_audio_instructions.lbl", true)
            ->SetTextToken(cal_audio_desc_guitar);
        break;
    case kControllerDrum:
        mDir->Find<UILabel>("green1.lbl", true)->SetIcon('1');
        mDir->Find<UILabel>("green02.lbl", true)->SetIcon('1');
        mDir->Find<UILabel>("cal_video_instructions.lbl", true)
            ->SetTextToken(cal_video_desc_drum);
        mDir->Find<UILabel>("cal_audio_instructions.lbl", true)
            ->SetTextToken(cal_audio_desc_drum);
        break;
    default:
        mDir->Find<UILabel>("green1.lbl", true)->SetIcon('A');
        mDir->Find<UILabel>("green02.lbl", true)->SetIcon('A');
        mDir->Find<UILabel>("cal_video_instructions.lbl", true)
            ->SetTextToken(cal_video_desc_pad);
        mDir->Find<UILabel>("cal_audio_instructions.lbl", true)
            ->SetTextToken(cal_audio_desc_pad);
        break;
    }
    if (mHardwareMode) {
        mDir->Find<UILabel>("audio_title.lbl", true)->SetTextToken(cal_hw_audio_title);
        mDir->Find<UILabel>("video_title.lbl", true)->SetTextToken(cal_hw_video_title);
        mDir->Find<UILabel>("cal_video_instructions.lbl", true)
            ->SetTextToken(cal_video_desc_calbert);
        mDir->Find<UILabel>("cal_audio_instructions.lbl", true)
            ->SetTextToken(cal_audio_desc_calbert);
        mDir->Find<RndGroup>("cal_hardware_video_illustration.grp", true)
            ->SetShowing(mEnableVideo);
        mDir->Find<RndGroup>("cal_hardware_audio_illustration.grp", true)
            ->SetShowing(!mEnableVideo);
    } else {
        mDir->Find<UILabel>("audio_title.lbl", true)->SetTextToken(cal_audio_title);
        mDir->Find<UILabel>("video_title.lbl", true)->SetTextToken(cal_video_title);
        mDir->Find<RndGroup>("cal_hardware_video_illustration.grp", true)
            ->SetShowing(false);
        mDir->Find<RndGroup>("cal_hardware_audio_illustration.grp", true)
            ->SetShowing(false);
    }
    unka0 = ty == 1 ? 0 : -1;
    SetTestState(tsIdle);
    if (mStream)
        RELEASE(mStream);
    if (mFader)
        RELEASE(mFader);
    const char *sound = "sfx/streams/sync_clap";
    if (mHardwareMode)
        sound = "sfx/streams/sync_beep";
    mFader = Hmx::Object::New<Fader>();
    mStream = TheSynth->NewStream(sound, mCycleTimeMs * 0.5f, 2.0f, true);
    MILO_ASSERT(mStream, 0x205);
    if (mStream) {
        mStream->SetJump(mCycleTimeMs, 1.0f, NULL);
        mStream->SetVolume(mVolDb);
        mFader->SetVal(-96.0f);
        mStream->Faders()->Add(mFader);
    }
    InitializeVisuals();
    return 0;
}

void CalibrationPanel::StartAudio() {
    mStream->SetVolume(mVolDb);
    if (mStream->GetFilePos() > 0) {
        mStream->Resync(0);
    }
    if (mStream->IsReady()) {
        mStream->Play();
    } else
        unk44 = true;
}

void CalibrationPanel::StopAudio() {
    unk44 = false;
    if (mStream) {
        mFader->DoFade(-96.0f, 200.0f);
    }
}

DataNode CalibrationPanel::OnStartTest(DataArray *arr) {
    static DataNode &cal_num_hits = DataVariable("cal_num_hits");
    if (cal_num_hits.Int() != 0)
        mNumHits = cal_num_hits.Int();
    static DataNode &cal_num_outliers = DataVariable("cal_num_outliers");
    if (cal_num_outliers.Int() != 0) {
        int outliers = cal_num_outliers.Int();
        mBottomOutliers = outliers;
        mTopOutliers = outliers;
    }
    unkd8 = 0;
    unkdc = 0;
    unkd4 = 0;
    unke4 = 0;
    mPad = arr->Obj<User>(2)->GetLocalUser()->GetPadNum();
    PrepareHwCalibrationState();
    StartAudio();
    unkd0 = -1.0f;
    mTestSamples.clear();
    SetTestState(tsPreRoll);
    int u4 = 10;
    if (mHardwareMode)
        u4 = 40;
    unk90 = u4;
    unk64 = 0;
    unk5c = mCycleTimeMs / 2.0f;
    if (mEnableVideo) {
        mDir->Find<RndGroup>("visuals_anim.grp", true)->Animate(0, false, 0);
    }
    if (mEnableAudio) {
        mDir->Find<RndGroup>("audio_anim.grp", true)->Animate(0, false, 0);
        unke0 = false;
    }
    return 0;
}

void CalibrationPanel::PrepareHwCalibrationState() {
    if (mHardwareMode) {
        unkcc = true;
        if (mEnableVideo) {
            JoypadSetCalbertMode(mPad, 1);
        } else {
            JoypadSetCalbertMode(mPad, 2);
        }
    }
}

void CalibrationPanel::TerminateHwCalibrationState() {
    if (unkcc) {
        JoypadSetCalbertMode(mPad, 0);
        unkcc = false;
    }
}

void CalibrationPanel::InitializeVisuals() {
    RndGroup *visuals = mDir->Find<RndGroup>("cal_visuals.grp", true);
    visuals->SetShowing(mEnableVideo);
    RndGroup *audio = mDir->Find<RndGroup>("cal_audio.grp", true);
    audio->SetShowing(mEnableAudio);
    mDir->Find<RndGroup>("prog_bar.grp", true)->SetShowing(false);
    if (mHardwareMode) {
        mDir->Find<RndGroup>("cal_metronome.grp", true)->SetShowing(false);
        mDir->Find<RndGroup>("visuals_anim.grp", true)->SetShowing(false);
        mDir->Find<RndGroup>("audio_anim.grp", true)->SetShowing(false);
        mDir->Find<RndGroup>("cal_hardware.grp", true)->SetShowing(false);
        unk88 = 0;
    } else {
        mDir->Find<RndGroup>("cal_metronome.grp", true)->SetShowing(true);
        mDir->Find<RndGroup>("cal_hardware.grp", true)->SetShowing(false);
        mDir->Find<RndGroup>("visuals_anim.grp", true)->SetShowing(false);
        mDir->Find<RndGroup>("audio_anim.grp", true)->SetShowing(false);
        if (mEnableVideo) {
            mDir->Find<RndGroup>("visuals_anim.grp", true)->SetShowing(true);
            mDir->Find<RndGroup>("visuals_anim.grp", true)->StopAnimation();
            mDir->Find<RndGroup>("visuals_anim.grp", true)->SetFrame(0, 1);
            mDir->Find<RndMesh>("strum.mesh", true)->SetShowing(false);
            mDir->Find<RndMesh>("drum_hit.mesh", true)->SetShowing(false);
            mDir->Find<RndMesh>("button_press_ps3.mesh", true)->SetShowing(false);
            mDir->Find<RndMesh>("button_press_xbox.mesh", true)->SetShowing(false);
            switch (GetControllerType()) {
            case kControllerGuitar:
                mDir->Find<RndMesh>("strum.mesh", true)->SetShowing(true);
                break;
            case kControllerDrum:
                mDir->Find<RndMesh>("drum_hit.mesh", true)->SetShowing(true);
                break;
            default:
                mDir->Find<RndMesh>("button_press_xbox.mesh", true)->SetShowing(true);
                break;
            }
        }
        if (mEnableAudio) {
            mDir->Find<RndGroup>("audio_anim.grp", true)->SetShowing(true);
            mDir->Find<RndGroup>("audio_anim.grp", true)->StopAnimation();
            mDir->Find<RndGroup>("audio_anim.grp", true)->SetFrame(0, 1);
        }
        const char *str = "cal_visuals.grp";
        mDir->Find<RndGroup>(str, true)->SetFrame(mRestingFrame, 1);
    }
}

void CalibrationPanel::EndTest() {
    MILO_LOG("-----------------------------\n");
    MILO_LOG("Pre Sort Calibration samples:\n");
    for (int i = 0; i < mTestSamples.size(); i++) {
        MILO_LOG("%f ms\n", mTestSamples[i]);
    }
    std::sort(mTestSamples.begin(), mTestSamples.end());
    MILO_LOG("------------------------------------------\n");
    MILO_LOG("Sorted Calibration samples, not truncated:\n");
    for (int i = 0; i < mTestSamples.size(); i++) {
        MILO_LOG("%f ms\n", mTestSamples[i]);
    }
    SetTestState(tsPostTest);
    mFader->DoFade(-96.0f, 1000.0f);
    unk80 = GetAudioTimeMs();
}

float CalibrationPanel::GetAudioTimeMs() const {
    if (!mStream)
        return 0;
    else {
        float f3 = 0;
        if (mEnableVideo && mEnableAudio) {
            f3 = TheProfileMgr.GetSongToTaskMgrMsRaw();
        }
        return f3 + mStream->GetTime();
    }
}

DataNode CalibrationPanel::OnMsg(const ButtonDownMsg &msg) {
    if (mTestState != tsIdle) {
        if (msg.GetAction() == kAction_Cancel) {
            SetTestState(tsIdle);
            mTestSamples.clear();
            StopAudio();
            return 0;
        } else if (mHardwareMode) {
            return 0;
        } else {
            if (mTestState == tsTesting && GetTestRep() >= 5) {
                bool b3 = true;
                bool b4 = true;
                if (msg.GetButton() != kPad_DUp && msg.GetButton() != kPad_DDown) {
                    b4 = false;
                }
                if (!b4) {
                    if (msg.GetAction() != kAction_Confirm) {
                        b3 = false;
                    }
                }
                if (msg.GetButton() == kPad_Xbox_A)
                    b3 = unka0;
                if (b3) {
                    auto _tmp0 = msg.GetPadNum();
                    TriggerCalibration(_tmp0);
                }
            }
            return 0;
        }
    } else
        return DataNode(kDataUnhandled, 0);
}

DataNode CalibrationPanel::OnMsg(const KeyboardKeyPressedMsg &msg) {
    if (mTestState == tsTesting && GetTestRep() >= 5) {
        TriggerCalibration(msg.GetPadNum());
        return 0;
    } else
        return DataNode(kDataUnhandled, 0);
}

void CalibrationPanel::ScanHardwareModeInputs() {
    static DataNode &trace_sensors = DataVariable("trace_sensors");
    if (mTestState == tsTesting) {
        float f4 = JoypadGetCalbertValue(mPad, mEnableVideo);
        float f6 = f4 - unkd4;
        float f5 = unke4;
        unkd4 = f6 * 0.05 + unkd4;
        if (f6 < f5)
            unke4 = f6 * 0.1 + f5;
        else
            unke4 = f5 * 0.9850000143051147f;
        float d = f4 - unk94;
        if (std::fabs(d) > 0.015625f) {
            trace_sensors.Int();
        }
        if (mEnableVideo) {
            if (f6 < unke4 * 0.35 && unk94 >= 0.0) {
                TriggerCalibration(mPad);
                trace_sensors.Int();
            }
            unk94 = f6;
        } else if (mEnableAudio) {
            if (0.71 > f4 && f4 > 0.49 && (unk94 <= 0.49 || 0.71 <= unk94)) {
                unkdc++;
                unkd8 += f4;
                TriggerCalibration(mPad);
                trace_sensors.Int();
            }
            unk94 = f4;
        }
    }
}

void CalibrationPanel::TriggerCalibration(int pad) {
    float sample = 0;
    float &_ref0 = mCycleTimeMs;
    float halfCycle = _ref0 / 2.0f;
    float cycleOff = std::fmod(halfCycle + GetAudioTimeMs(), _ref0);
    sample = cycleOff - halfCycle;
    float nowMs = GetAudioTimeMs();
    float lastMs = unkd0;
    if (nowMs > lastMs && (nowMs - lastMs) < 210.0f && lastMs != -1.0f)
        return;
    unkd0 = nowMs;
    float lag = TheProfileMgr.GetExcessAudioLagNeutral(pad, true);
    if (mEnableVideo)
        lag = TheProfileMgr.GetExcessVideoLagNeutral(pad, true);
    if (mHardwareMode) {
        lag = TheProfileMgr.GetPlatformAudioLatency();
        if (mEnableVideo)
            lag = TheProfileMgr.GetPlatformVideoLatency();
        sample -= 16.666666f;
        if (!mEnableVideo) {
            sample -= 20.0f;
        } else {
            sample -= 24.0f;
        }
    } else {
        if (mEnableVideo) {
            sample += kAnimPerceptualOffset;
        } else {
            sample += -6.0f;
        }
    }
    switch (JoypadGetPadData(pad)->mType) {
    case kJoypadWiiButtonGuitar:
        if (mEnableVideo)
            sample += 7.0f;
        else
            sample -= 14.0f;
        break;
    case kJoypadXboxButtonGuitar:
    case kJoypadPs3ButtonGuitar:
        if (mEnableVideo)
            sample -= 12.0f;
        else
            sample -= 34.0f;
        break;
    default:
        break;
    }
    int repAdvance = 10;
    sample -= lag;
    if (mHardwareMode)
        repAdvance = 40;
    unk90 = GetTestRep() + repAdvance;
    mTestSamples.push_back(sample);
    if (mTestSamples.size() >= mNumHits)
        EndTest();
}

int CalibrationPanel::GetAverageTestTime() {
    MILO_ASSERT(mTestSamples.size() >= (mNumHits - mBottomOutliers - mTopOutliers), 0x3BB);
    std::sort(mTestSamples.begin(), mTestSamples.end());
    MILO_LOG("--------------------\n");
    MILO_LOG("Calibration samples:\n");
    float f1 = 0;
    for (int i = mBottomOutliers; i < mTestSamples.size() - mTopOutliers; i++) {
        f1 += mTestSamples[i];
        MILO_LOG("%f ms\n", mTestSamples[i]);
    }
    float total = (float)(mTestSamples.size() - mTopOutliers - mBottomOutliers);
    float testresult = f1 / total;
    MILO_LOG("TestResult: %f ms\n", testresult);
    MILO_LOG("Sample Spread: %f ms\n", GetSampleSpread());
    float f2 = 0;
    if (mHardwareMode)
        f2 = 8.0f; // Xbox 360 retail constant (0x41000000); rb3-Wii dev decomp has 14.0f (0x41600000) - platform-specific HW latency offset
    return std::floor((testresult + 0.5) - f2);
}

int CalibrationPanel::GetTestRep() const {
    float cyclems = mCycleTimeMs;
    float audioms = GetAudioTimeMs();
    int result = (int)((cyclems / 2.0f + audioms) / cyclems);
    return result - unk7c;
}

float CalibrationPanel::GetSampleSpread() const {
    MILO_ASSERT(mTestSamples.size() >= (mNumHits - mTopOutliers - mBottomOutliers), 0x3F4);
    return mTestSamples[mTestSamples.size() - mTopOutliers - 1]
        - mTestSamples[mBottomOutliers];
}

int CalibrationPanel::GetTestQuality() const {
    if (mTestSamples.size() < mNumHits - mTopOutliers - mBottomOutliers)
        return 0;
    else {
        int ret = GetSampleSpread() < (float)mMaxSlack;
        if (ret == 0) {
            for (int i = 0; i < mTestSamples.size(); i++) {
                MILO_LOG("%f ms\n", mTestSamples[i]);
            }
        }
        return ret;
    }
}

float CalibrationPanel::ReshapeTime(float f, float recip) {
    float cycle = mCycleTimeMs;
    float f1 = f * recip;
    if (f1 <= 1.0f)
        f1 = f1 * f1;
    else
        f1 = (f1 - 2.0f) * (f1 - 2.0f) + 2.0f;
    f1 = f1 * cycle;
    return Interp(f, f1, unk8c);
}

void CalibrationPanel::Enter() {
    TheRnd.SetGSTiming(true);
    UIPanel::Enter();
}

void CalibrationPanel::Exit() {
    if (mTestState != tsIdle) {
        SetTestState(tsIdle);
        StopAudio();
    }
    RELEASE(mStream);
    RELEASE(mFader);
    TheRnd.SetGSTiming(false);
    TerminateHwCalibrationState();
    UIPanel::Exit();
}

#pragma push
#pragma pool_data off
void CalibrationPanel::SetTestState(TestState state) {
    mTestState = state;
    if (state != tsIdle) {
        static Message notIdleMsg("handle_non_idle_state");
        Handle(notIdleMsg, true);
    } else {
        static Message idleMsg("handle_idle_state");
        Handle(idleMsg, true);
    }
}
#pragma pop

BEGIN_HANDLERS(CalibrationPanel)
    HANDLE(initialize_content, OnInitializeContent)
    HANDLE_EXPR(is_processing_input, mTestState != tsIdle)
    HANDLE_EXPR(get_test_quality, (unsigned char)GetTestQuality())
    HANDLE_EXPR(get_test_result, GetAverageTestTime())
    HANDLE(start_test, OnStartTest)
    HANDLE_MESSAGE(ButtonDownMsg)
    HANDLE_MESSAGE(KeyboardKeyPressedMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x463)
END_HANDLERS

BEGIN_PROPSYNCS(CalibrationPanel)
    SYNC_PROP(cycle_time_ms, mCycleTimeMs)
    SYNC_PROP(hardware_mode, mHardwareMode)
    SYNC_PROP(audio_enable, mEnableAudio)
    SYNC_PROP(audio_vol_db, mVolDb)
    SYNC_PROP(anim_cycle_frames, mAnimCycleFrames)
    SYNC_PROP(anim_num_cycles, mAnimNumCycles)
    SYNC_PROP(video_enable, mEnableVideo)
    SYNC_PROP(anim_half_off, mHalfOffAnim)
    SYNC_PROP(anim_resting_frame, mRestingFrame)
    SYNC_PROP(num_hits, mNumHits)
    SYNC_PROP(bottom_outliers, mBottomOutliers)
    SYNC_PROP(top_outliers, mTopOutliers)
    SYNC_PROP(max_slack, mMaxSlack)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

CalibrationModesProvider::CalibrationModesProvider()
    : mAutoCalibrateMat(0), mAutoCalibrateDisabledMat(0), mManualCalibrateMat(0),
      mEnterNumbersMat(0) {
    SetName("calibration_modes_provider", ObjectDir::Main());
}

void CalibrationModesProvider::Cleanup() {
    mModes.clear();
    mAutoCalibrateMat = 0;
    mAutoCalibrateDisabledMat = 0;
    mManualCalibrateMat = 0;
    mEnterNumbersMat = 0;
}

Symbol CalibrationModesProvider::GetCalibrationMode(int selectedPos) {
    MILO_ASSERT(( 0) <= (selectedPos) && (selectedPos) < ( mModes.size()), 0x48A);
    return mModes[selectedPos];
}

void CalibrationModesProvider::InitData(RndDir *dir) {
    MILO_ASSERT(dir, 0x490);
    mAutoCalibrateMat = dir->Find<RndMat>("auto_calibrate.mat", true);
    mAutoCalibrateDisabledMat = dir->Find<RndMat>("auto_calibrate_disabled.mat", true);
    mManualCalibrateMat = dir->Find<RndMat>("manual_calibrate.mat", true);
    mEnterNumbersMat = dir->Find<RndMat>("enter_numbers.mat", true);
    mModes.clear();
    mModes.push_back(cal_auto);
    mModes.push_back(cal_manual);
    mModes.push_back(cal_numbers);
}

void CalibrationModesProvider::Text(int, int data, UIListLabel *slot, UILabel *label)
    const {
    MILO_ASSERT(( 0) <= (data) && (data) < ( mModes.size()), 0x4A3);
    if (slot->Matches("name")) {
        label->SetTextToken(mModes[data]);
    }
}

RndMat *CalibrationModesProvider::Mat(int, int data, UIListMesh *slot) const {
    static Symbol cal_auto("cal_auto");
    static Symbol cal_manual("cal_manual");
    static Symbol cal_numbers("cal_numbers");
    MILO_ASSERT(( 0) <= (data) && (data) < ( mModes.size()), 0x4B1);
    if (slot->Matches("icon")) {
        Symbol mode = mModes[data];
        if (mode == cal_auto) {
            if (CalibrationWelcomePanel::HaveCalbertConnected())
                return mAutoCalibrateMat;
            else
                return mAutoCalibrateDisabledMat;
        } else if (mode == cal_manual)
            return mManualCalibrateMat;
        else if (mode == cal_numbers)
            return mEnterNumbersMat;
    }
    return slot->DefaultMat();
}

int CalibrationModesProvider::NumData() const { return mModes.size(); }

int CalibrationModesProvider::DataIndex(Symbol s) const {
    for (int i = 0; i < mModes.size(); i++) {
        if (mModes[i] == s)
            return i;
    }
    return 0;
}

BEGIN_HANDLERS(CalibrationModesProvider)
    HANDLE_EXPR(get_calibration_mode, GetCalibrationMode(_msg->Int(2)))
    HANDLE_ACTION(cleanup, Cleanup())
    HANDLE_CHECK(0x4D9)
END_HANDLERS

void CalibrationWelcomePanel::Enter() {
    UIPanel::Enter();
    MILO_ASSERT(TheInputMgr, 0x4E0);
    static Symbol input_status_changed("input_status_changed");
    TheInputMgr->AddSink(this, input_status_changed);
    static InputStatusChangedMsg dummy;
    OnMsg(dummy);
}

void CalibrationWelcomePanel::Exit() {
    UIPanel::Exit();
    MILO_ASSERT(TheInputMgr, 0x4EB);
    TheInputMgr->RemoveSink(this);
}

DataNode CalibrationWelcomePanel::OnMsg(const InputStatusChangedMsg &msg) {
    Handle(controllers_changed_msg, true);
    return 0;
}

bool CalibrationWelcomePanel::HaveCalbertConnected() {
    for (int i = 0; i < 4; i++) {
        if (JoypadIsCalbertGuitar(i))
            return true;
    }
    return false;
}

BEGIN_HANDLERS(CalibrationWelcomePanel)
    HANDLE_MESSAGE(InputStatusChangedMsg)
    HANDLE_EXPR(have_calbert_connected, HaveCalbertConnected())
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x509)
END_HANDLERS
