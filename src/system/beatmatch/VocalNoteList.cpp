#include "beatmatch/VocalNote.h"
#include "beatmatch/SongData.h"
#include "os/System.h"
#include "math/Utl.h"
#include "utl/MemMgr.h"
#include <algorithm>
#include <cfloat> // FLT_MAX (the Wii oracle's 3.4028235E+38f decimal literal
                  // overflows MSVC X360's parser (C2177); FLT_MAX is the exact
                  // same value, 0x7F7FFFFF, and is MSVC/clang portable)
#include <functional>

const char *VocalNoteList::PrintTick(int tick) const {
    return TickFormat(tick, *mSongData->GetMeasureMap());
}

VocalPhrase::VocalPhrase()
    : unk0(0), unk4(0), unk8(-1), unkc(-1), unk10(-1), unk14(-1), unk18(0), unk19(0),
      unk1a(0), unk1c(0), unk20(0), unk24(FLT_MAX), unk28(-FLT_MAX),
      unk2c(0), mTambourinePhrase(0), unk30(0), unk34(0) {}

VocalNoteList::VocalNoteList(SongData *data)
    : mSongData(data), mFreestyleMinDuration(0), mFreestylePad(0) {
    DataArray *scoringArr = SystemConfig()->FindArray("scoring", false);
    if (scoringArr) {
        DataArray *vocalsArr = SystemConfig("scoring")->FindArray("vocals", false);
        if (vocalsArr) {
            mFreestyleMinDuration =
                vocalsArr->FindArray("freestyle_min_duration")->Array(1);
            mFreestylePad = vocalsArr->FindArray("freestyle_pad")->Array(1);
            if (mFreestyleMinDuration->Size() != mFreestylePad->Size()) {
                MILO_WARN(
                    "scoring.dta: must have same number of items in both freestyle_min_duration and freestyle_pad."
                );
            }
        }
    }
}

void VocalNoteList::Clear() {
    mPhrases.clear();
    mLyricPhrases.clear();
    mNotes.clear();
    mTambourineGems.clear();
    mFreestyleSections.clear();
}

void CopyPhraseVec(const std::vector<VocalPhrase> &src, std::vector<VocalPhrase> *dest) {
    MILO_ASSERT(dest, 0x56);
    dest->clear();
    MILO_ASSERT(dest->size() == 0, 0x59);
    for (std::vector<VocalPhrase>::const_iterator it = src.begin(); it != src.end();
         it++) {
        dest->push_back(*it);
    }
    MILO_ASSERT(dest->size() == src.size(), 0x62);
}

void VocalNoteList::CopyPhrasesFrom(const VocalNoteList *srcList) {
    MILO_ASSERT(srcList, 0x68);
    CopyPhraseVec(srcList->mPhrases, &mPhrases);
}

void VocalNoteList::CopyLyricPhrases() { CopyPhraseVec(mPhrases, &mLyricPhrases); }

// fn_80497850
void VocalNoteList::AddNote(const VocalNote &note) {
    MemDoTempAllocations tmp;
    if (!mNotes.empty() && mNotes.back().GetTick() == note.GetTick()) {
        MILO_WARN(
            "%s (%s): double note-on at %s",
            mSongData->SongFullPath(),
            mTrackName,
            PrintTick(note.GetTick())
        );
    } else
        mNotes.push_back(note);
}

