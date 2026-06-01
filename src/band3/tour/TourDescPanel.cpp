#include "tour/Tour.h"
#include "tour/TourDesc.h"
#include "tour/TourProgress.h"
#include "meta_band/Accomplishment.h"
#include "meta_band/AccomplishmentManager.h"
#include "meta_band/AccomplishmentProgress.h"
#include "meta_band/BandProfile.h"
#include "meta_band/Campaign.h"
#include "meta_band/CampaignLevel.h"
#include "game/BandUser.h"
#include "meta_band/MetaPanel.h"
#include "meta_band/SessionMgr.h"
#include "meta_band/TexLoadPanel.h"
#include "meta_band/BandMachineMgr.h"
#include "bandobj/StarDisplay.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "rndobj/Mesh.h"
#include "stl/_algo.h"
#include "ui/PanelDir.h"
#include "ui/UIList.h"
#include "ui/UIListLabel.h"
#include "ui/UIListMesh.h"
#include "ui/UIListProvider.h"
#include "ui/UIPicture.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/Messages.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"
#include "utl/VectorSizeDefs.h"
#include <vector>

#ifndef HX_NATIVE
// STLport-internal _Temporary_buffer<Symbol*> specialization that routes the
// std::sort scratch buffer through Milo's _MemAlloc. Host STL has no
// stlpmtx_std::_Temporary_buffer; native std::sort uses the host allocator.
namespace stlpmtx_std {
template <>
inline void _Temporary_buffer<Symbol *, Symbol>::_M_allocate_buffer() {
    _M_original_len = _M_len;
    _M_buffer = 0;
    if (_M_len > (ptrdiff_t)(INT_MAX / sizeof(Symbol)))
        _M_len = INT_MAX / sizeof(Symbol);
    while (_M_len > 0) {
        _M_buffer = (Symbol *)_MemAlloc(_M_len * sizeof(Symbol), 0);
        if (_M_buffer)
            break;
        _M_len /= 2;
    }
}

template <>
inline _Temporary_buffer<Symbol *, Symbol>::~_Temporary_buffer() {
    _STLP_STD::_Destroy_Range(_M_buffer, _M_buffer + _M_len);
    _MemFree(_M_buffer);
}
} // namespace stlpmtx_std
#endif

class TourDescCmp {
public:
    TourDescCmp(const Tour *tour);
    bool operator()(Symbol s1, Symbol s2) const;
    const Tour *mTour; // 0x0
};

TourDescCmp::TourDescCmp(const Tour *tour) : mTour(tour) {}

bool TourDescCmp::operator()(Symbol s1, Symbol s2) const {
    TourDesc *pLHSTourDesc = mTour->GetTourDesc(s1);
    MILO_ASSERT(pLHSTourDesc, 0x2D);
    TourDesc *pRHSTourDesc = mTour->GetTourDesc(s2);
    MILO_ASSERT(pRHSTourDesc, 0x30);
    return pLHSTourDesc->GetIndex() < pRHSTourDesc->GetIndex();
}

class TourDescProvider : public UIListProvider, public Hmx::Object {
public:
    TourDescProvider(std::vector<DynamicTex *> *texs)
        : mTexs(texs), mUnearnedMat(0), mEarnedMat(0) {}
    virtual ~TourDescProvider();
    virtual void Text(int, int, UIListLabel *, UILabel *) const;
    virtual RndMat *Mat(int, int, UIListMesh *) const;
    virtual void UpdateExtendedText(int, int, UILabel *) const;
    virtual void UpdateExtendedMesh(int, int, RndMesh *) const;
    virtual void UpdateExtendedCustom(int, int, Hmx::Object *) const;
    virtual Symbol DataSymbol(int) const;
    virtual int NumData() const;
    virtual void InitData(RndDir *);
    virtual UIComponent::State
    ComponentStateOverride(int, int, UIComponent::State) const;

    inline bool IsTourDescAvailable(Symbol) const;
    inline void UpdateList();

    std::vector<DynamicTex *> *mTexs; // 0x20
    RndMat *mUnearnedMat; // 0x24
    RndMat *mEarnedMat; // 0x28
    std::vector<Symbol VECTOR_SIZE_SMALL> mTours; // 0x2c
};

