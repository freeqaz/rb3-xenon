#include "game/GameMicManager.h"
#include "GameMic.h"
#include "GameMicManager.h"
#include "game/GameMode.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "os/Debug.h"
#include "synth/FxSend.h"
#include "synth/FxSendDelay.h"
#include "synth/FxSendSynapse.h"
#include "synth/MicClientMapper.h"
#include "synth/MicManagerInterface.h"
#include "synth/Synth.h"
#include "utl/Loader.h"
#include "utl/Symbols.h"
#include <cstddef>

GameMicManager *TheGameMicManager;
static MicClientID sNullMicClientID(-1, -1);

void GameMicManager::Init() {
    MILO_ASSERT(TheGameMicManager == NULL, 0x28);
    TheGameMicManager = new GameMicManager();
    TheDebug.AddExitCallback(GameMicManager::Terminate);
    MILO_ASSERT(TheSynth, 0x30);
    MicClientMapper *pMapper = TheSynth->GetMicClientMapper();
    TheGameMicManager->LoadMicFx();
    MILO_ASSERT(pMapper, 0x37);
    pMapper->SetMicManager(TheGameMicManager);
}

void GameMicManager::Terminate() {
    MILO_ASSERT(TheGameMicManager != NULL, 0x42);
    RELEASE(TheGameMicManager);
}

static DataNode SetProximity(DataArray *a) {
    TheGameMicManager->SetSynapseProximity(a->Float(1));
    return 0;
}

static DataNode SetSlackyness(DataArray *a) {
    TheGameMicManager->SetSynapseSlackyness(a->Float(1));
    return 0;
}

static DataNode SetFocus(DataArray *a) {
    TheGameMicManager->SetSynapseFocus(a->Float(1));
    return 0;
}

static DataNode SetAmount(DataArray *a) {
    TheGameMicManager->SetSynapseAmount(a->Float(1));
    return 0;
}

static DataNode SetEnable(DataArray *a) {
    TheGameMicManager->unk2f = a->Float(1) > 0.5;
    return 0;
}

GameMicManager::GameMicManager()
    : unk2c(0), unk2d(0), unk2e(0), unk2f(1), mSynapseProximity(-1), mSynapseFocus(-1),
      mMicCount(0), mPlayback(0) {
    DataRegisterFunc("synapse_set_proximity", SetProximity);
    DataRegisterFunc("synapse_set_amount", SetAmount);
    DataRegisterFunc("synapse_set_enable", SetEnable);
    DataRegisterFunc("synapse_set_focus", SetFocus);
    DataRegisterFunc("synapse_set_slackyness", SetSlackyness);
    mMics.resize(4);
}

GameMicManager::~GameMicManager() {}

void GameMicManager::HandleMicsChanged() {
    MicClientMapper *pMapper = TheSynth->GetMicClientMapper();
    MILO_ASSERT(pMapper, 0x92);
    std::vector<int> mics;
    pMapper->GetAllConnectedMics(mics);
    // Searched through a const view: retail calls __find<const int *>.
    const std::vector<int> &connected = mics;
    for (int i = 0; i < mMics.size(); i++) {
        if (std::find(connected.begin(), connected.end(), i) == connected.end()) {
            DeleteMic(i);
        } else
            CreateMic(i);
    }
    static GameMicsChangedMsg msg;
    MsgSource::Handle(msg, false);
}

bool GameMicManager::HasMic(const MicClientID &id) const {
    MILO_ASSERT(TheSynth, 0xB4);
    // Retail 0x82681488 has no frame_rate-mode fallback.
    int nMicID = TheSynth->GetMicClientMapper()->GetMicIDForClientID(id);
    return nMicID != -1;
}

GameMic *GameMicManager::GetMic(const MicClientID &id) {
    MILO_ASSERT(TheSynth, 0xCA);
    int nMicID = TheSynth->GetMicClientMapper()->GetMicIDForClientID(id);
    if (nMicID == -1) {
#ifdef HX_NATIVE
        if (TheGameMode && TheGameMode->InMode(frame_rate)) {
            if (mFakeMics.empty()) {
                InitFakeMics();
            }
            return mFakeMics[id.mClientID];
        } else
            return nullptr;
#else
        return nullptr;
#endif
    } else
        return mMics[nMicID];
}

float GameMicManager::GetEnergyForMic(const MicClientID &id) {
    GameMic *mic = GetMic(id);
    if (mic) {
        mic->Update();
        return mic->mLastEnergy;
    } else
        return 0;
}

void GameMicManager::SetPlayback(bool b1) {
    if (mPlayback != b1) {
        mPlayback = b1;
        for (int i = 0; i < mMics.size(); i++) {
            GameMic *mic = mMics[i];
            if (mic) {
                ApplyPlayback(b1, mic);
            }
        }
    }
}

void GameMicManager::ApplyPlayback(bool b1, GameMic *iMic) const {
    MILO_ASSERT(iMic, 0x119);
    Mic *pMic = iMic->GetMyMic();
    MILO_ASSERT(pMic, 0x11E);
    if (b1) {
        pMic->StartPlayback();
    } else
        pMic->StopPlayback();
}

void GameMicManager::DeleteMic(int idx) {
    GameMic *mic = mMics[idx];
    if (mic) {
        delete mic;
        mMics[idx] = nullptr;
        mMicCount--;
    }
}

