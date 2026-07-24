#include "decomp.h"
#include "meta/StorePackedMetadata.h"
#include "StoreMainPanel.h"
#include "BandStorePanel.h"
#include "meta_band/AppLabel.h"
#include "meta_band/BandSongMetadata.h"
#include "obj/Data.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Anim.h"
#include "rndobj/Mat.h"
#include "rndobj/Tex.h"
#include "utl/MakeString.h"
#include "utl/Std.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"

DECOMP_FORCEFUNC(StoreMainPanel, StoreMetadataManager, GetString(0))

StoreMainPanel::StoreMainPanel()
    : mConfigData(0), mTimeNextEvent(TheTaskMgr.UISeconds()), mCurrentEntry(-1),
      mDisplayRate(2.0f), mCrossfadeDuration(0.5f), unk6c(false), mNoneTex(0),
      mScrollAnim(0), mLabel1(0), mLabel2(0), mLabel3(0) {
    for (int i = 0; i < 6; i++) {
        mCoverArtMats[i] = 0;
    }
}

StoreMainPanel::~StoreMainPanel() { ClearConfigData(); }

void StoreMainPanel::Load() {
    StoreArtLoaderPanel::Load();
    BandStorePanel::Instance()->AddSink(this);
}

void StoreMainPanel::FinishLoad() {
    UIPanel::FinishLoad();
    mNoneTex = mDir->Find<RndTex>("cover_art_none.tex", true);
    for (int i = 0; i < 6; i++) {
        RndMat *mat = mDir->Find<RndMat>(MakeString("cover_art_%02i.mat", i + 1), true);
        mCoverArtMats[i] = mat;
        mat->SetDiffuseTex(mNoneTex);
        mat->MarkDirty(2);
    }
    mLabel1 = mDir->Find<AppLabel>("text_line_1.lbl", true);
    mLabel2 = mDir->Find<AppLabel>("text_line_2.lbl", true);
    mLabel3 = mDir->Find<AppLabel>("text_line_3.lbl", true);
    mScrollAnim = mDir->Find<RndAnimatable>("album_scroll.anim", true);
    mScrollAnim->Animate(
        mScrollAnim->EndFrame(), mScrollAnim->EndFrame(), kTaskUISeconds, 0, 0
    );
    mCurrentEntry = -1;
    mLabel1->SetTextToken(gNullStr);
    mLabel2->SetTextToken(gNullStr);
    MILO_ASSERT(TypeDef(), 0x57);
    static Symbol display_rate("display_rate");
    static Symbol crossfade_duration("crossfade_duration");
    mDisplayRate = TypeDef()->FindArray(display_rate, true)->Float(1);
    mCrossfadeDuration = TypeDef()->FindArray(crossfade_duration, true)->Float(1);
    ParseConfigData();
}

void StoreMainPanel::Poll() {
    StoreArtLoaderPanel::Poll();
    if (mNewReleaseList.empty())
        return;
    if (!unk6c) {
        if (IsAllArtLoadedOrFailed()) {
            MILO_ASSERT(mNewReleaseList.size() == mCoverArtTexs.size(), 0x6F);
            for (int i = 0; i < mNewReleaseList.size(); i++) {
                RndBitmap *bmp = GetBmp(mNewReleaseList[i].mStrName);
                if (bmp) {
                    mCoverArtTexs[i]->SetBitmap(*bmp, 0, 0);
                } else {
                    delete mCoverArtTexs[i];
                    mCoverArtTexs[i] = 0;
                }
            }
            unk6c = true;
            goto time_check;
        }
        return;
    }
time_check:
    if (mTimeNextEvent <= TheTaskMgr.UISeconds()) {
        int n = mNewReleaseList.size();
        mCurrentEntry = (mCurrentEntry + 1) % n;
        for (int i = 0; i < 6; i++) {
            int idx = (mCurrentEntry + i - 2);
            if (n == 0) {
                idx = 0;
            } else {
                idx = idx % n;
                if (idx < 0)
                    idx += n;
            }
            if (i < 2) {
                RndTex *tex = mCoverArtMats[i + 1]->GetDiffuseTex();
                if (tex) {
                    mCoverArtMats[i]->SetDiffuseTex(tex);
                } else {
                    mCoverArtMats[i]->SetDiffuseTex(mNoneTex);
                }
            } else {
                RndTex *tex = mCoverArtTexs[idx];
                if (tex) {
                    mCoverArtMats[i]->SetDiffuseTex(tex);
                } else {
                    mCoverArtMats[i]->SetDiffuseTex(mNoneTex);
                }
            }
        }
        mScrollAnim->Animate(
            mScrollAnim->StartFrame(), mScrollAnim->EndFrame(), kTaskUISeconds,
            mCrossfadeDuration, 0
        );
        mLabel1->SetNewReleaseEntryText1(this);
        mLabel2->SetNewReleaseEntryText2(this);
        mLabel3->SetNewReleaseEntryText3(this);
        mTimeNextEvent = mDisplayRate + (mCrossfadeDuration + TheTaskMgr.UISeconds());
    }
}