inline bool TourDescProvider::IsTourDescAvailable(Symbol s) const {
    TourDesc *pTourDesc = TheTour->GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x226);
    MILO_ASSERT(TheTour->GetTourProgress(), 0x229);
    if (pTourDesc->HasRequiredCampaignLevel()) {
        Symbol level = pTourDesc->GetRequiredCampaignLevel();
        BandMachineMgr *pMachineMgr = TheSessionMgr->GetMachineMgr();
        MILO_ASSERT(pMachineMgr, 0x230);
        int score = pMachineMgr->GetLeaderPrimaryMetaScore();
        if (!TheCampaign->HasScoreReachedCampaignLevel(score, level))
            return false;
    }
    return true;
}

class TourDescProvider;

class TourDescPanel : public TexLoadPanel {
public:
    TourDescPanel();
    OBJ_CLASSNAME(TourDescPanel);
    OBJ_SET_TYPE(TourDescPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~TourDescPanel();
    virtual void Load();
    virtual void FinishLoad();
    virtual void Enter();
    virtual void Unload();

    Symbol GetSelectedTourDesc(class UIComponent *);
    void LoadIcons();
    void Refresh();
    bool IsTourAvailable();
    Symbol GetInitiallySelectedTour();
    void ClearInitiallySelectedTour();
    void SelectDefaultTour();
    void SelectTour(Symbol);
    void CheatWinTour();
    TourDescProvider *m_pTourDescProvider; // 0x4c
};

inline void TourDescProvider::UpdateList() {
    MILO_ASSERT(TheTour, 0x49);
    mTours.clear();
    for (std::map<Symbol, TourDesc *>::const_iterator it =
             TheTour->m_mapTourDesc.begin();
         it != TheTour->m_mapTourDesc.end();
         ++it) {
        mTours.push_back(it->first);
    }
}

void TourDescPanel::Refresh() {
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x2C7);
    MILO_ASSERT(m_pTourDescProvider, 0x2C9);
    m_pTourDescProvider->UpdateList();
    TourDescCmp cmp(TheTour);
    std::stable_sort(
        m_pTourDescProvider->mTours.begin(),
        m_pTourDescProvider->mTours.end(),
        cmp
    );
    static Message cUpdateFilterProviderMsg("update_tourdesc_provider", 0);
    cUpdateFilterProviderMsg[0] = m_pTourDescProvider;
    Handle(cUpdateFilterProviderMsg, true);
    SelectDefaultTour();
}

void TourDescPanel::LoadIcons() {
    AddTex(
        "ui/accomplishments/accomplishment_art/acc_unearned_award_bronze_keep.bmp",
        "tourprize_bronze",
        true,
        false
    );
    AddTex(
        "ui/accomplishments/accomplishment_art/acc_unearned_award_silver_keep.bmp",
        "tourprize_silver",
        true,
        false
    );
    AddTex(
        "ui/accomplishments/accomplishment_art/acc_unearned_award_gold_keep.bmp",
        "tourprize_gold",
        true,
        false
    );
    {
        std::map<Symbol, TourDesc *>::const_iterator end =
            TheTour->m_mapTourDesc.end();
        for (std::map<Symbol, TourDesc *>::const_iterator it =
                 TheTour->m_mapTourDesc.begin();
             it != end;
             ++it) {
            TourDesc *pTourDesc = it->second;
            Symbol s = (*it).first;
            MILO_ASSERT(pTourDesc, 0x2A7);
            AddTex(pTourDesc->GetArt(), s.Str(), true, false);
            const char * _tmp0 = MakeString("%s_gray", s.Str());
            AddTex(pTourDesc->GetGrayArt(), _tmp0, true, false);
        }
    }
    {
        std::map<Symbol, CampaignLevel *>::const_iterator end =
            TheCampaign->m_mapCampaignLevels.end();
        for (std::map<Symbol, CampaignLevel *>::const_iterator it =
                 TheCampaign->m_mapCampaignLevels.begin();
             it != end;
             ++it) {
            CampaignLevel *pLevel = (*it).second;
            Symbol s = (*it).first;
            MILO_ASSERT(pLevel, 0x2B9);
            String iconArt = pLevel->GetIconArt();
            if (pLevel->IsMajorLevel())
                AddTex(iconArt.c_str(), s.Str(), true, false);
        }
    }
}
void TourDescProvider::UpdateExtendedMesh(int, int iData, RndMesh *i_pMesh) const {
    MILO_ASSERT(iData < NumData(), 0x14C);
    Symbol s = DataSymbol(iData);
    TourDesc *pTourDesc = TheTour->GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x150);
    bool bAvailable;
    if (MetaPanel::sUnlockAll) {
        bAvailable = true;
    } else {
        bAvailable = bool(IsTourDescAvailable(s));
    }
    if (!strcmp(i_pMesh->Name(), "tour_art.mesh")) {
        String texName(bAvailable ? pTourDesc->GetName()
                                  : MakeString("%s_gray", pTourDesc->GetName()));
        std::vector<DynamicTex *>::iterator it =
            std::find(mTexs->begin(), mTexs->end(), texName);
        if (it != mTexs->end())
            i_pMesh->SetMat((*it)->mMat);
        else
            i_pMesh->SetMat(0);
    }
}

