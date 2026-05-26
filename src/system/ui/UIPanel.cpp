#include "ui/UIPanel.h"
#include "obj/Data.h"
#include "obj/DataUtl.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "ui/PanelDir.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Symbol.h"
#ifdef HX_NATIVE
#include "obj/Dir.h"
#include "rndobj/Mesh.h"
#include "rndobj/Text.h"
#include <cstdlib>
static inline bool DebugUIFlow() {
    static bool checked = false, val = false;
    if (!checked) { val = getenv("MILO_DEBUG_UI_FLOW") != nullptr; checked = true; }
    return val;
}

static inline bool DebugSharedPanelDirs() {
    static bool checked = false, val = false;
    if (!checked) { val = getenv("MILO_DEBUG_SHARED_PANELS") != nullptr; checked = true; }
    return val;
}
#endif

#ifdef HX_NATIVE
int UIPanel::sMaxPanelId;
bool UIPanel::sIsFinalDrawPass;
#endif

ObjectDir *UIPanel::DataDir() {
    if (mDir) {
        return mDir->DataDir();
    }
    return Hmx::Object::DataDir();
}

void UIPanel::SetTypeDef(DataArray *data) {
    Hmx::Object::SetTypeDef(data);
    if (data) {
        static Symbol focus("focus");
        data->FindData(focus, mFocusName, false);
        static Symbol force_exit("force_exit");
        data->FindData(force_exit, mForceExit, false);
    }
}

void UIPanel::CheckLoad() {
    if (++mLoadRefs == 1)
        Load();
}

bool UIPanel::CheckIsLoaded() {
    if (mState != kUnloaded)
        return true;
    else {
        PollForLoading();
        if (IsLoaded()) {
            FinishLoad();
#ifdef HX_NATIVE
            if (DebugUIFlow()) {
                printf("DC3 UI: Panel '%s' finished loading (state=%d)\n", Name(), (int)mState);
            }
#endif
            return true;
        } else {
#ifdef HX_NATIVE
            if (DebugUIFlow()) {
                printf("DC3 UI: Panel '%s' still loading: mLoader=%p loaded=%d loadRefs=%d\n",
                       Name(), (void *)mLoader, mLoader ? mLoader->IsLoaded() : -1, mLoadRefs);
            }
#endif
            return false;
        }
    }
}

UIComponent *UIPanel::FocusComponent() {
    if (mDir)
        return mDir->FocusComponent();
    else
        return nullptr;
}

bool UIPanel::Entering() const {
    if (mDir && !mLoaded)
        return mDir->Entering();
    else
        return false;
}

void UIPanel::Draw() {
    if (mFinalDrawPassFlag == sIsFinalDrawPass && mDir && !mLoaded) {
        mDir->DrawShowing();
    }
}

void UIPanel::SetFocusComponent(UIComponent *comp) {
    if (mDir) {
        mDir->SetFocusComponent(comp, gNullStr);
    }
}

UIPanel::UIPanel()
    : mDir(0), mLoader(0), mLoaded(0), mState(kUnloaded), mPaused(0), mShowing(1),
      mForceExit(0), mLoadRefs(0), mFinalDrawPassFlag(0) {
    mPanelId = sMaxPanelId++;
    MILO_ASSERT(sMaxPanelId < 0x8000, 0x27);
}

void UIPanel::SetLoadedDir(PanelDir *dir, bool loaded) {
    MILO_ASSERT(!mLoader, 0x65);
    MILO_ASSERT(dir, 0x66);
#ifdef HX_NATIVE
    if (DebugSharedPanelDirs()) {
        printf(
            "DC3 UI: UIPanel '%s' SetLoadedDir dir='%s' shared=%d prev='%s'\n",
            Name(),
            dir->Name(),
            loaded,
            mDir ? mDir->Name() : "<null>"
        );
    }
#endif
    if (mDir) {
        mDir->SetOwnerPanel(nullptr);
    }
    mDir = dir;
    mLoaded = loaded;
    mDir->SetOwnerPanel(this);
}

void UIPanel::UnsetLoadedDir() {
    MILO_ASSERT(!mLoader, 0x73);
#ifdef HX_NATIVE
    if (DebugSharedPanelDirs() && mDir) {
        printf("DC3 UI: UIPanel '%s' UnsetLoadedDir dir='%s'\n", Name(), mDir->Name());
    }
#endif
    if (mDir) {
        mDir->SetOwnerPanel(nullptr);
    }
    mDir = nullptr;
    mLoaded = false;
}

