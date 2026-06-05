#include "meta_band/LessonProvider.h"
#include "BandProfile.h"
#include "bandobj/CheckboxDisplay.h"
#include "meta_band/LessonMgr.h"
#include "meta_band/ProfileMgr.h"
#include "os/Debug.h"
#include "ui/UIListCustom.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"

// Retail RB3-360 KEPT this TU's MILO_ASSERTs as condition evaluation (the
// fail-call was stripped, but the argument side-effects survive — e.g.
// GetLessonEntry's range assert still evaluates the virtual NumData()). The
// global Debug.h no-op (void)sizeof discards even the evaluation, which makes
// GetLessonEntry trivial enough for /Ob2 to inline it into every caller and
// breaks the match cascade. Evaluate-and-discard locally to restore the
// surviving-assert shape without calling the failer.
#ifndef HX_NATIVE
#undef MILO_ASSERT
#undef MILO_ASSERT_RANGE
#define MILO_ASSERT(cond, line) ((void)(cond))
#define MILO_ASSERT_RANGE(value, min, max, line)                                         \
    ((void)((min) <= (value) && (value) < (max)))
#endif

LessonProvider::LessonProvider() : mCategories(0), mLessons(0) {}

LessonProvider::~LessonProvider() {}

void LessonProvider::InitData(RndDir *dir) {
    mCategoryMat = dir->Find<RndMat>("category.mat", false);
    mLessonMat = dir->Find<RndMat>("lesson.mat", false);
}

bool LessonProvider::IsActive(int i) const {
        if (NumData() == 0) {
        return false;
    } else {
        return !GetLessonEntry(i).unk4;
    }
}

void LessonProvider::Text(int, int idx, UIListLabel *slot, UILabel *label) const {
    MILO_ASSERT(slot, 0x34);
    MILO_ASSERT(label, 0x35);
    const LessonEntry &entry = GetLessonEntry(idx);
    if (entry.unk4) {
        if (slot->Matches("category")) {
            label->SetTextToken(entry.unk0);
        } else if (slot->Matches("progress")) {
            Symbol key = entry.unk0;
            LessonMgr *pLessonMgr = LessonMgr::GetLessonMgr();
            MILO_ASSERT(pLessonMgr, 0x44);
            BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
            if (profile) {
                int count_completed =
                    pLessonMgr->GetCompletedCountFromCategory(profile, key);
                int count = pLessonMgr->GetTotalCountFromCategory(key);
                static Symbol s_trainer_progress("trainer_progress");
                label->SetTokenFmt(s_trainer_progress, count_completed, count);
            }
        } else
            label->SetTextToken(gNullStr);
    } else if (slot->Matches("lesson")) {
        label->SetTextToken(entry.unk0);
    } else
        label->SetTextToken(gNullStr);
}

RndMat *LessonProvider::Mat(int, int idx, UIListMesh *slot) const {
    MILO_ASSERT(slot, 100);
    const LessonEntry &entry = GetLessonEntry(idx);
    if (slot->Matches("bg")) {
        if (entry.unk4)
            return mCategoryMat;
        else
            return mLessonMat;
    } else
        return nullptr;
}

void LessonProvider::Custom(int, int idx, UIListCustom *slot, Hmx::Object *obj) const {
    MILO_ASSERT(slot, 0x79);
    const LessonEntry &entry = GetLessonEntry(idx);
    if (slot->Matches("checkbox")) {
        CheckboxDisplay *pCheckboxDisplay = dynamic_cast<CheckboxDisplay *>(obj);
        MILO_ASSERT(pCheckboxDisplay, 0x80);
        if (entry.unk4)
            pCheckboxDisplay->SetShowing(false);
        else {
            pCheckboxDisplay->SetShowing(true);
            BandProfile *profile = TheProfileMgr.GetPrimaryProfile();
            if (profile) {
                if (profile->IsLessonComplete(entry.unk0, 1.0f))
                    pCheckboxDisplay->SetChecked(true);
                else
                    pCheckboxDisplay->SetChecked(false);
            }
        }
    }
}

Symbol LessonProvider::DataSymbol(int idx) const { return GetLessonEntry(idx).unk0; }

int LessonProvider::NumData() const { return mLessonEntries.size(); }

void LessonProvider::Update(Symbol s) {
    mLessonEntries.erase(mLessonEntries.begin(), mLessonEntries.end());
    LessonMgr *pLessonMgr = LessonMgr::GetLessonMgr();
    MILO_ASSERT(pLessonMgr, 0xB1);
    mCategories = pLessonMgr->GetCategoriesFromTrainer(s);
    MILO_ASSERT(mCategories, 0xB5);
    for (std::vector<Symbol>::iterator it = mCategories->begin();
         it != mCategories->end();
         ++it) {
        Symbol cur = *it;
        mLessonEntries.push_back(LessonEntry(cur, true));
        mLessons = pLessonMgr->GetLessonsFromCategory(cur);
        MILO_ASSERT(mLessons, 0xC1);
        for (std::vector<Symbol>::iterator lit = mLessons->begin();
             lit != mLessons->end();
             ++lit) {
            mLessonEntries.push_back(LessonEntry(*lit, false));
        }
    }
}

const LessonProvider::LessonEntry &LessonProvider::GetLessonEntry(int data) const {
    MILO_ASSERT_RANGE(data, 0, NumData(), 0xD0);
    return mLessonEntries[data];
}
