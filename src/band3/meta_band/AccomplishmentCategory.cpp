#include "AccomplishmentCategory.h"
#include "os/Debug.h"

AccomplishmentCategory::AccomplishmentCategory(const DataArray *config, int index)
    : mIndex(index), mName(""), mGroup(""), mAward("") {
    Configure(config);
}

AccomplishmentCategory::~AccomplishmentCategory() {}
// Retail declares `group` and `award` as FUNCTION-LOCAL statics (one shared
// guard word, group claiming bit 0x1, award claiming bit 0x2, in that call
// order), not the utl/Symbols.h globals -- W16-GZ localstatic lever.
void AccomplishmentCategory::Configure(const DataArray *i_pConfig) {
    MILO_ASSERT(i_pConfig, 0x1e);

    mName = i_pConfig->Sym(0);
    static Symbol group("group");
    i_pConfig->FindData(group, mGroup, true);
    static Symbol award("award");
    i_pConfig->FindData(award, mAward, false);
}

Symbol AccomplishmentCategory::GetName() const { return mName; }

int AccomplishmentCategory::GetIndex() const { return mIndex; }

Symbol AccomplishmentCategory::GetGroup() const { return mGroup; }

Symbol AccomplishmentCategory::GetAward() const { return mAward; }

bool AccomplishmentCategory::HasAward() const { return !(mAward == ""); }
