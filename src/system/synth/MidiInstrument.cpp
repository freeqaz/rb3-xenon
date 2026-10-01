#include "synth/MidiInstrument.h"
#include "SampleZone.h"
#include "math/Decibels.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "synth/FxSend.h"
#include "synth/Synth.h"
#include "synth/Utl.h"
#include <algorithm>

#pragma region NoteVoiceInst

NoteVoiceInst::NoteVoiceInst(
    MidiInstrument *owner,
    SampleZone *zone,
    unsigned char trigger,
    unsigned char ratio,
    int durFramesLeft,
    int glideID,
    float fineTune
)
    : mSample(nullptr), mVolume(0), mStartProgress(0), mTriggerNote(trigger),
      mCenterNote(zone->CenterNote()), mStarted(false), mStopped(false),
      mGlideID(glideID), mGlideFrames(0), mGlideToNote(0), mGlideFromNote(0),
      mGlideFramesLeft(-1), mFineTune(fineTune), mDurationFramesLeft(durFramesLeft),
      mOwner(owner) {
    if (zone->Sample()) {
        mSample = zone->Sample()->NewInst(false, 0, -1);
        float db = RatioToDb(ratio / 127.0f);
        mSample->SetBankVolume(zone->Volume() + db);
        mSample->SetBankPan(zone->Pan());
        mSample->SetBankSpeed(getCalculatedSpeed(mTriggerNote));
        mSample->SetFXCore(zone->GetFXCore());
        mSample->SetADSR(zone->ADSR());
        mSample->SetSend(owner->GetSend());
    }
}

NoteVoiceInst::~NoteVoiceInst() { RELEASE(mSample); }

void NoteVoiceInst::Start() {
    mStarted = true;
    mSample->SetStartProgress(mStartProgress);
#ifdef HX_NATIVE
    mSample->Play(mOwner->Faders().GetVal() + mVolume);
#else
    // RB3 retail: the non-virtual SampleInst::Start (stop, then
    // StartImpl); no volume is pushed here.
    mSample->Start();
#endif
}

void NoteVoiceInst::Stop() {
    mStopped = true;
    mSample->Stop(false);
}

bool NoteVoiceInst::IsRunning() { return mSample->IsPlaying(); }

void NoteVoiceInst::SetTranspose(float transpose) {
    float speed = CalcSpeedFromTranspose(transpose);
    mSample->SetSpeed(speed);
}

void NoteVoiceInst::UpdateVolume() {
    if (mSample && mOwner) {
        mSample->SetVolume(mOwner->Faders().GetVal() + mVolume);
    }
}

// NoteVoiceInst::UpdatePan() removed -- see the note at its former declaration
// in MidiInstrument.h. Retail's vtable has no such slot, RB3 has
// no such method, and nothing here called it. It is DC3-only, and its body
// (SetPan(0.0f), i.e. hard-centre the voice) was live behaviour we do not want
// to inherit by accident.

void NoteVoiceInst::SetPan(float pan) { mSample->SetPan(pan); }

void NoteVoiceInst::SetVolume(float volume) {
    mVolume = volume;
    UpdateVolume();
}

float NoteVoiceInst::getCalculatedSpeed(float f1) {
    return CalcSpeedFromTranspose(mFineTune / 100.0f + (f1 - mCenterNote));
}

void NoteVoiceInst::Poll() {
    if (mDurationFramesLeft == 0) {
        Stop();
    } else {
        if (mDurationFramesLeft > 0) {
            mDurationFramesLeft--;
        }
        if (mGlideFramesLeft >= 0) {
            float interped = Interp(
                mGlideFromNote,
                mGlideToNote,
                (float)(mGlideFrames - mGlideFramesLeft--) / (float)mGlideFrames
            );
            mSample->SetBankSpeed(
                CalcSpeedFromTranspose(mFineTune / 100.0f + (interped - mCenterNote))
            );
        }
    }
}

// Retail 0x827131D8 (92 B) and 0x82713608 (108 B): the speed goes through
// getCalculatedSpeed (bl 0x827130B0) and then CalcSpeedFromTranspose again (bl
// 0x8270EFE0).
void NoteVoiceInst::SetFineTune(float tune) {
    mFineTune = tune;
    if (mGlideFramesLeft <= 0) {
        mSample->SetBankSpeed(CalcSpeedFromTranspose(getCalculatedSpeed(mTriggerNote)));
    }
}

void NoteVoiceInst::GlideToNote(unsigned char note, int frames) {
    frames = std::max(1, frames);
    if (mSample) {
        mGlideToNote = note;
        mGlideFromNote = mTriggerNote;
        mTriggerNote = note;
        mGlideFrames = frames;
        mGlideFramesLeft = frames;
    }
}

#pragma endregion
#pragma region MidiInstrument

