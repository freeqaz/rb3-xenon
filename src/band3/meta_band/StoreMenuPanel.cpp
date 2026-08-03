#include "macros.h"
// ObjMacros.h must precede StoreMenuPanel.h so the class body expands the
// ObjMacros OBJ_SET_TYPE (arg-evaluating stripped-warn arm) instead of the
// Object.h fallback — retail SetType keeps the PathName/ClassName arg
// evaluation (same fix as TrainingPanel, whose header chain pulls ObjMacros
// first via LessonProvider.h).
#include "obj/ObjMacros.h"
#include "meta_band/StoreMenuPanel.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandStorePanel.h"
#include "meta_band/StoreMenuProvider.h"
#include "bandobj/BandList.h"
#include "meta/StorePackedMetadata.h"
#include "obj/Data.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "ui/UI.h"
#include "ui/UIList.h"
#include "utl/MakeString.h"
#include "utl/Messages2.h"
#include "utl/Std.h"
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

StoreMenuPanel::StoreMenuPanel()
    : mMenuStack(), mCurrentMenuIx(0), mPendingMenuIx(-1), mList(0),
      mStartingHighlightIx(0) {}

void StoreMenuPanel::SetPendingMenuIx(int ix) {
    mPendingMenuIx = ix;
    if (mList)
        mList->Conceal();
}

void StoreMenuPanel::FinishLoad() {
    UIPanel::FinishLoad();
    const DataArray *t = TypeDef();
    static Symbol menu_list("menu_list");
    Symbol tag = menu_list;
    const char *name = t->FindArray(tag, true)->Str(1);
    ObjectDir *dir = mDir;
    mList = dir->Find<BandList>(name, true);
}

void StoreMenuPanel::Unload() {
    mCurrentMenuIx = 0;
    DeleteAll(mMenuStack);
    mList = nullptr;
    UIPanel::Unload();
}

void StoreMenuPanel::Enter() {
    BandStorePanel *storePanel = BandStorePanel::Instance();
    UIPanel::Enter();
    storePanel->AddSink(this);
    if (mCurrentMenuIx == -1) {
        mStartingHighlightIx = 0;
        mPendingMenuIx = -1;
        storePanel->Request(String(storePanel->GetIndexFile()), true);
    }
}

void StoreMenuPanel::Exit() {
    BandStorePanel::Instance()->RemoveSink(this);
    UIPanel::Exit();
}

void StoreMenuPanel::Poll() {
    UIPanel::Poll();
    if (mPendingMenuIx >= 0) {
        if (mList && !mList->IsAnimating()) {
            mList->SetShowing(true);
            mList->SetProvider(mMenuStack[mPendingMenuIx]);
            mList->SetSelected(mMenuStack[mPendingMenuIx]->mIxHighlight, -1);
            mList->ForceConcealedStateOnAllEntries();
            mList->Reveal();
            mCurrentMenuIx = mPendingMenuIx;
            mPendingMenuIx = -1;
            static Message new_provider_msg(Symbol("new_provider"));
            TheUI->Handle(new_provider_msg, false);
        }
    }
}

const char *StoreMenuPanel::GetCrumbText() const {
    const char *result = gNullStr;
    int limit = mCurrentMenuIx;
    int vecLimit = (mMenuStack.end() - mMenuStack.begin()) - 1;
    if (mCurrentMenuIx >= vecLimit)
        limit = vecLimit;
    for (int i = 1; i <= limit; i++) {
        String title = mMenuStack[i]->GetTitle();
        if (!title.empty())
            result = MakeString("%s::%s", result, title.c_str());
    }
    return result;
}

void StoreMenuPanel::AddMenu(DataArray *data, const char *path, int ix) {
    if (ix < 0)
        ix = mCurrentMenuIx + 1;
    StoreMenuProvider *provider;
    if (ix == mMenuStack.size()) {
        provider = new StoreMenuProvider(data, path);
        mMenuStack.push_back(provider);
    } else {
        provider = mMenuStack[ix];
        provider->SetData(data);
    }
    int hl = 0;
    int numData = provider->NumData();
    if (mMenuStack.size() == 1 && mStartingHighlightIx < numData) {
        hl = mStartingHighlightIx;
    }
    while (!provider->IsActive(hl)) {
        hl = (hl + 1) % numData;
    }
    provider->mIxHighlight = hl;
    SetPendingMenuIx(ix);
}

DataNode StoreMenuPanel::OnBack(const DataArray *) {
    int ix = mCurrentMenuIx;
    if (ix != 0) {
        if (ix == 1) {
            mStartingHighlightIx = mMenuStack[0]->mIxHighlight;
            BandStorePanel *bsp = BandStorePanel::Instance();
            {
                String indexFile(bsp->GetIndexFile());
                bsp->Request(indexFile, true);
            }
            ix = -1;
        } else {
            ix = ix - 1;
        }
        SetPendingMenuIx(ix);
        return DataNode(1);
    } else {
        mStartingHighlightIx = 0;
        SetPendingMenuIx(-1);
        return DataNode(kDataUnhandled, 0);
    }
}

DataNode StoreMenuPanel::OnMsg(const MultipleItemsEnumCompleteMsg &msg) {
    BandStorePanel *panel = BandStorePanel::Instance();
    if (msg->Int(3)) {
        DataArray *arr = msg->Array(2);
        if (arr->FindArray(Symbol("submenus"), false)) {
            String str(msg->Str(4));
            str = str.substr(0, str.find("/") + 1);
            bool replaceRoot = msg->Int(5);
            AddMenu(arr, str.c_str(), replaceRoot ? 0 : -1);
        }
    } else {
        panel->ExitError(kStoreErrorCacheRemoved);
    }
    return DataNode(1);
}

BEGIN_HANDLERS(StoreMenuPanel)
    HANDLE_EXPR(get_menu_provider, mMenuStack[mCurrentMenuIx])
    HANDLE(back, OnBack)
    HANDLE_ACTION(reset_last_menu, SetPendingMenuIx(mMenuStack.size() - 1))
    HANDLE_ACTION(set_menu_waiting, SetPendingMenuIx(-1))
    HANDLE_EXPR(get_menu_waiting, mPendingMenuIx + 1 == 0)
    HANDLE_MESSAGE(MultipleItemsEnumCompleteMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x104)
END_HANDLERS
