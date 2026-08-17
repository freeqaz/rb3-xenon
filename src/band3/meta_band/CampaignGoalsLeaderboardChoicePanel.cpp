#include "meta_band/CampaignGoalsLeaderboardChoicePanel.h"
#include "BandProfile.h"
#include "Campaign.h"
#include "game/BandUser.h"
#include "meta_band/Accomplishment.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/AccomplishmentProgress.h"
#include "meta_band/ProfileMgr.h"
#include "meta_band/TexLoadPanel.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "stl/_algo.h"
#include "ui/UILabel.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIPanel.h"
#include "utl/MemMgr.h"
#include "utl/Messages.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols3.h"
#include <algorithm>
#include <vector>

// Explicit specialization of _Temporary_buffer for Symbol iterators to use
// game allocator (_MemAlloc/_MemFree) instead of stdlib malloc/free.
#ifndef HX_NATIVE
// Host STL has no STLport _Temporary_buffer; native std::sort uses host allocator.
_STLP_BEGIN_NAMESPACE
template <>
class _Temporary_buffer<Symbol *, Symbol> {
private:
    ptrdiff_t _M_original_len;
    ptrdiff_t _M_len;
    Symbol *_M_buffer;

    void _M_allocate_buffer() {
        _M_original_len = _M_len;
        _M_buffer = 0;
        if (_M_len > (ptrdiff_t)(INT_MAX / sizeof(Symbol)))
            _M_len = INT_MAX / sizeof(Symbol);
        while (_M_len > 0) {
            _M_buffer = (Symbol *)MemAlloc(_M_len * sizeof(Symbol), __FILE__, 43, "_Temporary_buffer", 0);
            if (_M_buffer)
                break;
            _M_len /= 2;
        }
    }

    void _M_initialize_buffer(const Symbol &val, const __false_type &) {
        uninitialized_fill_n(_M_buffer, _M_len, val);
    }

public:
    ptrdiff_t size() const { return _M_len; }
    ptrdiff_t requested_size() const { return _M_original_len; }
    Symbol *begin() { return _M_buffer; }
    Symbol *end() { return _M_buffer + _M_len; }

    _Temporary_buffer(Symbol *__first, Symbol *__last) {
        _M_len = __last - __first;
        _M_allocate_buffer();
        if (_M_len > 0)
            _M_initialize_buffer(*__first, __false_type());
    }

    ~_Temporary_buffer() {
        _STLP_STD::_Destroy_Range(_M_buffer, _M_buffer + _M_len);
        MemFree(_M_buffer);
    }

private:
    _Temporary_buffer(const _Temporary_buffer<Symbol *, Symbol> &) {}
    void operator=(const _Temporary_buffer<Symbol *, Symbol> &) {}
};
_STLP_END_NAMESPACE
#endif

GoalCmp::GoalCmp(const AccomplishmentManager *mgr) : m_pAccomplishmentMgr(mgr) {}

bool GoalCmp::operator()(Symbol lhs, Symbol rhs) const {
    if (lhs == campaign_metascore)
        return true;
    else if (rhs == campaign_metascore)
        return false;
    else {
        Accomplishment *pLHSAccomplishment = m_pAccomplishmentMgr->GetAccomplishment(lhs);
        MILO_ASSERT(pLHSAccomplishment, 0x31);
        Accomplishment *pRHSAccomplishment = m_pAccomplishmentMgr->GetAccomplishment(rhs);
        MILO_ASSERT(pRHSAccomplishment, 0x34);
        return pLHSAccomplishment->mIndex < pRHSAccomplishment->mIndex;
    }
}

CampaignGoalsLeaderboardChoicePanel::CampaignGoalsLeaderboardChoicePanel()
    : mCampaignGoalsLeaderboardChoiceProvider(0) {}

Symbol CampaignGoalsLeaderboardChoicePanel::SelectedGoal() {
    if (GetState() != kUp)
        return "";
    else {
        DataNode handled = Handle(get_selected_index_msg, true);
        int handledInt = handled.Int();
        if (mCampaignGoalsLeaderboardChoiceProvider->NumData() > 0) {
            return mCampaignGoalsLeaderboardChoiceProvider->DataSymbol(handledInt);
        } else {
            return "";
        }
    }
}

