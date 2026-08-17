#include <stdio.h>
#include "synth/MetaMusic.h"
#include "obj/ObjMacros.h"
#include "synth/Synth.h"
#include "synth/FxSendEQ.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "utl/Symbols.h"
#include "decomp.h"

void MetaMusicLoader::DoneLoading() {}

inline MetaMusicLoader::MetaMusicLoader(File *f, int &bytes, unsigned char *buf, int size)
    : Loader(FilePath(""), kLoadFront), mFile(f), mBytesRead(bytes), mBuf(buf),
      mBufSize(size) {
    MILO_ASSERT(mFile, 0x2A);
    mState = &MetaMusicLoader::OpenFile;
}

MetaMusic::MetaMusic(const char *cc)
    : mStream(0), mLoop(0), mFadeTime(1.0f), mVolume(0), mPlayFromBuffer(1), mRndHeap(0),
      mBufferH(0), mBuf(0), mFile(0), mBufSize(0), mBytesRead(0), mExtraFaders(this),
      mLoader(0), unk78(0), unk88(cc), unk8c(1) {
    mFader = Hmx::Object::New<Fader>();
    mFaderMute = Hmx::Object::New<Fader>();
}

MetaMusic::~MetaMusic() {
    RELEASE(mStream);
    UnloadStreamFx();
    RELEASE(mFile);
    RELEASE(mLoader);
    if (mRndHeap) {
        if (mBufferH) {
            mBufferH->Unlock();
            MemFreeH(mBufferH);
            mBufferH = 0;
        }
        mBuf = 0;
    } else if (mBuf) {
        MemFree(mBuf);
        mBuf = 0;
    }
    delete mFader;
    delete mFaderMute;
}

void MetaMusic::Load(const char *cc, float f, bool b1, bool b2) {
    mLoop = b2;
    unk8c = b1;
    DataArray *cfg = SystemConfig("synth", "metamusic");
    cfg->FindData("fade_time", mFadeTime, true);
    cfg->FindData("volume", mVolume, true);
    mVolume += f;
    cfg->FindData("play_from_memory", mPlayFromBuffer, true);
    mStartTimes.clear();
    DataArray *startPtsArr = cfg->FindArray("start_points_ms", false);
    if (startPtsArr) {
        for (int i = 1; i < startPtsArr->Size(); i++) {
            mStartTimes.push_back(startPtsArr->Int(i));
        }
    }
    if (mPlayFromBuffer) {
        MILO_ASSERT(!mBuf, 0xB2);
        TheSynth->NewStreamFile(cc, mFile, mExt);
        mBytesRead = 0;
        if (!mFile) {
            MILO_FAIL("\nMetagame music not found:\n%s\n", cc);
        }
        mBufSize = mFile->Size();
        if (!mRndHeap) {
            // NOTE (match): retail homes this size into a frame temp AND passes it in
            // r3 (a redundant `stw r3, 0x58(r31)` before the alloc call). That store
            // only appears when the size is a local declared INSIDE this block whose
            // live range ends at the MemAlloc call; hoisting it above the `if`, or
            // keeping it live past the call, both lose the store. See DB notes.
            int size = mBufSize;
            mBuf = (unsigned char *)MemAlloc(size, __FILE__, 0xC1, "MetaMusic", 0);
            MILO_ASSERT(!mLoader, 0xC2);
            mLoader = new MetaMusicLoader(mFile, mBytesRead, mBuf, mBufSize);
        }
    } else
        mFilename = cc;
}

void MetaMusicLoader::OpenFile() {
    mFile->ReadAsync(mBuf, mBufSize);
    mState = &MetaMusicLoader::LoadFile;
}

void MetaMusicLoader::LoadFile() {
    if (mFile->ReadDone(mBytesRead)) {
        mState = &MetaMusicLoader::DoneLoading;
    }
}

#pragma push
#pragma force_active on
inline bool MetaMusic::Loaded() {
    bool isLoaded = 0;
    if (mPlayFromBuffer == 0 || (mBuf != 0 && mFile == 0)) {
        isLoaded = 1;
    }
    return isLoaded;
}
#pragma pop

