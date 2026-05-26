#include "synth_xbox/Synth.h"
#include "synth_xbox/HeadsetXferEffect.h"
#include "FxSendBitCrush.h"
#include "FxSendChorus.h"
#include "FxSendCompress.h"
#include "FxSendDelay.h"
#include "FxSendDistortion.h"
#include "FxSendEQ.h"
#include "FxSendFlanger.h"
#include "FxSendMeterEffect.h"
#include "synth_xbox/FxSendPitchShift360.h"
#include "FxSendReverb.h"
#include "synth_xbox/FxSendSynapse360.h"
#include "FxSendWah.h"
#include "Synth.h"
#include "macros.h"
#include "math/Decibels.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "stl/_algobase.h"
#include "synth/Synth.h"
#include "synth_xbox/ExternalMic.h"
#include "synth_xbox/FxSend.h"
#include "synth_xbox/Mic.h"
#include "synth_xbox/StreamReceiver360.h"
#include "synth_xbox/SynthSample.h"
#include "utl/Std.h"
#include "xdk/xapilibi/xbox.h"
#include "xdk/xaudio2/xaudio2.h"

class ExternalMicClientMgr {
public:
    static void Associate(int, MicXbox *);
};

Synth360 *TheXboxSynth;

Synth360::Synth360()
    : unke8(0), unkec(0), unkf0(0), unkf4(0), unkf8(0), unkfc(0), mDolbyEnabled(true),
      mDolbyPending(false), unk138(false), unk13c(0), unk14c(false) {}

BEGIN_HANDLERS(Synth360)
    HANDLE_ACTION(set_headset_target, Voice::sHeadsetTarget = _msg->Int(2))
    HANDLE_SUPERCLASS(Synth)
END_HANDLERS

void Synth360::Init() {
    Synth::Init();
    SynthSample360::Init();
    StreamReceiver360::Init();
    REGISTER_OBJ_FACTORY(FxSendReverb360)
    REGISTER_OBJ_FACTORY(FxSendDelay360)
    REGISTER_OBJ_FACTORY(FxSendCompress360)
    REGISTER_OBJ_FACTORY(FxSendEQ360)
    REGISTER_OBJ_FACTORY(FxSendFlanger360)
    REGISTER_OBJ_FACTORY(FxSendMeterEffect360)
    REGISTER_OBJ_FACTORY(FxSendWah360)
    REGISTER_OBJ_FACTORY(FxSendBitCrush360)
    REGISTER_OBJ_FACTORY(FxSendDistortion360)
    REGISTER_OBJ_FACTORY(FxSendChorus360)
    REGISTER_OBJ_FACTORY(FxSendPitchShift360)
    REGISTER_OBJ_FACTORY(FxSendSynapse360)

    Symbol enableHeadsetSym("enable_headset_output");
    DataArray *synthCfg = SystemConfig(Symbol("synth"));
    if (synthCfg->FindArray(enableHeadsetSym, true)->Int(1)) {
        SetupHeadsetSubmixes();
    }

    float micVolume = 0.0f;
    Symbol volumeSym("volume");
    DataArray *micCfg = SystemConfig(Symbol("synth"), Symbol("mic"));
    micCfg->FindData(volumeSym, micVolume, false);

    if (GetNumMics() > 0) {
        MicManagerXbox::GetInstance()->Init();
        mMics.resize(GetNumMics(), nullptr);
        ExternalMic::Init();
        for (unsigned int i = 0; i < mMics.size(); i++) {
            mMics[i] = new MicXbox(-1, DbToRatio(micVolume));
            ExternalMicClientMgr::Associate(i, dynamic_cast<MicXbox *>(mMics[i]));
        }
    }
}

Mic *Synth360::GetMic(int index) { return mMics[index]; }

bool Synth360::HasPendingVoices() { return Voice::HasPendingVoices(); }

bool Synth360::DidMicsChange() const {
    if (mMics.empty())
        return false;
    else {
        MicManagerXbox *x = MicManagerXbox::GetInstance();
        return x->mMicsChanged;
    }
}

void Synth360::ResetMicsChanged() {
    if (!mMics.empty()) {
        MicManagerXbox *x = MicManagerXbox::GetInstance();
        x->mMicsChanged = false;
    }
}

void Synth360::CaptureMic(int micID) {
    MILO_ASSERT_RANGE(micID, 0, mMics.size(), 0x350);
    MILO_ASSERT(!mMics[micID]->IsInUse(), 0x351);
    mMics[micID]->MarkAsInUse(true);
}

void Synth360::ReleaseAllMics() {
    for (int i = 0; i < mMics.size(); i++) {
        mMics[i]->MarkAsInUse(false);
    }
}

void Synth360::AddFxSend(FxSend360 *fx) { mFxSends.push_back(fx); }

bool Synth360::IsMicConnected(int i) const {
    if (i < 0 || i >= mMics.size())
        return false;
    else {
        return mMics[i]->GetType() != 0;
    }
}

void Synth360::RequirePushToTalk(bool b, int i) {
    if (!mMics.empty()) {
        MicManagerXbox::GetInstance()->RequirePushToTalk(b, i);
    }
}

void Synth360::ReleaseMic(int micID) {
    MILO_ASSERT_RANGE(micID, 0, mMics.size(), 0x35b);
    if (!mMics[micID]->IsInUse()) {
        MILO_NOTIFY_ONCE("Releasing a microphone [%d]that was not in use\n", micID);
    }
    mMics[micID]->MarkAsInUse(false);
}

void Synth360::RemoveFxSend(FxSend360 *fx) {
    auto *findFx = std::find(mFxSends.begin(), mFxSends.end(), fx);
    if (findFx != mFxSends.end()) {
        mFxSends.erase(findFx);
    }
}

IXAudio2SubmixVoice *Synth360::GetHeadsetSubmix(int i) {
    if (!mHeadsetSubmixes.empty() && i != -1) {
        return mHeadsetSubmixes[i];
    }
    return nullptr;
}

int Synth360::GetNextAvailableMicID() const {
    for (int i = 0; i < mMics.size(); i++) {
        if (!mMics[i]->IsInUse() && mMics[i]->GetType() != 0)
            return i;
    }
    return -1;
}

void Synth360::SetupHeadsetSubmixes() {
    // Ensure mHeadsetSubmixes has exactly 4 entries
    if (mHeadsetSubmixes.size() > 4) {
        mHeadsetSubmixes.erase(mHeadsetSubmixes.begin() + 4, mHeadsetSubmixes.end());
    } else {
        mHeadsetSubmixes.resize(4, 0);
    }

    for (int i = 0; i < 4; i++) {
        // Build send descriptors for this headset submix
        std::vector<XAUDIO2_SEND_DESCRIPTOR> sendDescs;
        XAUDIO2_SEND_DESCRIPTOR desc;
        memset(&desc, 0, sizeof(desc));
        desc.Flags = 0;
        desc.pOutputVoice = 0;
        sendDescs.push_back(desc);

        XAUDIO2_VOICE_SENDS voiceSends;
        voiceSends.SendCount = sendDescs.size();
        voiceSends.pSends = &sendDescs[0];

        MILO_ASSERT(mHeadsetSubmixes[i] == 0, 0);
    }
}

void Synth360::SetDolby(bool b1, bool b2) {
    if (b2) {
        mDolbyEnabled = b1;
        UpdateDolby();
    } else if (mDolbyEnabled != b1) {
        mDolbyTimer.Restart();
        mDolbyEnabled = b1;
        mDolbyPending = true;
    }
}