void StoreMainPanel::Unload() {
    for (int i = 0; i < 6; i++) {
        mCoverArtMats[i] = 0;
    }
    mNoneTex = 0;
    mScrollAnim = 0;
    mLabel1 = 0;
    mLabel2 = 0;
    if (mConfigData) {
        mConfigData->Release();
        mConfigData = 0;
    }
    BandStorePanel::Instance()->RemoveSink(this);
    ClearConfigData();
    StoreArtLoaderPanel::Unload();
}

DataNode StoreMainPanel::OnMsg(const MetadataLoadedMsg &msg) {
    if (!msg->Int(3) || !msg->Int(5) || mNewReleaseList.size() != 0)
        return DataNode(1);
    MILO_ASSERT_FMT(
        msg->Array(2),
        "NULL data array passed to StoreMainPanel::SetConfigData()\n"
    );
    mConfigData = msg->Array(2);
    ParseConfigData();
    return DataNode(1);
}

void StoreMainPanel::ParseConfigData() {
    StoreMarqueeTable *table = TheStoreMetadata.mMarqueeTable;
    mNewReleaseList.reserve(table->mNumMarquees);
    NewReleaseEntry entry;
    for (int i = 0; i < TheStoreMetadata.mMarqueeTable->mNumMarquees; i++) {
        unsigned short *marquee =
            (unsigned short *)(TheStoreMetadata.mMarqueeTable->mMarquees + i * 0xA);
        entry.mStrName = BandStorePanel::Instance()->GetRequestPrefix();
        entry.mStrName += TheStoreMetadata.GetString(marquee[1]);
        entry.mText1 = TheStoreMetadata.GetString(marquee[2]);
        entry.mText2 = TheStoreMetadata.GetString(marquee[3]);
        entry.mText3 = TheStoreMetadata.GetString(marquee[0]);
        entry.mText4 = MakeString("%d", (int)marquee[4]);
        EnsureArtLoader(entry.mStrName);
        mNewReleaseList.push_back(entry);
        mCoverArtTexs.push_back(Hmx::Object::New<RndTex>());
    }
    mTimeNextEvent = 0;
    mCurrentEntry = -1;
}

void StoreMainPanel::ClearConfigData() {
    unk6c = false;
    DeleteAll(mCoverArtTexs);
    mCoverArtTexs.resize(0);
    ClearAndShrink(mNewReleaseList);
}

const StoreMainPanel::NewReleaseEntry *StoreMainPanel::CurrentEntry() const {
    MILO_ASSERT(mCurrentEntry < mNewReleaseList.size(), 0x134);
    return &mNewReleaseList[mCurrentEntry];
}

const char *StoreMainPanel::MarqueePath() const {
    if (mNewReleaseList.size() && mCurrentEntry >= 0) {
        return CurrentEntry()->mText4.c_str();
    }
    return gNullStr;
}

BEGIN_HANDLERS(StoreMainPanel)
    HANDLE_EXPR(is_waiting_on_enum, mConfigData == 0)
    HANDLE_EXPR(marquee_path, MarqueePath())
    HANDLE_MESSAGE(MetadataLoadedMsg)
    HANDLE_SUPERCLASS(StoreArtLoaderPanel)
    HANDLE_CHECK(0x14B)
END_HANDLERS

#pragma pool_data off
void StoreMainPanel::SetType(Symbol type) {
    static DataArray *types = SystemConfig(StaticClassName(), "types", "objects");
    if (type.Null()) {
        SetTypeDef(0);
    } else {
        DataArray *found = types->FindArray(type, false);
        if (found != 0) {
            SetTypeDef(found);
        } else {
            MILO_NOTIFY(
                "%s:%s couldn't find type %s", ClassName(), PathName(this), type
            );
            SetTypeDef(0);
        }
    }
}
#pragma pool_data reset