void MetaMusic::Poll() {
    if (mRndHeap && !mBuf) {
        int i18, i1c, i20, i24;
        MemFreeBlockStats(MemFindHeap("rnd"), i18, i1c, i20, i24);
        if (i24 > mBufSize + 0x20) {
            static int _x = MemFindHeap("rnd");
            MemTempHeap tmp(_x);
            mBufferH = _MemAllocH(mBufSize);
            mBuf = (unsigned char *)mBufferH->Lock();
            MILO_ASSERT(!mLoader, 0xE9);
            mLoader = new MetaMusicLoader(mFile, mBytesRead, mBuf, mBufSize);
        } else {
            return;
        }
    }
    if (mLoader && mBytesRead == mBufSize) {
        RELEASE(mLoader);
        RELEASE(mFile);
    }
    // NOTE: retail RB3 predates the `&& !ThePlatformMgr.GuideShowing()` gate
    // that the rb3-Wii dev build / DC3 carry here.
    if (mStream && !mStream->IsPlaying() && mStream->IsReady()) {
        mFader->SetVal(-96.0f);
        mFader->DoFade(mVolume, mFadeTime * 1000.0f);
        mStream->Play();
    }
    if (mStream && mStream->IsPlaying()) {
        if (!mFader->IsFading() && mFader->mVal == -96.0f) {
            RELEASE(mStream);
            UnloadStreamFx();
        } else
            UpdateMix();
    }
}

void MetaMusic::Start() {
    if (!mPlayFromBuffer || mBuf) {
        if (mStream && mStream->IsPlaying()) {
            mFader->DoFade(mVolume, mFadeTime * 1000.0f);
        } else {
            MILO_ASSERT(Loaded(), 0x122);
            RELEASE(mStream);
            UnloadStreamFx();
            if (mPlayFromBuffer) {
                MILO_ASSERT(mBuf, 0x128);
                mStream =
                    TheSynth->NewBufStream(mBuf, mBufSize, mExt, ChooseStartMs(), true, true);
            } else {
                MILO_ASSERT(!mFilename.empty(), 0x12D);
                // Retail passes floatSamples=TRUE here (li r7,0x1 at the call
                // site); the rb3-Wii dev build passes false.  Consistent with the
                // mPlayFromBuffer branch above, which also requests float samples.
                mStream =
                    TheSynth->NewStream(mFilename.c_str(), ChooseStartMs(), 0, true);
            }
            mStream->Faders()->Add(mFaderMute);
            mStream->Faders()->Add(mFader);
            for (ObjPtrList<Fader>::iterator it = mExtraFaders.begin();
                 it != mExtraFaders.end();
                 ++it) {
                mStream->Faders()->Add(*it);
            }
            if (mLoop) {
                mStream->SetJump(Stream::kStreamEndMs, 0, 0);
            }
            if (unk88) {
                LoadStreamFx();
                for (int i = 0; i < 6; i++) {
                    mStream->SetFXSend(i, unk70[i]->Find<FxSendEQ>("eq.send", true));
                }
            }
            unk78 = true;
        }
    }
}

// matches in retail
void MetaMusic::UpdateMix() {
    if (!unk88) {
        if (mStream && mStream->GetNumChannels() == 2) {
            if (unk8c) {
                mStream->SetPan(0, -2.0f);
                mStream->SetPan(1, 2.0f);
            } else {
                mStream->SetPan(0, -1.0f);
                mStream->SetPan(1, 1.0f);
            }
        }
    } else {
        static Symbol vols("vols");
        static Symbol pans("pans");
        MILO_ASSERT(m_CurrentFxConfig, 0x16F);
        DataArray *volsArr = m_CurrentFxConfig->FindArray(vols);
        DataArray *pansArr = m_CurrentFxConfig->FindArray(pans);
        float f15, f16;
        f16 = (float)unk84 / 90.0f;
        f15 = 1.0f - f16;
        int numChannels = Min(mStream->GetNumChannels(), 6);
        if (unk80 && unk84 <= 90) {
            DataArray *volsArr80 = unk80->FindArray(vols);
            DataArray *pansArr80 = unk80->FindArray(pans);
            for (int i = 0; i < numChannels; i++) {
                char buf[16];
                sprintf(buf, "channel_%d", i + 1);
                DataArray *chanArr7c = m_CurrentFxConfig->FindArray(buf, false);
                DataArray *chanArr80 = unk80->FindArray(buf, false);
                if (chanArr7c && chanArr80) {
                    for (ObjDirItr<FxSend> it(unk70[i], true); it != nullptr; ++it) {
                        it->EnableUpdates(false);
                        DataArray *thisFxConfigPost =
                            chanArr7c->FindArray(it->Name(), false);
                        DataArray *thisFxConfigPre =
                            chanArr80->FindArray(it->Name(), false);
                        MILO_ASSERT(thisFxConfigPost, 0x18C);
                        MILO_ASSERT(thisFxConfigPre, 0x18D);
                        MILO_ASSERT(thisFxConfigPre->Size() == thisFxConfigPost->Size(), 0x18E);
                        for (int j = 1; j < thisFxConfigPre->Size(); j++) {
                            DataArray *yetAnotherArr80 = thisFxConfigPre->Array(j);
                            DataArray *yetAnotherArr7c = thisFxConfigPost->Array(j);
                            it->SetProperty(
                                yetAnotherArr80->Sym(0),
                                f15 * yetAnotherArr80->Float(1)
                                    + f16 * yetAnotherArr7c->Float(1)
                            );
                        }
                        it->EnableUpdates(true);
                    }
                }
                mStream->SetVolume(
                    i, f15 * volsArr80->Float(i + 1) + f16 * volsArr->Float(i + 1)
                );
                mStream->SetPan(
                    i, f15 * pansArr80->Float(i + 1) + f16 * pansArr->Float(i + 1)
                );
            }

        } else if (unk84 == 0) {
            for (int i = 0; i < numChannels; i++) {
                char buf[16];
                sprintf(buf, "channel_%d", i + 1);
                DataArray *chanArr = m_CurrentFxConfig->FindArray(buf, false);
                if (chanArr) {
                    for (ObjDirItr<FxSend> it(unk70[i], true); it != nullptr; ++it) {
                        it->EnableUpdates(false);
                        DataArray *fxArr = chanArr->FindArray(it->Name(), false);
                        for (int j = 1; j < fxArr->Size(); j++) {
                            DataArray *propArr = fxArr->Array(j);
                            it->SetProperty(propArr->Sym(0), propArr->Node(1));
                        }
                        it->EnableUpdates(true);
                    }
                }
                mStream->SetVolume(i, volsArr->Float(i + 1));
                mStream->SetPan(i, pansArr->Float(i + 1));
            }
        }
        unk84++;
    }
}