MidiInstrument::MidiInstrument()
    : mMultiSampleMap(this), mPatchNumber(0), mSend(this), mReverbMixDb(kDbSilence),
      mReverbEnable(false), mActiveVoices(this), mFaders(this), mFineTuneCents(0) {
    mFaders.Add(TheSynth->MasterFader());
    mFaders.Add(TheSynth->InstFader());
}

#ifdef HX_NATIVE
MidiInstrument::~MidiInstrument() {
    if (ObjectDir::InDeleteObjects()) {
        mActiveVoices.clear();
        return;
    }
    mActiveVoices.DeleteAll();
}
#endif

BEGIN_HANDLERS(MidiInstrument)
    HANDLE_ACTION(add_map, mMultiSampleMap.push_back())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(SampleZone)
    SYNC_PROP(sample, o.mSample)
    SYNC_PROP(volume, o.mVolume)
    SYNC_PROP(pan, o.mPan)
    SYNC_PROP(centernote, o.mCenterNote)
    SYNC_PROP(minnote, o.mMinNote)
    SYNC_PROP(maxnote, o.mMaxNote)
    SYNC_PROP(minvelo, o.mMinVel)
    SYNC_PROP(maxvelo, o.mMaxVel)
    SYNC_PROP(fx_core, (int &)o.mFXCore)
    SYNC_PROP(adsr, o.mADSR)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(MidiInstrument)
    SYNC_PROP(multisamplemaps, mMultiSampleMap)
    SYNC_PROP_SET(send, GetSend(), SetSend(_val.Obj<FxSend>()))
    SYNC_PROP_SET(reverb_mix_db, GetReverbMixDb(), SetReverbMixDb(_val.Float()))
    SYNC_PROP_SET(reverb_enable, GetReverbEnable(), SetReverbEnable(_val.Int()))
    SYNC_PROP(faders, mFaders)
    SYNC_PROP(patchnum, mPatchNumber)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(MidiInstrument)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mMultiSampleMap;
    bs << mSend;
    bs << mPatchNumber;
    mFaders.Save(bs);
    bs << mReverbMixDb;
    bs << mReverbEnable;
END_SAVES

BEGIN_COPYS(MidiInstrument)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(MidiInstrument)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax)
            COPY_MEMBER(mMultiSampleMap)
        COPY_MEMBER(mSend)
        COPY_MEMBER(mPatchNumber)
        COPY_MEMBER(mReverbMixDb)
        COPY_MEMBER(mReverbEnable)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(3, 0)

// RB3 retail (0x82715D50) Load: a plain int rev, a too-new rev
// skips the body, and the rev reaches the sample zones through SampleZone::gRev
// rather than a BinStreamRev -- the zone readers take the raw BinStream.
BEGIN_LOADS(MidiInstrument)
    int rev;
    bs >> rev;
    if (rev > 3)
        MILO_WARN("Can't load new MidiInstrument");
    else {
        SampleZone::gRev = rev;
        Hmx::Object::Load(bs);
        bs >> mMultiSampleMap;
        bs >> mSend;
        bs >> mPatchNumber;
        mFaders.Load(bs);
        if (rev >= 3) {
            bs >> mReverbMixDb;
            bs >> mReverbEnable;
        }
    }
#ifdef HX_NATIVE
    StartPolling();
#endif
END_LOADS