void GameMicManager::CreateMic(int idx) {
    if (!mMics[idx]) {
        GameMic *mic = new GameMic(idx);
        mMics[idx] = mic;
        mMicCount++;
        HookUpFxForMicId(mic);
        ApplyPlayback(mPlayback, mic);
    }
}

void GameMicManager::LoadMicFx() {
    FilePath fp(".", "sfx/mic_fx.milo");
    unk20.LoadFile(fp, true, false, kLoadFront, false);
}

int GameMicManager::GetMicCount() const {
    // Retail 0x8235BA70 is the 8-byte `lwz r3,0x34(r3); blr`: no frame_rate-mode
    // fallback (that is the fake-mic feature, native-only like GetMic's).
#ifdef HX_NATIVE
    if (TheGameMode && TheGameMode->InMode(frame_rate)) {
        return 4;
    }
#endif
    return mMicCount;
}

void GameMicManager::HookUpFxForMicId(GameMic *gmic) {
    if (!unk2c) {
        unk20.PostLoad(nullptr);
        unk2c = true;
    }
    Mic *mic = TheSynth->GetMic(gmic->mMicID);
    if (mic) {
        FxSend *send = unk20->Find<FxSend>("synapse.send", false);
        mic->SetFxSend(send);
    }
    // TU5: the synapse is reset to its neutral target whether or not the mic exists.
    SetPitchCorrectionTarget(false, false, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f);
}

void GameMicManager::SetOverdriveEffectEnable(bool b1) {
    if (unk20) {
        FxSend *send = unk20->Find<FxSend>("delay.send", true);
        static Symbol wet_gain("wet_gain");
        send->SetProperty(wet_gain, b1 ? 0.0f : -96.0f);
    }
}

void GameMicManager::Poll(float f1) {
    if (unk20) {
        FxSendDelay *send = unk20->Find<FxSendDelay>("delay.send", true);
        if (send) {
            static Symbol tempo_sync("tempo_sync");
            static Symbol tempo("tempo");
            const DataNode *syncProp = send->Property(tempo_sync, false);
            if (syncProp && syncProp->Int()) {
                send->SetProperty(tempo, f1);
            }
        }
    }
}

// TU5 body, rebuilt from retail bytes (fn_826818D0); it is not an
// empty stub. Notes are MIDI pitches, converted with 8.1757989 Hz * 2^(n/12) (retail 0x4102D013, MIDI note 0).
// The second range test overwriting `prox` (not `focus`) is retail's own: both
// `fmr f24, f0`, and `focus` still reaches SetProximityFocus unmodified.
void GameMicManager::SetPitchCorrectionTarget(
    bool enable, bool unison, float prox, float focus, float n1, float n2, float n3
) {
    if (mSynapseProximity >= 0.0f && mSynapseProximity <= 1.0f)
        prox = mSynapseProximity;
    if (mSynapseFocus >= 0.0f && mSynapseFocus <= 1.0f)
        prox = mSynapseFocus;
    if (!unk20)
        return;
    if (!unk2f) {
        for (int i = 0; i < 3; i++) {
            if (unk20)
                unk20->Find<FxSendSynapse>("synapse.send", true)->SetAmount(0.0f);
        }
        return;
    }
    FxSendSynapse *send = unk20->Find<FxSendSynapse>("synapse.send", false);
    if (send) {
        if (enable) {
            float hz1 = (float)pow(2.0, n1 / 12.0f) * 8.1757989f;
            float hz2 = (float)pow(2.0, n2 / 12.0f) * 8.1757989f;
            if (n2 == 0.0f)
                hz2 = 0.0f;
            float hz3 = (float)pow(2.0, n3 / 12.0f) * 8.1757989f;
            if (n3 == 0.0f)
                hz3 = 0.0f;
            send->SetProximityEffect(prox);
            send->SetProximityFocus(focus);
            send->SetNoteHz(hz1, hz2, hz3);
            send->SetUnisonTrio(unison);
            send->SetAmount(1.0f);
        } else {
            send->SetAmount(0.0f);
        }
    }
}

void GameMicManager::SetSynapseProximity(float f1) {
    mSynapseProximity = f1;
    FxSendSynapse *send = unk20->Find<FxSendSynapse>("synapse.send", true);
    send->SetProximityEffect(f1);
}

void GameMicManager::SetSynapseSlackyness(float f1) {
    FxSendSynapse *send = unk20->Find<FxSendSynapse>("synapse.send", true);
    float mult = f1 * 40.0f;
    send->SetAttackSmoothing(mult + 15.0f);
    send->SetReleaseSmoothing(mult + 40.0f);
}

void GameMicManager::SetSynapseAmount(float f1) {
    FxSendSynapse *send = unk20->Find<FxSendSynapse>("synapse.send", true);
    send->SetAmount(f1);
}

void GameMicManager::SetSynapseFocus(float f1) {
    mSynapseFocus = f1;
    FxSendSynapse *send = unk20->Find<FxSendSynapse>("synapse.send", true);
    send->SetProximityFocus(f1);
}

#ifdef HX_NATIVE
void GameMicManager::InitFakeMics() {
    if (mFakeMics.empty()) {
        mFakeMics.resize(4);
        for (int i = 0; i < 4; i++) {
            mFakeMics[i] = new GameMic(-1);
        }
    }
}
#endif