DECOMP_FORCEACTIVE(MetaMusic, "mStream")

bool MetaMusic::IsPlaying() const { return mStream; }

bool MetaMusic::IsFading() const { return mFader->IsFading(); }

void MetaMusic::Stop() {
    if (mStream) {
        if (!mStream->IsPlaying()) {
            RELEASE(mStream);
            UnloadStreamFx();
        } else
            mFader->DoFade(-96.0f, mFadeTime * 1000.0f);
        unk78 = false;
    }
}

void MetaMusic::Mute() { mFaderMute->DoFade(-96.0f, 1000.0); }

void MetaMusic::UnMute() { mFaderMute->DoFade(0.0f, 1000.0); }

void MetaMusic::AddFader(Fader *fader) {
    if (fader)
        mExtraFaders.push_back(fader);
    else
        MILO_WARN("trying to add null fader");
}

void MetaMusic::SetScene(MetaMusicScene *scene) {
    if (scene) {
        DataArray *mix = scene->GetMix();
        if (mix) {
            if (!mStream || !mStream->IsPlaying()) {
                Start();
                unk80 = nullptr;
            } else
                unk80 = m_CurrentFxConfig;
            m_CurrentFxConfig = mix;
            if (m_CurrentFxConfig != unk80) {
                unk84 = 0;
            }
        }
    } else
        Stop();
}

void MetaMusic::LoadStreamFx() {
    unk70.reserve(6);
    unk70.resize(6);
    FilePath fp(".", unk88);
    for (int i = 0; i < 6; i++) {
        unk70[i].LoadFile(fp, true, false, kLoadFront, false);
        unk70[i].PostLoad(nullptr);
    }
}

void MetaMusic::UnloadStreamFx() {
    if (mStream) {
        for (int i = 0; i < 6; i++) {
            mStream->SetFXSend(i, nullptr);
        }
    }
    unk70.clear();
}

int MetaMusic::ChooseStartMs() const {
    int startMs = 0;

    if (mStartTimes.size() != 0) {
        // pick a random element
        int randomInt = RandomInt(0, mStartTimes.size());
        startMs = mStartTimes[randomInt];
    }

    return startMs;
}

BEGIN_HANDLERS(MetaMusic)
    HANDLE_ACTION(stop, Stop())
    HANDLE_ACTION(start, Start())
    HANDLE_ACTION(mute, Mute())
    HANDLE_ACTION(unmute, UnMute())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x252)
END_HANDLERS

// sw3 cross-dialect scatter-include (default/MetaMusic <- rndobj/PropAnim.cpp) [Object owner]
#ifndef SW_SCATTER_OWNER_INCLUDE
#define SW_SCATTER_OWNER_INCLUDE
#define gRev gRev_PropAnim
#define gAltRev gAltRev_PropAnim
#include "obj/dialect_object_push.h"
#include "rndobj/PropAnim.cpp"
#include "obj/dialect_object_pop.h"
#undef gRev
#undef gAltRev
#undef SW_SCATTER_OWNER_INCLUDE
#endif

// Retail's MetaMusic TU emits MakeString<FilePath> at 0x8270fef0 (caller-side
// inversion, lane laneO-wrongunit).  Ours lost the call site -- almost certainly
// a stripped MILO_WARN/MILO_LOG -- so the COMDAT was never instantiated here.
template const char *MakeString<FilePath>(const char *, FilePath);