void UIPanel::PollForLoading() {
    MILO_ASSERT(mState == kUnloaded, 0xE4);
    if (mLoader && mLoader->IsLoaded()) {
        PanelDir *pDir = dynamic_cast<PanelDir *>(mLoader->GetDir());
        MILO_ASSERT_FMT(pDir, "%s not PanelDir", mLoader->LoaderFile());
        RELEASE(mLoader);
        SetLoadedDir(pDir, mLoaded);
    }
}

bool UIPanel::Unloading() const {
    if (!mFilePath.empty()) {
        if (TheLoadMgr.GetLoader(mFilePath)) {
            return true;
        }
        const_cast<UIPanel *>(this)->mFilePath.Set(FilePath::Root().c_str(), gNullStr);
    }
    return false;
}

DataNode UIPanel::OnLoad(DataArray *da) {
    CheckLoad();
    if (da->Size() > 2) {
        if (da->Int(2) && mLoader) {
            TheLoadMgr.PollUntilLoaded(mLoader, 0);
            bool bLoaded = CheckIsLoaded();
            MILO_ASSERT(bLoaded, 0x1DB);
        }
    }
    return 0;
}

void UIPanel::CheckUnload() {
    if (mLoadRefs >= 1) {
        if (mState == kDown) {
            static Message msg("exit_complete");
            Handle(msg, false);
        }
        if (--mLoadRefs == 0)
            Unload();
    }
}

void UIPanel::Load() {
    if (mState != kUnloaded) {
        MILO_FAIL(
            "Can't load panel %s, it's already in state %s",
            Name(),
            mState == kUp ? "kUp" : "kDown"
        );
    }
    static Message msg("load");
    HandleType(msg);
    if (TypeDef()) {
        static Symbol proj_file("proj_file");
        static Symbol file("file");
        static Symbol heap("heap");
        FilePath fp;
        LoaderPos pos = kLoadBack;
        DataArray *fileArr = TypeDef()->FindArray(file, false);
        if (fileArr) {
            Hmx::Object *dataThis = DataSetThis(this);
            fp.Set(FileGetPath(fileArr->File()), fileArr->Str(1));
            if (fileArr->Size() == 3) {
                pos = (LoaderPos)fileArr->Int(2);
            }
            DataSetThis(dataThis);
        }
        int heapNum = GetCurrentHeapNum();
        DataArray *heapArr = TypeDef()->FindArray(heap, false);
        if (heapArr) {
            heapNum = MemFindHeap(heapArr->Str(1));
        }
        if (!fp.empty()) {
#ifdef HX_NATIVE
            if (DebugUIFlow()) {
                printf("DC3 UI: Panel '%s' starting load from '%s' pos=%d\n",
                       Name(), fp.c_str(), (int)pos);
            }
#endif
            MemHeapTracker t(heapNum);
            mLoader = new DirLoader(fp, pos, nullptr, nullptr, nullptr, false, nullptr);
            MILO_ASSERT(mLoader, 0xA9);
            mLoaded = false;
        }
#ifdef HX_NATIVE
        else if (DebugUIFlow()) {
            printf("DC3 UI: Panel '%s' load skipped: empty file path\n", Name());
        }
#endif
    }
}

bool UIPanel::IsLoaded() const {
    if (mState != kUnloaded)
        return true;
    if (mLoader && !mLoader->IsLoaded())
        return false;
    static Message msg("is_loaded");
    DataNode node = const_cast<UIPanel *>(this)->HandleType(msg);
    if (node.Type() != kDataUnhandled)
        return node.Int();
    else
        return true;
}

void UIPanel::FinishLoad() {
    static Message msg("finish_load");
    HandleType(msg);
    MILO_ASSERT(mLoadRefs > 0, 0x11C);
    mState = kDown;
}

bool UIPanel::Exiting() const {
#ifdef __EMSCRIPTEN__
    return false;
#else
    if (mDir && !mLoaded && mDir->Exiting()) {
        return true;
    }
    static Message msg("exiting");
    DataNode node = const_cast<UIPanel *>(this)->HandleType(msg);
    if (node.Type() != kDataUnhandled) {
        return node.Int();
    } else
        return false;
#endif
}

