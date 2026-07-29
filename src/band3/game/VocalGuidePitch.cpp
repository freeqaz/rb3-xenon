#include "game/VocalGuidePitch.h"
#include "beatmatch/VocalNote.h"
#include "decomp.h"
#include "game/SongDB.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/ProfileMgr.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "synth/MidiInstrument.h"
#include "synth/Synth.h"
#include "utl/Loader.h"
#include "utl/TimeConversion.h"

extern "C" VocalNote *NoteAt__13VocalNoteListCFf(const VocalNoteList *self, float ms);

VocalGuidePitch::VocalGuidePitch()
    : mGuideTrack(-1), unk8(0), mGuidePitch(0), mInstrument(0), mTuningOffset(0),
      mPaused(0), mPitchModifier(-12) {}

VocalGuidePitch::~VocalGuidePitch() {}

void VocalGuidePitch::Load() {
    DataArray *cfg = SystemConfig("sound", "instruments");
    mBank.LoadFile(
        FilePath(".", cfg->FindStr("chamberlin")), true, true, kLoadFront, false
    );
}

bool VocalGuidePitch::IsLoaded() const { return mBank.IsLoaded(); }
void VocalGuidePitch::FinishLoad() { mBank.PostLoad(nullptr); }
void VocalGuidePitch::Unload() { mBank = nullptr; }

static inline int RoundPitchBend(float f) {
    if (f > 0.0f) return (int)(f + 0.5f);
    else return (int)(f - 0.5f);
}

void VocalGuidePitch::Poll(float ms) {
    float adjMs = ms - TheProfileMgr.GetSongToTaskMgrMs(kGame);
    float pitchOffset = TheSongDB->GetPitchOffsetForTick((int)MsToTick(adjMs));
    if (-50.0f <= pitchOffset && pitchOffset <= 50.0f) {
        mTuningOffset = pitchOffset;
    }
    if (mGuideTrack != -1 && !mPaused) {
        mInstrument->SetFineTune(mTuningOffset);
        VocalNoteList *list = TheSongDB->GetVocalNoteList(mGuideTrack);
        MILO_ASSERT(list, 0x51);
        VocalNote *note = NoteAt__13VocalNoteListCFf(list, adjMs);
        if (note != (VocalNote *)unk8) {
            if (note == nullptr || note->mUnpitchedNote) {
                if (mGuidePitch != 0) {
                    mInstrument->ReleaseNote(mGuidePitch + mPitchModifier);
                    mGuidePitch = 0;
                }
            } else {
                int begin = note->mBeginPitch;
                if (begin != mGuidePitch) {
                    mGuidePitch = begin;
                    mInstrument->PressNote(begin + mPitchModifier, 127, 1, -1);
                    if (note->mBeginPitch != note->mEndPitch) {
                        mGuidePitch = note->mEndPitch;
                        mInstrument->PressNote(
                            mPitchModifier + note->mEndPitch, 127, 1,
                            // retail folds the constant as a direct 60.0f/1000.0f
                            // division (0x3D75C28F); our previous form (60.0f *
                            // note->mDurationMs * (1.0f/1000.0f)) folds the
                            // reciprocal-multiply instead, landing 1 ULP high
                            // (0x3D75C290). Use the literal directly.
                            RoundPitchBend(note->mDurationMs * 0.06f)
                        );
                    }
                } else if (note->mEndPitch != mGuidePitch) {
                    mGuidePitch = note->mEndPitch;
                    mInstrument->PressNote(
                        mPitchModifier + note->mEndPitch, 127, 1,
                        // see comment above: literal 0.06f matches retail's
                        // direct-division fold (0x3D75C28F)
                        RoundPitchBend(note->mDurationMs * 0.06f)
                    );
                }
            }
            unk8 = (int)note;
        }
    }
}

void VocalGuidePitch::EnableGuideTrack(int i1) {
    if (mGuideTrack != i1) {
        mInstrument->ReleaseNote(mGuidePitch + mPitchModifier);
        mGuideTrack = i1;
        mGuidePitch = 0;
        unk8 = 0;
    }
}

void VocalGuidePitch::Init() {
    mInstrument = mBank->Find<MidiInstrument>("Chamberlin.inst", false);
    TheSynth->GetMidiInstrumentMgr()->SetInstrument(mInstrument);
    mGuideTrack = -1;
    mGuidePitch = 0;
    mTuningOffset = 0;
    unk8 = 0;
    mPaused = false;
    mPitchModifier = -12;
}

void VocalGuidePitch::Terminate() {
    mInstrument = nullptr;
    TheSynth->GetMidiInstrumentMgr()->UnloadInstrument();
}

void VocalGuidePitch::SetSong(const Symbol &s) {
    int songID = TheSongMgr.GetSongIDFromShortName(s, true);
    UpdateTuning(((BandSongMetadata *)TheSongMgr.Data(songID))->TuningOffset());
    SetVolume(((BandSongMetadata *)TheSongMgr.Data(songID))->GuidePitchVolume());
}

void VocalGuidePitch::UpdateTuning(float tuning) {
    mTuningOffset = tuning;
    mInstrument->SetFineTune(tuning);
}

int VocalGuidePitch::GetGuideTrack() const { return mGuideTrack; }
void VocalGuidePitch::SetVolume(float vol) {
    TheSynth->InstFader()->SetVal(vol);
}

void VocalGuidePitch::Pause(bool b1) {
    mInstrument->Pause(b1);
    mPaused = b1;
}

bool VocalGuidePitch::IsPaused() const { return mPaused; }
