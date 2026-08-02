#include "GigFilter.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/SongSortMgr.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "tour/QuestManager.h"
#include "tour/Tour.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols4.h"

GigFilter::GigFilter()
    : mName(""), mIsInternal(0), mFilteredPartSym(gNullStr), mWeight(1.0f) {}

GigFilter::~GigFilter() {}

void GigFilter::Init(const DataArray *i_pConfig) {
    static Symbol is_internal("is_internal");
    static Symbol weight("weight");
    static Symbol part_difficulty_filter("part_difficulty_filter");
    static Symbol filter("filter");
    MILO_ASSERT(i_pConfig, 0x1E);
    mName = i_pConfig->Sym(0);
    i_pConfig->FindData(is_internal, mIsInternal, false);
    i_pConfig->FindData(weight, mWeight, false);
    mFilteredPartSym = gNullStr;
    i_pConfig->FindData(part_difficulty_filter, mFilteredPartSym, false);
    DataArray *filterarr = i_pConfig->FindArray(filter, false);
    if (filterarr) {
        for (int i = 1; i < filterarr->Size(); i++) {
            DataArray *pEntry = filterarr->Array(i);
            MILO_ASSERT(pEntry, 0x34);
            MILO_ASSERT(pEntry->Size() == 2, 0x35);
            FilterType ty = (FilterType)pEntry->Int(0);
            Symbol s = pEntry->Sym(1);
            mFilter.AddFilter(ty, s);
        }
    }
}

Symbol GigFilter::GetName() const { return mName; }
bool GigFilter::IsInternal() const { return mIsInternal; }

const SongSortMgr::SongFilter &GigFilter::GetFilter() const { return mFilter; }

Symbol GigFilter::GetFilteredPartSym() const { return mFilteredPartSym; }

void GigFilter::InitializeMusicLibraryTask(
    MusicLibrary::MusicLibraryTask &task, int size, Symbol s
) const {
    task.maxSetlistSize = size;
    task.filter = mFilter;
    task.partSym = mFilteredPartSym;
    if (s != gNullStr) {
        GigFilter *pSecondaryFilter = TheQuestMgr.GetQuestFilter(s);
        MILO_ASSERT(pSecondaryFilter, 102);
        SongSortMgr::SongFilter secondaryFilter = pSecondaryFilter->mFilter;
        task.filter.IntersectFilter(&secondaryFilter);
        Symbol secondaryPartSym = pSecondaryFilter->mFilteredPartSym;
        task.partSym = TheTour->CombinePartSymbols(task.partSym, secondaryPartSym);
    }
    task.allowDuplicates = false;
    task.setlistMode = MusicLibrary::kSetlistForced;
}

float GigFilter::GetWeight() const { return mWeight; }