void UIPanel::Enter() {
    MILO_ASSERT(mState == kDown, 0x158);
#ifdef HX_NATIVE
    // Block Kinect tutorial panels from entering on native.
    // DTA handlers call {$tutorial_nav_panel enter} which bypasses our
    // UIScreen::Enter() skip. Suppress here at the panel level.
    if (strstr(Name(), "tutorial")) {
        return;
    }
#endif
    if (!mFocusName.empty() && mDir) {
        SetFocusComponent(mDir->FindComponent(mFocusName.c_str()));
    }
    MILO_ASSERT(mLoadRefs > 0, 0x15E);
    mState = kUp;
    if (mDir && !mLoaded) {
        mDir->Enter();
    }
    static Message msg("enter");
    HandleType(msg);
}

void UIPanel::Exit() {
    MILO_ASSERT(mState == kUp, 0x16F);
    bool resetFocus = false;
    const DataArray *td = TypeDef();
    if (td) {
        td->FindData("reset_focus", resetFocus, false);
    }
    if (!resetFocus && FocusComponent()) {
        mFocusName = FocusComponent()->Name();
    }
    MILO_ASSERT(mLoadRefs > 0, 0x178);
    mState = kDown;
    static Message msg("exit");
    HandleType(msg);
    if (mDir && !mLoaded) {
        mDir->Exit();
    }
}

void UIPanel::Poll() {
    static Message msg("poll");
    HandleType(msg);
    if (mDir && !mLoaded) {
        mDir->Poll();
    }
}

BEGIN_HANDLERS(UIPanel)
    HANDLE_EXPR(is_loaded, IsLoaded())
    HANDLE_EXPR(check_is_loaded, CheckIsLoaded())
    HANDLE_EXPR(is_unloaded, mState == kUnloaded)
    HANDLE_EXPR(is_referenced, IsReferenced())
    HANDLE_EXPR(is_up, mState == kUp)
    HANDLE_ACTION(set_paused, SetPaused(_msg->Int(2)))
    HANDLE_EXPR(paused, mPaused)
    HANDLE(load, OnLoad)
    HANDLE_ACTION(unload, CheckUnload())
    HANDLE_ACTION(set_focus, SetFocusComponent(_msg->Obj<UIComponent>(2)))
    HANDLE_ACTION(enter, Enter())
    HANDLE_ACTION(exit, Exit())
    HANDLE_EXPR(loaded_dir, mDir)
    HANDLE_ACTION(set_showing, SetShowing(_msg->Int(2)))
    HANDLE_EXPR(showing, Showing())
    HANDLE_ACTION(set_loaded_dir, SetLoadedDir(_msg->Obj<class PanelDir>(2), false))
    HANDLE_ACTION(set_loaded_dir_shared, SetLoadedDir(_msg->Obj<class PanelDir>(2), true))
    HANDLE_ACTION(unset_loaded_dir, UnsetLoadedDir())
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_MEMBER_PTR(mDir)
END_HANDLERS

void UIPanel::Unload() {
    if (mState == kUnloaded && mLoader && mLoader->IsLoaded()) {
        MILO_NOTIFY("Unload occurred before FinishedLoad");
        mLoadRefs++;
        TheLoadMgr.PollUntilLoaded(mLoader, nullptr);
        MILO_ASSERT(CheckIsLoaded(), 0xB9);
        mLoadRefs--;
    }
    static Message msg("unload");
    HandleType(msg);
    if (UIPanel::IsLoaded()) {
        bool async = false;
        if (TypeDef()) {
            static Symbol unload_async("unload_async");
            DataArray *asyncArr = TypeDef()->FindArray(unload_async, false);
            if (asyncArr) {
                if (asyncArr->Int(1) != 0)
                    async = true;
            }
        }
        if (async) {
            TheLoadMgr.StartAsyncUnload();
            mFilePath.Set(FilePath::Root().c_str(), mDir->GetPathName());
        } else
            mFilePath.Set(FilePath::Root().c_str(), gNullStr);
        RELEASE(mDir);
        if (async)
            TheLoadMgr.FinishAsyncUnload();
    }
    RELEASE(mLoader);
    MILO_ASSERT(mLoadRefs == 0, 0xDD);
    mLoaded = false;
    mState = kUnloaded;
}
