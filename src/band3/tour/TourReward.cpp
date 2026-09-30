#include "tour/TourReward.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "tour/Tour.h"
#include "utl/Symbol.h"

// Retail TU: 0x82364428-0x82364AA0 (vtable .?AVTourReward@@ at 0x8203F394).
// Ported from the rb3-Wii oracle (band3/tour/TourReward.cpp). Retail has no
// ValidatePropertyModification calls (dev-build only in the oracle).

TourReward::TourReward() : mRewards(NULL) {}

TourReward::~TourReward() {}

void TourReward::Init(const DataArray *i_pConfig) {
    mRewards = const_cast<DataArray *>(i_pConfig);
}

void TourReward::ApplyRewardEntry(TourProgress *tp, DataArray *da) const {
    static Symbol sym_band("band");
    static Symbol sym_perf("perf");
    Symbol s = da->Sym(0);
    if (s == sym_band) {
        DataArray *pEntryArray = da->Array(1);
        MILO_ASSERT(pEntryArray, 42);
        ApplyRewardEntry(tp, tp->GetTourProperties(), pEntryArray);
    } else if (s == sym_perf) {
        DataArray *pEntryArray = da->Array(1);
        MILO_ASSERT(pEntryArray, 49);
        ApplyRewardEntry(tp, tp->GetPerformanceProperties(), pEntryArray);
    } else {
        ApplyRewardEntry(tp, tp->GetTourProperties(), da);
    }
}

void TourReward::ApplyRewardEntry(
    TourProgress *tp, TourPropertyCollection &tpc, DataArray *da
) const {
    static Symbol sym_add("+");
    static Symbol sym_subtract("-");
    static Symbol sym_multiply("*");
    static Symbol sym_divide("/");
    Symbol s = da->Sym(0);
    if (s == sym_add) {
        ApplyAddReward(tp, tpc, da);
    } else if (s == sym_subtract) {
        ApplySubtractReward(tp, tpc, da);
    } else if (s == sym_multiply) {
        ApplyMultiplyReward(tp, tpc, da);
    } else if (s == sym_divide) {
        ApplyDivideReward(tp, tpc, da);
    } else {
        MILO_WARN("Unknown reward entry (%s).", s.Str());
    }
}

void TourReward::Apply(TourProgress *tp) const {
    if (mRewards) {
        for (int i = 1; i < mRewards->Size(); i++)
            ApplyRewardEntry(tp, mRewards->Array(i));
        tp->HandleTourRewardApplied();
    }
}

void TourReward::ApplyRewardValue(
    TourProgress *, TourPropertyCollection &pc, Symbol s, float f
) const {
    pc.GetPropertyValue(s);
    pc.SetPropertyValue(s, f);
}

void TourReward::ApplyAddReward(
    TourProgress *tp, TourPropertyCollection &pc, DataArray *i_pArray
) const {
    MILO_ASSERT(i_pArray->Size() == 3, 129);
    Symbol s = i_pArray->Sym(1);
    float val = i_pArray->Float(2);
    float f = pc.GetPropertyValue(s) + val;
    ApplyRewardValue(tp, pc, s, f);
}

void TourReward::ApplySubtractReward(
    TourProgress *tp, TourPropertyCollection &pc, DataArray *i_pArray
) const {
    MILO_ASSERT(i_pArray->Size() == 3, 149);
    Symbol s = i_pArray->Sym(1);
    float f = pc.GetPropertyValue(s) - i_pArray->Float(2);
    ApplyRewardValue(tp, pc, s, f);
}

void TourReward::ApplyMultiplyReward(
    TourProgress *tp, TourPropertyCollection &pc, DataArray *i_pArray
) const {
    MILO_ASSERT(i_pArray->Size() == 3, 168);
    Symbol s = i_pArray->Sym(1);
    float val = i_pArray->Float(2);
    float f = pc.GetPropertyValue(s) * val;
    ApplyRewardValue(tp, pc, s, f);
}

void TourReward::ApplyDivideReward(
    TourProgress *tp, TourPropertyCollection &pc, DataArray *i_pArray
) const {
    MILO_ASSERT(i_pArray->Size() == 3, 187);
    Symbol s = i_pArray->Sym(1);
    float f = pc.GetPropertyValue(s) / i_pArray->Float(2);
    ApplyRewardValue(tp, pc, s, f);
}