// fn_80497928
void VocalNoteList::NotesDone(const TempoMap &tmap, bool b) {
    static bool sDump;
    if (mPhrases.empty()) {
        if (!mNotes.empty()) {
            MILO_WARN(
                "%s (PART VOCALS): Vocal notes exist, but no vocal phrases found",
                mSongData->SongFullPath()
            );
        }
        return;
    }
    if (mNotes.empty())
        return;

    int ticktouse = mPhrases[0].unk8 < mNotes[0].GetTick() ? mPhrases[0].unk8
                                                           : mNotes[0].GetTick();
    if (b) {
        VocalPhrase phrase;
        phrase.unk8 = 0;
        if (0x280 < ticktouse)
            phrase.unkc = ticktouse - 0x280;
        else
            phrase.unkc = ticktouse;
        mPhrases.insert(mPhrases.begin(), phrase);
    }

    float currentMin = FLT_MAX;
    int noteIdx = 0;
    float currentMax = -FLT_MAX;
    int lastRangeBoundingPhrase = -1;
    if (sDump)
        MILO_LOG("parsing phrase data\n");
    int noteEnd;
    for (int phraseIdx = 1; phraseIdx < mPhrases.size(); phraseIdx++) {
        VocalPhrase &phrase = mPhrases[phraseIdx];
        phrase.unk18 = 0;
        phrase.unk19 = 0;
        phrase.unk10 = noteIdx;
        phrase.unk14 = noteIdx;
        for (; noteIdx != mNotes.size(); noteIdx++) {
            VocalNote &note = mNotes[noteIdx];
            if (note.GetTick() < phrase.unk8) {
                if (b) {
                    MILO_WARN(
                        "%s (%s): vocal note at tick %s is outside any phrases",
                        mSongData->SongFullPath(),
                        mTrackName,
                        PrintTick(note.GetTick())
                    );
                    phrase.unkc += phrase.unk8 - note.GetTick();
                    phrase.unk8 = note.GetTick();
                } else {
                    MILO_WARN(
                        "%s (%s): vocal note [%d-%d] at tick %s is outside any phrases",
                        mSongData->SongFullPath(),
                        mTrackName,
                        note.StartPitch(),
                        note.EndPitch(),
                        PrintTick(note.GetTick())
                    );
                }
            }
            if (note.GetTick() >= phrase.unk8 + phrase.unkc)
                break;
            phrase.unk14++;
            if (note.IsUnpitched())
                phrase.unk19 = 1;
            if (b && note.GetTick() + note.GetDurationTicks() > phrase.unk8 + phrase.unkc) {
                MILO_WARN(
                    "%s (%s): vocal note at tick %s extends beyond phrase",
                    mSongData->SongFullPath(),
                    mTrackName,
                    PrintTick(note.GetTick())
                );
            }
        }

        if (phrase.unk10 != phrase.unk14) {
            mNotes[phrase.unk14 - 1].SetPhraseEnd(true);
        }
        if (b) {
            mLyricPhrases.push_back(phrase);
        }

        noteEnd = phrase.unk14;
        for (int j = phrase.unk10; j < noteEnd; j++) {
            if (!mNotes[j].IsUnpitched()) {
                phrase.unk18 = 1;
                phrase.unk24 = Min<float>((float)mNotes[j].StartPitch(), phrase.unk24);
                phrase.unk24 = Min<float>((float)mNotes[j].EndPitch(), phrase.unk24);
                phrase.unk28 = Max<float>(phrase.unk28, (float)mNotes[j].StartPitch());
                phrase.unk28 = Max<float>(phrase.unk28, (float)mNotes[j].EndPitch());
            }
            if (b && mNotes[j].LyricShift()) {
                VocalPhrase &backphrase = mLyricPhrases.back();
                int endtick = mNotes[j].EndTick();
                int oldStart = backphrase.unk8;
                int oldDur = backphrase.unkc;
                backphrase.unkc = endtick - oldStart;
                int oldEnd = oldStart + oldDur;
                VocalPhrase newphrase;
                newphrase.unk8 = endtick;
                newphrase.unkc = oldEnd - endtick;
                mLyricPhrases.push_back(newphrase);
            }
        }

        currentMin = Min<float>(phrase.unk24, currentMin);
        currentMax = Max<float>(currentMax, phrase.unk28);
        if (phrase.unk1a || phraseIdx + 1 == mPhrases.size()) {
            for (int k = lastRangeBoundingPhrase + 1; k <= phraseIdx; k++) {
                mPhrases[k].unk24 = currentMin;
                mPhrases[k].unk28 = currentMax;
            }
            currentMin = FLT_MAX;
            lastRangeBoundingPhrase = phraseIdx;
            currentMax = -FLT_MAX;
        }
    }

    if (sDump) {
        for (int i = 0; i < mPhrases.size(); i++) {
            MILO_LOG(
                "[%d] ticks: (%d, %d), min: %.0f max: %.0f bounding: %d\n",
                i,
                mPhrases[i].unk8,
                mPhrases[i].unk8 + mPhrases[i].unkc,
                mPhrases[i].unk24,
                mPhrases[i].unk28,
                mPhrases[i].unk1a
            );
        }
    }

    if (noteIdx != mNotes.size()) {
        MILO_WARN(
            "%s (%s): vocal notes past end of last phrase are being discarded",
            mSongData->SongFullPath(),
            mTrackName
        );
        mNotes.resize(noteIdx);
    }

    int gem;
    for (int i = 0; i < mTambourineGems.size(); i++) {
        gem = mTambourineGems[i];
        int phraseIdx = 0;
        while (phraseIdx < mPhrases.size()
               && gem >= mPhrases[phraseIdx].unk8 + mPhrases[phraseIdx].unkc) {
            phraseIdx++;
        }
        if (phraseIdx < mPhrases.size() && gem >= mPhrases[phraseIdx].unk8
            && mPhrases[phraseIdx].unk10 == mPhrases[phraseIdx].unk14) {
            mPhrases[phraseIdx].mTambourinePhrase = true;
        } else {
            MILO_LOG(
                "NOTIFY: %s (%s): tambourine gem at tick %s not in phrase or in singing phrase; discarding\n",
                mSongData->SongFullPath(),
                mTrackName,
                PrintTick(gem)
            );
            mTambourineGems.erase(mTambourineGems.begin() + i);
            i--;
        }
    }

    if (b)
        DeterminePhraseTimes(tmap);

    for (int i = 0; i != mPhrases.size(); i++) {
        VocalPhrase &phrase = mPhrases[i];
        for (int j = phrase.unk10; j < phrase.unk14; j++) {
            mNotes[j].mPhrase = i;
            mNotes[j].mPlayerMask = phrase.unk2c;
        }
    }
    Finalize();
}