Symbol CampaignGoalsLeaderboardChoiceProvider::DataSymbol(int i_iData) const {
    MILO_ASSERT_RANGE(i_iData, 0, NumData(), 0x89);
    return mEntries[i_iData];
}

void CampaignGoalsLeaderboardChoicePanel::LoadIcons() {
    LocalBandUser *pUser = TheCampaign->GetUser();
    MILO_ASSERT(pUser, 0xB8);
    BandProfile *pProfile = TheProfileMgr.GetProfileForUser(pUser);
    MILO_ASSERT(pProfile, 0xBB);
    const AccomplishmentProgress &prog = pProfile->GetAccomplishmentProgress();
    std::hash_map<Symbol, int> lbData;
    prog.InqGoalLeaderboardData(lbData);
    std::vector<Symbol> syms;
    for (std::hash_map<Symbol, int>::iterator it = lbData.begin(); it != lbData.end();
         ++it) {
        Symbol key = it->first;
        if (TheAccomplishmentMgr->GetAccomplishment(key)) {
            syms.push_back(key);
        }
    }
    std::stable_sort(syms.begin(), syms.end(), GoalAlpaCmp());
    for (std::vector<Symbol>::iterator it = syms.begin(); it != syms.end(); ++it) {
        Symbol cur = *it;
        Accomplishment *pGoal = TheAccomplishmentMgr->GetAccomplishment(cur);
        MILO_ASSERT(pGoal, 0xDA);
        AddTex(pGoal->GetIconArt(), cur.Str(), true, false);
    }
}

void CampaignGoalsLeaderboardChoicePanel::Enter() {
    UIPanel::Enter();
    MILO_ASSERT(mCampaignGoalsLeaderboardChoiceProvider, 0xE6);
    static Message cUpdateLeaderboardProviderMsg("update_leaderboard_provider", 0);
    cUpdateLeaderboardProviderMsg[0] = mCampaignGoalsLeaderboardChoiceProvider;
    Handle(cUpdateLeaderboardProviderMsg, true);
}

void CampaignGoalsLeaderboardChoicePanel::Load() {
    TexLoadPanel::Load();
    LoadIcons();
    LocalBandUser *pUser = TheCampaign->GetUser();
    MILO_ASSERT(pUser, 0xF5);
    BandProfile *pProfile = TheProfileMgr.GetProfileForUser(pUser);
    MILO_ASSERT(pProfile, 0xF8);
    MILO_ASSERT(!mCampaignGoalsLeaderboardChoiceProvider, 0xFA);
    mCampaignGoalsLeaderboardChoiceProvider =
        new CampaignGoalsLeaderboardChoiceProvider(pProfile, mTexs);
}

void CampaignGoalsLeaderboardChoicePanel::Unload() {
    TexLoadPanel::Unload();
    RELEASE(mCampaignGoalsLeaderboardChoiceProvider);
}

BEGIN_HANDLERS(CampaignGoalsLeaderboardChoicePanel)
    HANDLE_EXPR(get_selected_goal, SelectedGoal())
    HANDLE_SUPERCLASS(TexLoadPanel)
    HANDLE_CHECK(0x10C)
END_HANDLERS

RndMat *
CampaignGoalsLeaderboardChoiceProvider::Mat(int, int i_iData, UIListMesh *slot) const {
    MILO_ASSERT_RANGE(i_iData, 0, NumData(), 0x75);
    Symbol sym = DataSymbol(i_iData);
    if (slot->Matches("icon")) {
        String target(sym.Str());
        RndMat *iconMat = GetIconMat(target);
        if (iconMat)
            return iconMat;
    }
    return slot->DefaultMat();
}

void CampaignGoalsLeaderboardChoiceProvider::Text(
    int, int i_iData, UIListLabel *slot, UILabel *label
) const {
    MILO_ASSERT_RANGE(i_iData, 0, NumData(), 0x5A);
    if (slot->Matches("name")) {
        label->SetTextToken(DataSymbol(i_iData));
    } else {
        label->SetTextToken(gNullStr);
    }
}
