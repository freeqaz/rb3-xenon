#include "meta_band/StoreMenuProvider.h"
#include "math/Rand.h"
#include "meta_band/AppLabel.h"
#include "obj/Data.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "ui/UIListLabel.h"
#include "ui/UILabel.h"
#include "utl/Locale.h"
#include "utl/MakeString.h"
#include "utl/Str.h"
#include "utl/Symbol.h"

int StoreMenuProvider::NumData() const { return mSubmenus->Size() - 1; }

bool StoreMenuProvider::IsActive(int i) const {
    DataArray *entry = mSubmenus->Array(i + 1);
    if (entry->Size() > 2)
        return entry->Int(2) > 0;
    return true;
}

bool StoreMenuProvider::IsRandomSelect(DataArray *entry) const {
    static Symbol random_song("random_song");
    if (entry->Size() != 4)
        return false;
    const DataNode &n = entry->Node(3);
    return n.Type() == kDataSymbol && random_song == n.Sym(NULL);
}

StoreMenuProvider::~StoreMenuProvider() {
    if (mData) {
        mData->Release();
        mData = NULL;
    }
}

void StoreMenuProvider::SetData(DataArray *data) {
    mIxHighlight = 0;
    if (mData) {
        mData->Release();
        mData = NULL;
    }
    mData = data;
    data->AddRef();
    mSubmenus = mData->FindArray("submenus", true);
    mBannerData = mData->FindArray("new_releases", false);
}

void StoreMenuProvider::Text(int, int i, UIListLabel *slot, UILabel *label) const {
    DataArray *entry = mSubmenus->Array(i + 1);
    if (slot->Matches("filter")) {
        AppLabel *pAppLabel = dynamic_cast<AppLabel *>(label);
        pAppLabel->SetStoreMenuText(entry->Node(1));
    } else if (slot->Matches("count") && !IsRandomSelect(entry) && entry->Size() > 2) {
        label->SetInt(entry->Int(2), true);
    } else {
        label->SetTextToken(gNullStr);
    }
}

const char *StoreMenuProvider::GetTitle() {
    DataArray *info = mData->FindArray("index_info", false);
    if (info) {
        DataArray *title = info->FindArray("title", false);
        if (title) {
            DataNode node(title->Node(1));
            if (node.Type() == kDataString) {
                return node.Str(NULL);
            }
            return Localize(node.Sym(NULL), NULL);
        }
    }
    return gNullStr;
}

const char *StoreMenuProvider::GetFileName(int i) {
    DataArray *entry = mSubmenus->Array(i + 1);
    bool slash = *entry->Str(0) == '/';
    const char *ret;
    if (IsRandomSelect(entry)) {
        int r = RandomInt(0, entry->Int(2));
        String s;
        if (slash) {
            s = entry->Str(0);
        } else {
            s = mPath + entry->Str(0);
        }
        ret = MakeString(s.c_str(), r);
    } else if (slash) {
        ret = entry->Str(0);
    } else {
        ret = MakeString("%s%s", mPath.c_str(), entry->Str(0));
    }
    return ret;
}

BEGIN_HANDLERS(StoreMenuProvider)
    HANDLE_EXPR(get_highlight_ix, 0)
    HANDLE_ACTION(set_highlight_ix, mIxHighlight = _msg->Int(2))
    HANDLE_EXPR(get_string, GetFileName(_msg->Int(2)))
    HANDLE_EXPR(has_banner_data, NULL != mBannerData)
    HANDLE_EXPR(get_banner_data, mBannerData)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x6C)
END_HANDLERS

StoreMenuProvider::StoreMenuProvider(DataArray *data, const char *path)
    : mPath(path), mData(NULL), mSubmenus(NULL), mBannerData(NULL) {
    SetData(data);
}