void VocalNoteList::DeterminePhraseTimes(const TempoMap &tmap) {
    for (unsigned int i = 0; mPhrases.size() != i; i++) {
#ifdef HX_NATIVE
        // libstdc++'s vector::iterator is a class type, not a raw pointer, so
        // (a) we can't assign begin() + i to T*, and (b) vector::insert wants
        // an iterator argument. Use indexed access + begin()+i for insert.
        VocalPhrase *phrase = &mPhrases[i];
#else
        VocalPhrase *phrase = mPhrases.begin() + i;
#endif
        int prevEnd = 0;
        if (i != 0) {
            prevEnd = phrase[-1].unk8 + phrase[-1].unkc;
        }
        if (i != 0 && phrase->mTambourinePhrase
            && phrase->unk8 > prevEnd + 0x780) {
            VocalPhrase newPhrase;
            newPhrase.unk8 = prevEnd;
            newPhrase.unkc = (phrase->unk8 - prevEnd) - 0x280;
            VocalPhrase *insertPos = &mPhrases[i];
            newPhrase.mTambourinePhrase = insertPos[-1].mTambourinePhrase;
#ifdef HX_NATIVE
            mPhrases.insert(mPhrases.begin() + i, newPhrase);
#else
            mPhrases.insert(insertPos, newPhrase);
#endif
            i--;
        } else {
            phrase->unkc = phrase->unkc + (phrase->unk8 - prevEnd);
            phrase->unk8 = prevEnd;
            float startTime = tmap.TickToTime(prevEnd);
            float endTime = tmap.TickToTime(phrase->unk8 + phrase->unkc);
            phrase->unk0 = startTime;
            phrase->unk4 = endTime - startTime;
        }
    }
}

void VocalNoteList::StartPlayerPhrase(int tick, int player) {
    if (!mPhrases.empty() && mPhrases.back().unkc == -1) {
        if (tick > mPhrases.back().unk8 + 0x1e0) {
            MILO_WARN(
                "%s (%s): confused by vocal phrase overlap around tick %s",
                mSongData->SongFullPath(),
                mTrackName,
                PrintTick(tick)
            );
        }
    } else {
        VocalPhrase phrase;
        mPhrases.push_back(phrase);
        mPhrases.back().unk8 = tick;
    }
    mPhrases.back().unk2c |= 1 << player;
}

void VocalNoteList::EndPlayerPhrase(int tick, int) {
    MILO_ASSERT(!mPhrases.empty(), 0x24d);
    if (mPhrases.back().unkc != -1
        && tick > mPhrases.back().unk8 + mPhrases.back().unkc + 0x1e0) {
        MILO_WARN(
            "%s (%s): confused by vocal phrase overlap around tick %s",
            mSongData->SongFullPath(),
            mTrackName,
            PrintTick(tick)
        );
    }
    int duration = tick - mPhrases.back().unk8;
    if (duration < 0x1e0) {
        MILO_WARN(
            "%s (%s): confused by vocal phrase overlap around tick %s",
            mSongData->SongFullPath(),
            mTrackName,
            PrintTick(tick)
        );
    }
    mPhrases.back().unkc = duration;
}