void TourDescProvider::UpdateExtendedCustom(int, int iData, Hmx::Object *i_pObj) const {
    Symbol s = DataSymbol(iData);
    TourDesc *pTourDesc = TheTour->GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x168);
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x16B);
    const char *pName = i_pObj->Name();
    if (strcmp(pName, "total.sd") == 0) {
        StarDisplay *pStarDisplay = dynamic_cast<StarDisplay *>(i_pObj);
        MILO_ASSERT(pStarDisplay, 0x170);
        if (pProgress->GetToursPlayed(s) == 0)
            pProgress->GetTourMostStars(s);
        int iStars = pProgress->GetTourMostStars(s);
        pStarDisplay->SetValues(iStars, iStars);
        pStarDisplay->SetShowDenominator(false);
        pStarDisplay->SetForceMixedMode(true);
    } else if (TheTour && strcmp(pName, "bronze.pic") == 0) {
        UIPicture *pPicture = dynamic_cast<UIPicture *>(i_pObj);
        MILO_ASSERT(pPicture, 0x184);
        if (TheTour->HasBronzeMedal(s)) {
            Accomplishment *pGoal =
                TheAccomplishmentMgr->GetAccomplishment(pTourDesc->GetTourBronzeGoal());
            MILO_ASSERT(pGoal, 0x18B);
            pPicture->SetTex(FilePath(pGoal->GetIconArt()));
        } else {
            pPicture->SetTex(FilePath(""));
        }
    } else if (strcmp(pName, "bronze_unearned.pic") == 0) {
        UIPicture *pPicture = dynamic_cast<UIPicture *>(i_pObj);
        MILO_ASSERT(pPicture, 0x197);
        if (!TheTour->HasBronzeMedal(s)) {
            Accomplishment *pGoal =
                TheAccomplishmentMgr->GetAccomplishment(pTourDesc->GetTourBronzeGoal());
            MILO_ASSERT(pGoal, 0x19E);
            pPicture->SetTex(FilePath(pGoal->GetIconArt()));
        } else {
            pPicture->SetTex(FilePath(""));
        }
    } else if (strcmp(pName, "silver.pic") == 0) {
        UIPicture *pPicture = dynamic_cast<UIPicture *>(i_pObj);
        MILO_ASSERT(pPicture, 0x1AA);
        if (TheTour->HasSilverMedal(s)) {
            Accomplishment *pGoal =
                TheAccomplishmentMgr->GetAccomplishment(pTourDesc->GetTourSilverGoal());
            MILO_ASSERT(pGoal, 0x1B1);
            pPicture->SetTex(FilePath(pGoal->GetIconArt()));
        } else {
            pPicture->SetTex(FilePath(""));
        }
    } else if (strcmp(pName, "silver_unearned.pic") == 0) {
        UIPicture *pPicture = dynamic_cast<UIPicture *>(i_pObj);
        MILO_ASSERT(pPicture, 0x1BD);
        if (!TheTour->HasSilverMedal(s)) {
            Accomplishment *pGoal =
                TheAccomplishmentMgr->GetAccomplishment(pTourDesc->GetTourSilverGoal());
            MILO_ASSERT(pGoal, 0x1C4);
            pPicture->SetTex(FilePath(pGoal->GetIconArt()));
        } else {
            pPicture->SetTex(FilePath(""));
        }
    } else if (strcmp(pName, "gold.pic") == 0) {
        UIPicture *pPicture = dynamic_cast<UIPicture *>(i_pObj);
        MILO_ASSERT(pPicture, 0x1D0);
        if (TheTour->HasGoldMedal(s)) {
            Accomplishment *pGoal =
                TheAccomplishmentMgr->GetAccomplishment(pTourDesc->GetTourGoldGoal());
            MILO_ASSERT(pGoal, 0x1D7);
            pPicture->SetTex(FilePath(pGoal->GetIconArt()));
        } else {
            pPicture->SetTex(FilePath(""));
        }
    } else if (strcmp(pName, "gold_unearned.pic") == 0) {
        UIPicture *pPicture = dynamic_cast<UIPicture *>(i_pObj);
        MILO_ASSERT(pPicture, 0x1E3);
        if (!TheTour->HasGoldMedal(s)) {
            Accomplishment *pGoal =
                TheAccomplishmentMgr->GetAccomplishment(pTourDesc->GetTourGoldGoal());
            MILO_ASSERT(pGoal, 0x1EA);
            pPicture->SetTex(FilePath(pGoal->GetIconArt()));
        } else {
            pPicture->SetTex(FilePath(""));
        }
    } else if (strcmp(pName, "bronze_prize_overlay.mesh") == 0) {
        RndMesh *pMesh = dynamic_cast<RndMesh *>(i_pObj);
        MILO_ASSERT(pMesh, 0x1F6);
        pMesh->SetShowing(TheTour->HasBronzeMedal(s));
    } else if (strcmp(pName, "silver_prize_overlay.mesh") == 0) {
        RndMesh *pMesh = dynamic_cast<RndMesh *>(i_pObj);
        MILO_ASSERT(pMesh, 0x1FD);
        pMesh->SetShowing(TheTour->HasSilverMedal(s));
    } else if (strcmp(pName, "gold_prize_overlay.mesh") == 0) {
        RndMesh *pMesh = dynamic_cast<RndMesh *>(i_pObj);
        MILO_ASSERT(pMesh, 0x204);
        pMesh->SetShowing(TheTour->HasGoldMedal(s));
    }
}