#ifdef HX_NATIVE
void MidiInstrument::SynthPoll() {
#else
void MidiInstrument::Poll() {
#endif
    if (!mActiveVoices.empty()) {
        for (auto it = mActiveVoices.begin(); it != mActiveVoices.end();) {
            NoteVoiceInst *cur = *it++;
            cur->Poll();
            if (cur->Started() && !cur->IsRunning()) {
                delete cur;
            }
        }
        if (mFaders.Dirty()) {
            // Retail RB3 updates volume only; DC3
            // (newer) added the UpdatePan() call.
            FOREACH (it, mActiveVoices) {
                (*it)->UpdateVolume();
            }
        }
    }
}

NoteVoiceInst *MidiInstrument::MakeNoteInst(
    SampleZone *zone,
    unsigned char triggerNote,
    unsigned char ratio,
    int durFramesLeft,
    int glideID
) {
    NoteVoiceInst *inst = new NoteVoiceInst(
        this, zone, triggerNote, ratio, durFramesLeft, glideID, mFineTuneCents
    );
    inst->UpdateVolume();
    return inst;
}

void MidiInstrument::PressNote(
    unsigned char note, unsigned char vel, int glideID, int glideFrames
) {
    if (glideID != -1) {
        bool found = false;
        for (ObjPtrList<NoteVoiceInst>::iterator it = mActiveVoices.begin();
             it != mActiveVoices.end();
             it++) {
            if ((*it)->GlideID() == glideID && !(*it)->Stopped()) {
                found = true;
                (*it)->GlideToNote(note, glideFrames);
                (*it)->SetFineTune(mFineTuneCents);
            }
        }
        if (found)
            return;
    }
    StartSample(note, vel, -1, glideID);
}

// Retail 0x827141D8 (104 B). Declared in MidiInstrument.h and defined NOWHERE in
// this tree until now -- the same declared-but-undefined hole W16-EH found for
// Shuttle::SetActive. Proven against retail bytes: the body loads the list head
// at 0x58(this) (mActiveVoices is at 0x50, its node pointer at +0x8), zero-extends
// the argument (`clrlwi r30, r4, 24` => unsigned char), compares it against
// `lbz r11, 0x34(r3)` (NoteVoiceInst::mTriggerNote, confirmed at 0x34 by
// /d1reportSingleClassLayout), and vcalls slot 0x58 == slot 22 == Stop().
void MidiInstrument::ReleaseNote(unsigned char uc) {
    for (ObjPtrList<NoteVoiceInst>::iterator it = mActiveVoices.begin();
         it != mActiveVoices.end();
         ++it) {
        // Operand order is load-bearing: retail
        // emits `cmplw cr6, r11, r30` (TriggerNote first), while
        // `uc == (*it)->TriggerNote()` emits `cmplw cr6, r30, r11`. One token,
        // 104 B.
        if ((*it)->TriggerNote() == uc) {
            (*it)->Stop();
        }
    }
}

// Retail 0x82714350 (96 B), which target_symbol_map.json mis-named
// `?clear@?$ObjPtrList@VTask@@VObjectDir@@@@QAAXXZ`. That name is a void() and
// CANNOT be this body: the body saves r4 (`mr r30, r4`) and forwards it into the
// per-node virtual call (`mr r4, r30`), so it consumes an argument the named
// signature does not have. It is MidiInstrument::Pause(bool) -- same list walk as
// ReleaseNote above, vcalling slot 0x58 with the bool.
//
// NOTE the call is on the SAMPLE, not the voice: retail does `lwz r3, 0x28(r11)`
// before the vcall, and mSample sits at 0x28 of NoteVoiceInst. SampleInst::Pause
// is vtable slot 22 == 0x58 (compiler-reported), so this is
// `(*it)->Sample()->Pause(b)`. A one-line NoteVoiceInst::Pause
// (`(*it)->Pause(b)`) is what retail INLINES away;
// writing that form literally would have emitted a vcall on the voice and
// not matched. Retail bytes decide.
// Retail 0x82713410 (8 B): `stfs f1, 0x7c(r3)` -- the range assert is
// MILO_ASSERT, which compiles away in this build.
void MidiInstrument::SetFineTune(float cents) {
    MILO_ASSERT_RANGE(cents, -100.f, 100.f, 0x1F9);
    mFineTuneCents = cents;
}

// Retail 0x827147E0 (8 B): `li r7, -1; b StartSample`.
void MidiInstrument::PlayNote(unsigned char uc1, unsigned char uc2, int i) {
    StartSample(uc1, uc2, i, -1);
}

// Retail 0x827145F0 (8 B): `addi r3, r3, 0x50; b <ObjPtrList::DeleteAll>` -- the
// list is mActiveVoices at 0x50.
void MidiInstrument::KillAllVoices() { mActiveVoices.DeleteAll(); }

void MidiInstrument::Pause(bool b) {
    for (ObjPtrList<NoteVoiceInst>::iterator it = mActiveVoices.begin();
         it != mActiveVoices.end();
         ++it) {
        (*it)->Sample()->Pause(b);
    }
}

void MidiInstrument::SetReverbMixDb(float db) {
    mReverbMixDb = db;
    FOREACH (it, mActiveVoices) {
        (*it)->Sample()->SetReverbMixDb(mReverbMixDb);
    }
}

void MidiInstrument::SetReverbEnable(bool enable) {
    mReverbEnable = enable;
    FOREACH (it, mActiveVoices) {
        (*it)->Sample()->SetReverbEnable(mReverbEnable);
    }
}

void MidiInstrument::SetSend(FxSend *send) {
    mSend = send;
    FOREACH (it, mActiveVoices) {
        (*it)->Sample()->SetSend(mSend);
    }
}

void MidiInstrument::StartSample(
    unsigned char note, unsigned char vel, int durFramesLeft, int glideID
) {
    for (int i = 0; i < mMultiSampleMap.size(); i++) {
        SampleZone &cur = mMultiSampleMap[i];
        if (cur.Includes(note, vel)) {
            NoteVoiceInst *inst = MakeNoteInst(&cur, note, vel, durFramesLeft, glideID);
            mActiveVoices.push_back(inst);
            inst->Start();
        }
    }
}

// sw2 scatter-include (default/MidiInstrument <- band3/bandtrack/GemTrack.cpp)
#define gRev gRev_GemTrack
#define gAltRev gAltRev_GemTrack
#include "band3/bandtrack/GemTrack.cpp"
#undef gRev
#undef gAltRev