void VocalNoteList::Finalize() {
    std::vector<VocalNote>(mNotes).swap(mNotes);
    DetermineFreestyleSections();
}

void VocalNoteList::DetermineFreestyleSections() {
    MILO_ASSERT(mFreestyleSections.empty(), 0x287);
    float sectionStart = 0.0f;
    bool atWordBoundary = true;
    for (std::vector<VocalNote>::iterator note = mNotes.begin(); note != mNotes.end();
         ++note) {
        if (atWordBoundary) {
            float gap = note->GetMs() - sectionStart;
            for (int i = 0; i < mFreestyleMinDuration->Size(); i++) {
                float pad = mFreestylePad->Float(i);
                float minDuration = mFreestyleMinDuration->Float(i);
                if (gap > 64.0f * pad + minDuration) {
                    mFreestyleSections.push_back(
                        std::make_pair(sectionStart + pad, note->GetMs() - pad)
                    );
                    break;
                }
            }
        }
        atWordBoundary = false;
        sectionStart = note->EndMs();
        String &text = note->mText;
        if (text.empty()
            || (text.rindex(-1) != '-' && text.rindex(-1) != '=')) {
            atWordBoundary = true;
        }
    }
    mFreestyleSections.push_back(std::make_pair(
        sectionStart + mFreestyleMinDuration->Float(0), FLT_MAX
    ));
}

void VocalNoteList::AddTambourineGem(int gem) { mTambourineGems.push_back(gem); }

void VocalNoteList::SetFreestyleSections(const std::vector<std::pair<float, float> > &sects
) {
    mFreestyleSections = sects;
}

bool VocalNoteList::IsIllegalFreestyleSection(
    DataArray *arr, const std::pair<float, float> &section
) {
    float duration = section.second - section.first;
    for (int i = 0; i < arr->Size(); i++) {
        if (duration >= arr->Float(i))
            return false;
    }
    return true;
}

void VocalNoteList::GenerateLegalFreestyleSections(
    std::vector<std::pair<float, float> > &out
) const {
    float sectionStart = 0.0f;
    float pad = mFreestylePad->Float(0);
    for (const VocalNote *note = mNotes.data();
         note != mNotes.data() + mNotes.size();
         ++note) {
        if (note->IsUnpitched()) {
            float sectionEnd = note->GetMs() - pad;
            std::pair<float, float> p(sectionStart, sectionEnd);
            if (p.second - p.first > 0.0f) {
                out.push_back(p);
            }
            sectionStart = pad + note->EndMs();
        }
    }
    out.push_back(std::make_pair(sectionStart, FLT_MAX));
}

#ifdef HX_NATIVE
// libstdc++'s std::binder1st instantiates with both T& and const T& overloads,
// which collapse to the same signature once T already carries a const-ref
// qualifier — yielding an ambiguous-overload error. Use a small callable that
// keeps the same call shape (`pred(section)`) the rest of this routine relies on.
namespace {
struct IsIllegalFreestylePred_HX {
    DataArray *arr;
    IsIllegalFreestylePred_HX(DataArray *a) : arr(a) {}
    bool operator()(const std::pair<float, float> &s) const {
        return VocalNoteList::IsIllegalFreestyleSection(arr, s);
    }
};
} // namespace
#endif