void TourDescProvider::UpdateExtendedText(int, int iData, UILabel *i_pLabel) const {
    MILO_ASSERT(iData < NumData(), 0x100);
    Symbol s = DataSymbol(iData);
    TourDesc *pTourDesc = TheTour->GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x104);
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x109);
    bool bPlayed = pProgress->GetToursPlayed(s) != 0
        || pProgress->GetTourMostStars(s) > 0;
    const char *pName = i_pLabel->Name();
    if (strcmp(pName, "tour_bronze.lbl") == 0) {
        i_pLabel->SetTokenFmt(
            tourdesc_stars_needed, pTourDesc->GetTourStarsBronzeGoalValue()
        );
    } else if (strcmp(pName, "tour_silver.lbl") == 0) {
        i_pLabel->SetTokenFmt(
            tourdesc_stars_needed, pTourDesc->GetTourStarsSilverGoalValue()
        );
    } else if (strcmp(pName, "tour_gold.lbl") == 0) {
        i_pLabel->SetTokenFmt(
            tourdesc_stars_needed, pTourDesc->GetTourStarsGoldGoalValue()
        );
    } else if (strcmp(pName, "tour_desc.lbl") == 0) {
        bool bAvailable = MetaPanel::sUnlockAll ? true : IsTourDescAvailable(s);
        if (bAvailable) {
            i_pLabel->SetTextToken(pTourDesc->GetDescription());
        } else {
            i_pLabel->SetTextToken(Symbol(gNullStr));
        }
    } else if (strcmp(pName, "total.lbl") == 0) {
        if (bPlayed) {
            i_pLabel->SetTextToken(tour_gig_total);
        } else {
            i_pLabel->SetTextToken(Symbol(gNullStr));
        }
    } else if (strcmp(pName, "tour_unlockinfo.lbl") == 0) {
        bool bAvailable = MetaPanel::sUnlockAll ? true : IsTourDescAvailable(s);
        if (bAvailable) {
            i_pLabel->SetTextToken(Symbol(gNullStr));
        } else {
            CampaignLevel *pCampaignLevel =
                TheCampaign->GetCampaignLevel(pTourDesc->GetRequiredCampaignLevel());
            MILO_ASSERT(pCampaignLevel, 0x140);
            i_pLabel->SetTextToken(pCampaignLevel->GetRequirementToken());
        }
    } else {
        i_pLabel->SetTextToken(Symbol(gNullStr));
    }
}

