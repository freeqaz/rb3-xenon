#include "meta_band/EyebrowsProvider.h"
#include "meta_band/AssetMgr.h"
#include "meta_band/TexLoadPanel.h"
#include "os/Debug.h"
#include "ui/UIListMesh.h"
#include "utl/Symbol.h"
#include "utl/Symbols4.h"

EyebrowsProvider::EyebrowsProvider(const std::vector<DynamicTex *> &vec)
    : mIcons(vec), mGender(gNullStr) {}

void EyebrowsProvider::Update(Symbol s) {
    mGender = s;
    mEyebrows.clear();
    AssetMgr *pAssetMgr = AssetMgr::GetAssetMgr();
    MILO_ASSERT(pAssetMgr, 0x1C);
    pAssetMgr->GetEyebrows(mEyebrows, mGender);
    // Retail spells this as a function-local static (Symbol at 0x82E02004 with
    // its guard word at 0x82E02008), not the Symbols4.h file-scope global the
    // Wii oracle uses. Same conversion already applied in AssetMgr.cpp and
    // AssetTypes.cpp. Declared at point of use: the retail guard test sits
    // immediately before the push_back.
    static Symbol none_eyebrows("none_eyebrows");
    mEyebrows.push_back(none_eyebrows);
}

RndMat *EyebrowsProvider::Mat(int, int data, UIListMesh *mesh) const {
    MILO_ASSERT(data < NumData(), 0x26);
    if (mesh->Matches("icon")) {
        String str(MakeString("%s_eyebrows_%d", mGender.Str(), data));
        std::vector<DynamicTex *>::const_iterator it =
            std::find(mIcons.begin(), mIcons.end(), str);
        if (it != mIcons.end())
            return (*it)->mMat;
    }
    return 0;
}

Symbol EyebrowsProvider::DataSymbol(int idx) const {
    int data = NumData() - 1;
    data = Clamp(0, data, idx);
    MILO_ASSERT_RANGE(data, 0, NumData(), 0x41);
    return mEyebrows[data];
}

int EyebrowsProvider::NumData() const { return mEyebrows.size(); }