namespace {
// STLport's remove_if = find_if + remove_copy_if, but its *public* find_if
// wrapper (stl/_algobase.h) is not tagged inline, so under -inline noauto a
// direct std::remove_if() call leaves find_if out-of-line. Reconstruct the
// inlinable pieces here so the whole remove-erase inlines exactly as the
// target does: the 4-wide Duff's-device search returns by value (the trailing
// ++first go dead → offset addressing rather than induction), and each stage
// takes the predicate by value (the target's three stacked pred copies).
//
// The two-tier find (FindIf wrapper -> Find impl) mirrors STLport's real
// find_if -> __find_if call chain; that extra by-value hop is what drives the
// compiler to lay the find/remove_copy predicate copies into the target's
// stack slots (0x10/0x14 and 0x8/0xc) rather than swapping them.
template <class _Pred>
inline std::vector<std::pair<float, float> >::iterator FindInvalidFreestyle(
    std::vector<std::pair<float, float> >::iterator first,
    std::vector<std::pair<float, float> >::iterator last,
    _Pred pred
) {
    for (int trip = (last - first) >> 2; trip > 0; --trip) {
        if (pred(*first))
            return first;
        ++first;
        if (pred(*first))
            return first;
        ++first;
        if (pred(*first))
            return first;
        ++first;
        if (pred(*first))
            return first;
        ++first;
    }
    switch (last - first) {
    case 3:
        if (pred(*first))
            return first;
        ++first;
    case 2:
        if (pred(*first))
            return first;
        ++first;
    case 1:
        if (pred(*first))
            return first;
    default:
        return last;
    }
}

template <class _Pred>
inline std::vector<std::pair<float, float> >::iterator FindIfInvalidFreestyle(
    std::vector<std::pair<float, float> >::iterator first,
    std::vector<std::pair<float, float> >::iterator last,
    _Pred pred
) {
    return FindInvalidFreestyle(first, last, pred);
}

template <class _Pred>
inline std::vector<std::pair<float, float> >::iterator RemoveCopyInvalidFreestyle(
    std::vector<std::pair<float, float> >::iterator first,
    std::vector<std::pair<float, float> >::iterator last,
    std::vector<std::pair<float, float> >::iterator result,
    _Pred pred
) {
    for (; first != last; ++first) {
        if (!pred(*first)) {
            *result = *first;
            ++result;
        }
    }
    return result;
}

template <class _Pred>
inline std::vector<std::pair<float, float> >::iterator RemoveInvalidFreestyle(
    std::vector<std::pair<float, float> >::iterator first,
    std::vector<std::pair<float, float> >::iterator last,
    _Pred pred
) {
    first = FindIfInvalidFreestyle(first, last, pred);
    if (first == last)
        return first;
    else {
        std::vector<std::pair<float, float> >::iterator next = first;
        return RemoveCopyInvalidFreestyle(++next, last, first, pred);
    }
}
} // namespace

void VocalNoteList::RemoveInvalidFreestyleSections() {
    std::vector<std::pair<float, float> >::iterator first = RemoveInvalidFreestyle(
        mFreestyleSections.begin(), mFreestyleSections.end(),
#ifdef HX_NATIVE
        IsIllegalFreestylePred_HX(mFreestyleMinDuration));
#else
        std::bind1st(std::ptr_fun(IsIllegalFreestyleSection), mFreestyleMinDuration));
#endif
    mFreestyleSections.erase(first, mFreestyleSections.end());
}

void VocalNoteList::CapLastFreestyleSection(float ms) {
    while (!mFreestyleSections.empty() && mFreestyleSections.back().first >= ms) {
        mFreestyleSections.erase(mFreestyleSections.end() - 1, mFreestyleSections.end());
    }
    if (!mFreestyleSections.empty() && mFreestyleSections.back().second > ms) {
        mFreestyleSections.back().second = ms;
    }
}

bool VocalNoteCmp(float ms, const VocalNote &note) { return ms < note.GetMs(); }

VocalNote *VocalNoteList::NextNote(float ms) const {
    if (0 == mNotes.size())
        return NULL;
    std::vector<VocalNote>::const_iterator it =
        std::upper_bound(mNotes.begin(), mNotes.end(), ms, VocalNoteCmp);
#ifdef HX_NATIVE
    // libstdc++'s const_iterator is a wrapper class around the raw pointer, so
    // C-style cast to T* is rejected. Recover the underlying pointer through
    // &*it (and const_cast to match the original mutable return contract).
    if (it == mNotes.begin())
        return const_cast<VocalNote *>(&*it);
    if (ms <= it[-1].GetDurationMs() + it[-1].GetMs())
        return const_cast<VocalNote *>(&*(it - 1));
    if (it == mNotes.end())
        return NULL;
    return const_cast<VocalNote *>(&*it);
#else
    if (it == mNotes.begin())
        return (VocalNote *)it;
    if (ms <= it[-1].GetDurationMs() + it[-1].GetMs())
        return (VocalNote *)(it - 1);
    if (it == mNotes.end())
        return NULL;
    return (VocalNote *)it;
#endif
}

const VocalNote *VocalNoteList::NoteAt(float ms) const {
    const VocalNote *it =
        std::upper_bound(mNotes.begin(), mNotes.end(), ms, VocalNoteCmp);
    if (it == mNotes.begin())
        return NULL;
    --it;
    MILO_ASSERT((*it).GetMs() <= ms, 0x22f);
    if (ms <= it->GetDurationMs() + it->GetMs())
        return it;
    return NULL;
}