RndMat *TourDescProvider::Mat(int, int iData, UIListMesh *i_pSlot) const {
    MILO_ASSERT(iData < NumData(), 0xAD);
    Symbol s = DataSymbol(iData);
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0xB2);
    bool bSelected = pProgress->GetTourDesc() == s;
    bool bPlayed = pProgress->GetToursPlayed(s) > 0;
    bPlayed = bPlayed || pProgress->GetTourMostStars(s) > 0;
    (void)bPlayed;
    if (i_pSlot->Matches("bronze")) {
        if (!bSelected) {
            if (TheTour->HasBronzeMedal(s)) {
                String str("tourprize_bronze");
                std::vector<DynamicTex *>::iterator it =
                    std::find(mTexs->begin(), mTexs->end(), str);
                RndMat *pMat = (it != mTexs->end()) ? (*it)->mMat : (RndMat *)0;
                return pMat;
            }
        }
        return 0;
    }
    if (i_pSlot->Matches("silver")) {
        if (!bSelected) {
            if (TheTour->HasSilverMedal(s)) {
                String str("tourprize_silver");
                std::vector<DynamicTex *>::iterator it =
                    std::find(mTexs->begin(), mTexs->end(), str);
                RndMat *pMat = (it != mTexs->end()) ? (*it)->mMat : (RndMat *)0;
                return pMat;
            }
        }
        return 0;
    }
    if (i_pSlot->Matches("gold")) {
        if (!bSelected) {
            if (TheTour->HasGoldMedal(s)) {
                String str("tourprize_gold");
                std::vector<DynamicTex *>::iterator it =
                    std::find(mTexs->begin(), mTexs->end(), str);
                RndMat *pMat = (it != mTexs->end()) ? (*it)->mMat : (RndMat *)0;
                return pMat;
            }
        }
        return 0;
    }
    if (i_pSlot->Matches("bg")) {
        if (iData % 2 != 0)
            return mUnearnedMat;
        return mEarnedMat;
    }
    if (i_pSlot->Matches("vehicle")) {
        TourDesc *pTourDesc = TheTour->GetTourDesc(s);
        MILO_ASSERT(pTourDesc, 0xF3);
        String str(pTourDesc->GetRequiredCampaignLevel().Str());
        std::vector<DynamicTex *>::iterator it =
            std::find(mTexs->begin(), mTexs->end(), str);
        RndMat *pMat = (it != mTexs->end()) ? (*it)->mMat : (RndMat *)0;
        return pMat;
    }
    return i_pSlot->DefaultMat();
}

void TourDescProvider::Text(
    int, int iData, UIListLabel *i_pSlot, UILabel *i_pLabel
) const {
    MILO_ASSERT(iData < NumData(), 0x5C);
    Symbol s = DataSymbol(iData);
    TourDesc *pTourDesc = TheTour->GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x60);
    TourProgress *pProgress = TheTour->GetTourProgress();
    MILO_ASSERT(pProgress, 0x63);
        bool bAvailable;
    bool bSelected = pProgress->GetTourDesc() == s;
    if (MetaPanel::sUnlockAll) {
        bAvailable = true;
    } else {
        bAvailable = IsTourDescAvailable(s);
    }
    if (i_pSlot->Matches("name")) {
        if (bAvailable)
            i_pLabel->SetTextToken(s);
        else
            i_pLabel->SetTextToken(Symbol(gNullStr));
    } else if (i_pSlot->Matches("inprogress")) {
        if (bAvailable && bSelected)
            i_pLabel->SetTextToken(tour_inprogress);
        else
            i_pLabel->SetTextToken(Symbol(gNullStr));
    } else if (i_pSlot->Matches("locked")) {
        if (!bAvailable)
            i_pLabel->SetTextToken(tour_locked);
        else
            i_pLabel->SetTextToken(Symbol(gNullStr));
    } else if (i_pSlot->Matches("numsongs")) {
        if (bAvailable && !bSelected) {
            i_pLabel->SetTokenFmt(tour_desc_songcount, pTourDesc->GetNumSongs());
        } else {
            i_pLabel->SetTextToken(Symbol(gNullStr));
        }
    } else {
        i_pLabel->SetTextToken(Symbol(i_pSlot->GetDefaultText()));
    }
}

void TourDescPanel::SelectTour(Symbol s) {
    int index = 0;
    if (s != "") {
        index = 0;
        std::vector<Symbol> &tours = m_pTourDescProvider->mTours;
        std::vector<Symbol>::iterator it = tours.begin();
        for (; it != tours.end(); ++it, ++index) {
            if (*it != s)
                continue;
            goto lFound;
        }
        index = 0;
        lFound:;
    }
    UIList *pList = mDir->Find<UIList>("pTourList", true);
    MILO_ASSERT(pList, 0x30B);
    pList->SetSelected(index, -1);
}

