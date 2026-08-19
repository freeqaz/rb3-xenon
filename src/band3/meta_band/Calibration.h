#pragma once
#include "meta_band/InputMgr.h"
#include "obj/Data.h"
#include "os/UsbMidiKeyboardMsgs.h"
#include "rndobj/Mesh.h"
#include "synth/Faders.h"
#include "synth/Stream.h"
#include "ui/UIListProvider.h"
#include "ui/UIPanel.h"

enum TestState {
    tsIdle = 0,
    tsPreRoll = 1,
    tsTesting = 2,
    tsPostTest = 3
};

class CalibrationPanel : public UIPanel {
public:
    CalibrationPanel();
    OBJ_CLASSNAME(CalibrationPanel);
    OBJ_SET_TYPE(CalibrationPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~CalibrationPanel();
    virtual void Draw();
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);

    float GetAudioTimeMs() const;
    void SetTestState(TestState);
    void StopAudio();
    void UpdateAnimation();
    void UpdateLabel();
    void UpdateStream();
    int GetTestRep() const;
    void ScanHardwareModeInputs();
    // Retail inlines this everywhere (no standalone .text address in the retail
    // map), and UpdateAnimation shares ONE `1.0f / mCycleTimeMs` reciprocal
    // between the reshape and the frame scaling -- retail keeps it live in f11
    // across the reshape's if/else diamond and reuses it with `fmuls`. MSVC will
    // not CSE two separately-written `1.0f / mCycleTimeMs` across that diamond,
    // so the reciprocal is passed in explicitly.
    float ReshapeTime(float, float);
    float HandlePreAndPostTestAnim(float);
    void UpdateProgress(bool);
    void InitializeVisuals();
    void StartAudio();
    void PrepareHwCalibrationState();
    void TerminateHwCalibrationState();
    void EndTest();
    void TriggerCalibration(int);
    int GetAverageTestTime();
    float GetSampleSpread() const;
    int GetTestQuality() const;

    DataNode OnInitializeContent(DataArray *);
    DataNode OnStartTest(DataArray *);
    DataNode OnMsg(const ButtonDownMsg &);
    DataNode OnMsg(const KeyboardKeyPressedMsg &);

    static float kAnimPerceptualOffset;
    NEW_OBJ(CalibrationPanel);
    static void Init() { REGISTER_OBJ_FACTORY(CalibrationPanel); }

    float mCycleTimeMs; // 0x3c
    Stream *mStream; // 0x40
    Fader *mFader; // 0x44
    bool unk44;
    std::vector<float> mTestSamples; // 0x4c
    bool mHalfOffAnim; // 0x58
    bool mEnableVideo; // 0x59
    int mNumHits; // 0x5c
    bool mEnableAudio; // 0x60
    float unk5c;
    TestState mTestState; // 0x68
    float unk64;
    bool mHardwareMode; // 0x70
    float mAnimCycleFrames; // 0x74
    int mAnimNumCycles; // 0x78
    int mMaxSlack; // 0x7c
    float mRestingFrame; // 0x80
    int unk7c;
    float unk80;
    float mVolDb; // 0x8c
    int unk88;
    float unk8c;
    int unk90;
    float unk94;
    int unk98;
    int mPad; // 0xa4
    bool unka0; // maybe char instead?
    float unka4[5]; // 0xa4
    float unkb8[5]; // 0xb8
    bool unkcc;
    float unkd0;
    float unkd4;
    float unkd8;
    int unkdc;
    bool unke0;
    float unke4;
    int mTopOutliers; // 0xf0
    int mBottomOutliers; // 0xf4
};

class CalibrationModesProvider : public UIListProvider, public Hmx::Object {
public:
    CalibrationModesProvider();
    virtual ~CalibrationModesProvider() {}
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual int DataIndex(Symbol s) const;
    virtual int NumData() const;
    virtual void InitData(RndDir *);
    virtual DataNode Handle(DataArray *, bool);

    void Cleanup();
    Symbol GetCalibrationMode(int);

    std::vector<Symbol> mModes; // 0x2c
    RndMat *mAutoCalibrateMat; // 0x38
    RndMat *mAutoCalibrateDisabledMat; // 0x3c
    RndMat *mManualCalibrateMat; // 0x40
    RndMat *mEnterNumbersMat; // 0x44
};

class CalibrationWelcomePanel : public UIPanel {
public:
    CalibrationWelcomePanel() {}
    OBJ_CLASSNAME(CalibrationWelcomePanel);
    OBJ_SET_TYPE(CalibrationWelcomePanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~CalibrationWelcomePanel() {}
    virtual void Enter();
    virtual void Exit();

    DataNode OnMsg(const InputStatusChangedMsg &);

    static bool HaveCalbertConnected();
    NEW_OBJ(CalibrationWelcomePanel);
    static void Init() { REGISTER_OBJ_FACTORY(CalibrationWelcomePanel); }

    CalibrationModesProvider mModesProvider; // 0x38
};