float VocalNoteList::PitchAt(float ms) const {
    const VocalNote *it =
        std::upper_bound(mNotes.begin(), mNotes.end(), ms, VocalNoteCmp);
    if (it == mNotes.begin())
        return 0.0;
    --it;
    MILO_ASSERT(it->GetMs() <= ms, 0x1ff);
    float noteMs = it->GetMs();
    float noteDur = it->GetDurationMs();
    if (ms <= noteMs + noteDur) {
        if (it->EndPitch() == it->StartPitch())
            return (float)it->StartPitch();
        float fraction =
            Max<float>(0.0f, Min<float>(ms, noteMs + noteDur) - noteMs)
            / noteDur;
        return fraction * (float)it->EndPitch()
            + (1.0f - fraction) * (float)it->StartPitch();
    }
    return 0.0f;
}

void VocalNoteList::GetPracticePhrases(
    std::vector<VocalPhrase> &out, int startTick, int endTick
) const {
    for (const VocalPhrase *phrase = mPhrases.data();
         phrase != mPhrases.data() + mPhrases.size();
         ++phrase) {
        if (startTick < phrase->unk8 + phrase->unkc
            && endTick > phrase->unk8) {
            out.push_back(*phrase);
        }
    }
}

void VocalNoteList::GetPracticePhrases2(
    std::vector<VocalPhrase> &out, int startTick, int endTick
) const {
    for (const VocalPhrase *phrase = mPhrases.data();
         phrase != mPhrases.data() + mPhrases.size();
         ++phrase) {
        if (startTick < phrase->unk8 + phrase->unkc && endTick > phrase->unk8
            && phrase->unk8 + phrase->unkc <= endTick) {
            out.push_back(*phrase);
        }
    }
}

int VocalNoteList::GetNumPracticePhrases(const std::vector<VocalPhrase> &phrases) const {
    int count = 0;
    for (const VocalPhrase *phrase = phrases.data();
         phrase != phrases.data() + phrases.size();
         ++phrase) {
        if (HasNoteInRange(phrase->unk8, phrase->unk8 + phrase->unkc) != -1)
            count++;
    }
    return count;
}

void VocalNoteList::AddLyricShift(float ms) {
    std::vector<VocalNote>::iterator it =
        std::upper_bound(mNotes.begin(), mNotes.end(), ms, VocalNoteCmp);
    if (it == mNotes.begin()) {
        MILO_WARN(
            "%s: Added lyric shift before lyrics at time %f",
            mSongData->SongFullPath(),
            ms
        );
    } else {
        it[-1].mLyricShift = true;
    }
}

bool VocalNote::PlayableBy(int activeNum) const {
    MILO_ASSERT(activeNum == 0 || activeNum == 1, 0x3d2);
    return (mPlayerMask & (1 << activeNum)) != 0;
}

void VocalNoteList::UpdatePitchRangeTickDelimited(
    int startTick, int endTick, float &min, float &max
) {
    // .begin()/.end() spelling preserved from the prior tree stub — it matches
    // retail (the oracle's .data() spelling regressed 2 whole-binary matches).
    VocalNote *it = mNotes.begin();
    VocalNote *end = mNotes.end();
    for (; it != end; ++it) {
        if (it->IsUnpitched())
            continue;
        if (it->GetTick() < startTick)
            continue;
        if (endTick > -1 && it->GetTick() > endTick)
            break;
        int startPitch = it->StartPitch();
        if ((float)startPitch < min)
            min = (float)startPitch;
        if ((float)startPitch > max)
            max = (float)startPitch;
        int endPitch = it->EndPitch();
        if ((float)endPitch < min)
            min = (float)endPitch;
        if ((float)endPitch > max)
            max = (float)endPitch;
    }
}

int VocalNoteList::HasNoteInRange(int startTick, int endTick) const {
    // .begin()/.end() spelling preserved from the prior tree stub (matches retail).
    for (const VocalNote *it = mNotes.begin(); it != mNotes.end(); ++it) {
        if (!it->IsUnpitched() && it->GetTick() <= endTick
            && it->EndTick() >= startTick) {
            return it->GetTick();
        }
    }
    return -1;
}
