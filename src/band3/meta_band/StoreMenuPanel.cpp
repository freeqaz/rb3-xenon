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
#include "utl/Symbol.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"
#include "utl/Symbols3.h"
#include "utl/Symbols4.h"

StoreMenuPanel *StoreMenuPanel::inst;

StoreMenuPanel::StoreMenuPanel()
    : mMenuStack(), mCurrentMenuIx(-1), mPendingMenuIx(-1), mList(0),
      mStartingHighlightIx(0) {
    inst = this;
}

StoreMenuPanel::~StoreMenuPanel() { inst = nullptr; }

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
    std::vector<StoreMenuProvider *>::iterator it = mMenuStack.begin();
    std::vector<StoreMenuProvider *>::iterator e = mMenuStack.end();
    for (; it != e; ++it) {
        delete *it;
    }
    mMenuStack.clear();
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
            result = MakeString("%s%s", result, title.c_str());
    }
    return result;
}

void StoreMenuPanel::AddMenu(DataArray *data, const char *path) {
    StoreMenuProvider *provider;
    int next = mCurrentMenuIx + 1;
    if (next == (int)mMenuStack.size()) {
        provider = new StoreMenuProvider(data, path);
        mMenuStack.push_back(provider);
    } else {
        provider = mMenuStack[next];
        provider->SetData(data);
    }
    int numData = provider->NumData();
    int ix = 0;
    if (mMenuStack.size() == 1) {
        if (mStartingHighlightIx < numData)
            ix = mStartingHighlightIx;
    }
    while (!provider->IsActive(ix)) {
        ix = (ix + 1) % numData;
    }
    provider->mIxHighlight = ix;
    SetPendingMenuIx(next);
}

DataNode StoreMenuPanel::OnBack(const DataArray *) {
    if (mCurrentMenuIx > 0) {
        SetPendingMenuIx(mCurrentMenuIx - 1);
        return DataNode(1);
    }
    mStartingHighlightIx = 0;
    SetPendingMenuIx(-1);
    return DataNode(kDataUnhandled, 0);
}

DataNode StoreMenuPanel::OnMsg(const MetadataLoadedMsg &msg) {
    BandStorePanel *panel = BandStorePanel::Instance();
    if (msg->Int(3)) {
        static Symbol submenus("submenus");
        if (msg->Array(2)->FindArray(submenus, false)) {
            AddMenu(msg->Array(2), String(msg->Str(4)).c_str());
        }
    } else {
        panel->ExitError(kStoreErrorCacheRemoved);
    }
    return DataNode(1);
}

BEGIN_HANDLERS(StoreMenuPanel)
    HANDLE_EXPR(get_menu_provider, mMenuStack[mCurrentMenuIx])
    HANDLE(back, OnBack)
    HANDLE_ACTION(reset_last_menu, SetPendingMenuIx(mCurrentMenuIx - 1))
    HANDLE_ACTION(set_menu_waiting, SetPendingMenuIx(-1))
    HANDLE_EXPR(get_menu_waiting, mPendingMenuIx + 1 == 0)
    HANDLE_MESSAGE(MetadataLoadedMsg)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0x104)
END_HANDLERS