UIComponent::State TourDescProvider::ComponentStateOverride(
    int iCol, int iData, UIComponent::State i_eState
) const {
    Symbol s = DataSymbol(iData);
    bool bAvailable = MetaPanel::sUnlockAll ? true : IsTourDescAvailable(s);
    if (!bAvailable)
        i_eState = UIComponent::kDisabled;
    return i_eState;
}

TourDescProvider::~TourDescProvider() {}

inline int TourDescProvider::NumData() const { return mTours.size(); }

inline Symbol TourDescProvider::DataSymbol(int i_iData) const {
    MILO_ASSERT(0 <= i_iData && i_iData < NumData(), 0x24A);
    return mTours[i_iData];
}

inline void TourDescProvider::InitData(RndDir *i_pDir) {
    mUnearnedMat = i_pDir->Find<RndMat>("song_disc_dark.mat", false);
    mEarnedMat = i_pDir->Find<RndMat>("song_disc_light.mat", false);
}

bool TourDescPanel::IsTourAvailable() {
    MILO_ASSERT(m_pTourDescProvider, 0x2D6);
    Symbol s = GetSelectedTourDesc(0);
    return MetaPanel::sUnlockAll ? true : m_pTourDescProvider->IsTourDescAvailable(s);
}

void TourDescPanel::CheatWinTour() {
    Symbol s = GetSelectedTourDesc(0);
    TourDesc *pTourDesc = TheTour->GetTourDesc(s);
    MILO_ASSERT(pTourDesc, 0x318);
    LocalBandUser *pUser = TheTour->GetUser();
    MILO_ASSERT(pUser, 0x31B);
    BandProfile *pProfile = TheTour->GetProfile();
    MILO_ASSERT(pProfile, 0x31E);
    AccomplishmentProgress &progress = pProfile->AccessAccomplishmentProgress();
    int iPlayed = progress.GetToursPlayed(s) + 1;
    progress.SetToursPlayed(s, iPlayed);
    int iStarsPossible = pTourDesc->GetNumStarsPossibleForTour();
    progress.SetMostStars(s, iStarsPossible);
    int iGotAllStars = progress.GetToursGotAllStars(s) + 1;
    progress.SetToursGotAllStars(s, iGotAllStars);
    TheAccomplishmentMgr->CheckForFinishedTourAccomplishmentsForUser(pUser);
    Refresh();
}


TourDescPanel::TourDescPanel() : m_pTourDescProvider(0) {}

TourDescPanel::~TourDescPanel() {}

void TourDescPanel::Load() {
    TexLoadPanel::Load();
    MILO_ASSERT(!m_pTourDescProvider, 0x279);
    LoadIcons();
}

void TourDescPanel::FinishLoad() {
    TexLoadPanel::FinishLoad();
    MILO_ASSERT(!m_pTourDescProvider, 0x283);
    m_pTourDescProvider = new TourDescProvider(&mTexs);
}

void TourDescPanel::Enter() {
    UIPanel::Enter();
    Refresh();
}

void TourDescPanel::Unload() {
    TexLoadPanel::Unload();
    delete m_pTourDescProvider;
    m_pTourDescProvider = 0;
}

Symbol TourDescPanel::GetSelectedTourDesc(UIComponent *) {
    if (GetState() != kUp)
        return Symbol("");
    DataNode handled = Handle(get_selected_tourdesc_index_msg, true);
    int index = handled.Int();
    if (m_pTourDescProvider->NumData() > 0)
        return m_pTourDescProvider->DataSymbol(index);
    return Symbol("");
}

Symbol TourDescPanel::GetInitiallySelectedTour() {
    return Handle(get_initially_selected_tour_msg, true).Sym();
}

void TourDescPanel::ClearInitiallySelectedTour() {
    Handle(clear_initially_selected_tour_msg, true);
}

void TourDescPanel::SelectDefaultTour() {
    Symbol s = GetInitiallySelectedTour();
    ClearInitiallySelectedTour();
    if (s == gNullStr) {
        TourProgress *pProgress = TheTour->GetTourProgress();
        MILO_ASSERT(pProgress, 0x2F8);
        s = pProgress->GetTourDesc();
    }
    SelectTour(s);
}

BEGIN_HANDLERS(TourDescPanel)
    HANDLE_ACTION(refresh, Refresh())
    HANDLE_EXPR(get_selected_tour, GetSelectedTourDesc(0))
    HANDLE_EXPR(is_tour_available, IsTourAvailable())
    HANDLE_ACTION(cheat_win_tour, CheatWinTour())
    HANDLE_SUPERCLASS(TexLoadPanel)
    HANDLE_CHECK(0x345)
END_HANDLERS
