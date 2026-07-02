#include "synth/MoggClip.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/File.h"
#include "synth/Stream.h"
#include "synth/Synth.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"

MoggClip::PanInfo::PanInfo(int c, float p) : channel(c), panning(p) {}

MoggClip::MoggClip()
    : mControllerVolume(0), mLoop(false), mVolume(0), mStream(nullptr), unk50(0),
      mData(nullptr), mDataSize(0), mLoader(nullptr), mFader(Hmx::Object::New<Fader>()),
      mUnloadWhenFinished(false), mPlaying(false), mLoopStartSample(0), mLoopEndSample(-1) {
    mFaders.push_back(mFader);
    StartPolling();
}

void MoggClip::SetSend(FxSend *send) { mFxSend = send; }

MoggClip::~MoggClip() {
    RELEASE(mLoader);
    RELEASE(mFader);
    KillStream();
    UnloadData();
}

BEGIN_HANDLERS(MoggClip)
    HANDLE_ACTION(play, Play(0))
    HANDLE_ACTION(stop, Stop(0))
    HANDLE_ACTION(set_pan, SetPan(_msg->Int(2), _msg->Float(3)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(MoggClip)
    SYNC_PROP_SET(file, mMoggFile, SetFile(_val.Str()))
    SYNC_PROP_SET(volume, mControllerVolume, SetControllerVolume(_val.Float()))
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(MoggClip)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mMoggFile << mControllerVolume << mLoop;
    bs << mLoopStartSample << mLoopEndSample;
    if (bs.Cached()) {
        FileLoader::SaveData(bs, mData, mDataSize);
    }
END_SAVES

BEGIN_COPYS(MoggClip)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(MoggClip)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mMoggFile)
        COPY_MEMBER(mControllerVolume)
        COPY_MEMBER(mLoop)
        COPY_MEMBER(mLoopStartSample)
        COPY_MEMBER(mLoopEndSample)
    END_COPYING_MEMBERS
END_COPYS

BEGIN_LOADS(MoggClip)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

INIT_REVS(2, 0)

void MoggClip::PreLoad(BinStream &bs) {
    int rev;
    bs >> rev;
    if (rev > 2)
        MILO_WARN("Can't load new MoggClip");
    else {
        Hmx::Object::Load(bs);
        bs >> mMoggFile >> mControllerVolume >> mLoop;
        if (rev > 1)
            bs >> mLoopStartSample >> mLoopEndSample;
        LoadFile(rev > 0 ? &bs : 0);
    }
}

void MoggClip::PostLoad(BinStream &bs) {
    EnsureLoaded();
    LoadNumChannels();
}

const char *MoggClip::GetSoundDisplayName() {
    return !mPlaying ? gNullStr
                     : MakeString("MoggClip: %s", FileGetName(mMoggFile.c_str()));
}

void MoggClip::SynthPoll() {
    if (mPlaying && mStream) {
        mStream->PollStream();
        if (!mStream->IsPlaying() && mStream->IsReady()) {
            if (mPanInfos.empty()) {
                int chans = mStream->GetNumChannels();
                if (chans == 1) {
                    mStream->SetPan(0, 0);
                } else if (chans == 2) {
                    mStream->SetPan(0, -1);
                    mStream->SetPan(1, 1);
                }
            }
            mStream->Play();
        } else {
            if (mStream->IsFinished() || mFader->mVal == -96.0f) {
                KillStream();
                if (mUnloadWhenFinished) {
                    UnloadData();
                }
            }
        }
    }
}

void MoggClip::Play(float f1) {
    if (EnsureLoaded()) {
        KillStream();
        Stream *stream = TheSynth->NewBufStream(mData, mDataSize, "mogg", 0, false);
        mStream = dynamic_cast<StandardStream *>(stream);
        if (!mStream) {
            delete stream;
        } else {
            mFader->SetVal(0);
            SetVolume(f1);
            SetControllerVolume(mControllerVolume);
            UpdateFaders();
            UpdatePanInfo();
            ApplyLoop(mLoop, mLoopStartSample, mLoopEndSample);
            mPlaying = true;
        }
    } else
        MILO_WARN("Mogg file not loaded: '%s'", mMoggFile.c_str());
}

void MoggClip::Stop(bool b1) {
    KillStream();
    if (mUnloadWhenFinished) {
        UnloadData();
    }
}

void MoggClip::Pause(bool pause) {
    mPlaying = !pause;
    if (mStream && !mPlaying) {
        mStream->Stop();
    }
}

bool MoggClip::DonePlaying() { return !mStream; }

void MoggClip::SetVolume(float vol) {
    mVolume = vol;
    if (mStream) {
        mStream->Stream::SetVolume(mControllerVolume + mVolume);
    }
}

void MoggClip::SetPan(float f1) {
    if (mNumChannels == 1) {
        SetPan(0, f1);
    }
}

void MoggClip::EndLoop() { SetLoop(false, mLoopStartSample, mLoopEndSample); }

bool MoggClip::IsStreaming() const { return mStream && mStream->IsPlaying(); }

void MoggClip::ApplyLoop(bool b1, int i2, int i3) {
    if (mStream) {
        mStream->ClearJump();
        if (b1) {
            mStream->SetJumpSamples(i3, i2, 0);
        }
    }
}

void MoggClip::FadeOut(float f1) { mFader->DoFade(-96.0f, f1); }

void MoggClip::UnloadWhenFinishedPlaying(bool unload) { mUnloadWhenFinished = unload; }

bool MoggClip::IsReadyToPlay() const {
    if (mLoader)
        return mLoader->IsLoaded();
    else
        return mData && mDataSize > 0;
}

void MoggClip::KillStream() {
    mPlaying = false;
    RELEASE(mStream);
}

void MoggClip::UnloadData() {
    if (mData) {
        MemFree(mData);
        mData = nullptr;
        mDataSize = 0;
    }
}

void MoggClip::SetLoop(bool b1, int i2, int i3) {
    mLoop = b1;
    mLoopStartSample = i2;
    mLoopEndSample = i3;
    ApplyLoop(mLoop, mLoopStartSample, mLoopEndSample);
}

bool MoggClip::EnsureLoaded() {
    if (mLoader) {
        if (!mLoader->IsLoaded()) {
            MILO_WARN("MoggClip blocked while loading '%s'", mMoggFile.c_str());
            TheLoadMgr.PollUntilLoaded(mLoader, nullptr);
        }
        mData = mLoader->GetBuffer(&mDataSize);
        RELEASE(mLoader);
    }
    return mData && mDataSize > 0;
}

void MoggClip::UpdateFaders() {
    if (mStream) {
        for (std::vector<Fader *>::iterator it = mFaders.begin(); it != mFaders.end();
             ++it) {
            mStream->Faders()->Add(*it);
        }
    }
}

void MoggClip::UpdatePanInfo() {
    if (mStream) {
        for (std::vector<PanInfo>::iterator it = mPanInfos.begin(); it != mPanInfos.end();
             ++it) {
            mStream->SetPan(it->channel, it->panning);
        }
    }
}

void MoggClip::LoadNumChannels() {
    if (mMoggFile.empty()) {
        mNumChannels = -1;
        return;
    }
    if (mLoader && !mLoader->IsLoaded()) {
        TheLoadMgr.PollUntilLoaded(mLoader, nullptr);
    }
    SynthPoll();
    if (!mStream) {
        mNumChannels = -1;
        return;
    }
    int retries = 0;
    int numChannels = 0;
    while (retries < 200) {
        Timer::Sleep(1);
        TheSynth->Poll();
        numChannels = mStream->GetNumChannels();
        if (numChannels >= 1) {
            break;
        }
        retries++;
    }
    mNumChannels = numChannels;
    Stop(false);
    if (mNumChannels < 0) {
        mNumChannels = -1;
    }
}

void MoggClip::LoadFile(BinStream *bs) {
    RELEASE(mLoader);
    KillStream();
    UnloadData();
    if (!mMoggFile.empty()) {
        BinStream *toUse = (bs && bs->Cached()) ? bs : 0;
        mLoader = new FileLoader(
            mMoggFile,
            FileLocalize(mMoggFile.c_str(), nullptr),
            kLoadFront,
            0,
            false,
            true,
            toUse,
            nullptr
        );
    }
}

void MoggClip::SetFile(const char *file) {
    MILO_ASSERT(file != NULL, 0x14C);
    mMoggFile.Set(FilePath::Root().c_str(), file);
    LoadFile(nullptr);
}

void MoggClip::RemoveFader(Fader *fader) {
    if (fader) {
        for (std::vector<Fader *>::iterator it = mFaders.begin(); it != mFaders.end();
             ++it) {
            if (*it == fader) {
                mFaders.erase(it);
                break;
            }
        }
        if (mStream) {
            mStream->Faders()->Remove(fader);
        }
    }
}

void MoggClip::AddFader(Fader *fader) {
    if (fader) {
        bool found = false;
        for (std::vector<Fader *>::iterator it = mFaders.begin(); it != mFaders.end();
             ++it) {
            if (*it == fader) {
                found = true;
                break;
            }
        }
        if (!found) {
            mFaders.push_back(fader);
        }
        if (mStream) {
            mStream->Faders()->Add(fader);
        }
    }
}

void MoggClip::SetPan(int i1, float f2) {
    PanInfo info(i1, f2);
    bool found = false;
    for (std::vector<PanInfo>::iterator it = mPanInfos.begin(); it != mPanInfos.end();
         ++it) {
        if (it->channel == i1) {
            found = true;
            *it = info;
            break;
        }
    }
    if (!found) {
        mPanInfos.push_back(info);
    }
    if (mStream) {
        mStream->SetPan(info.channel, info.panning);
    }
}

void MoggClip::SetupPanInfo(float f1, float f2, bool stereo) {
    if (stereo) {
        SetPan(0, f2 / 2.0f + f1);
        SetPan(1, -f2 / 2.0 + f1);
    } else {
        SetPan(0, f1);
    }
}
