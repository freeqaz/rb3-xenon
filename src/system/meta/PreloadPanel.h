#pragma once
#include "meta/SongMgr.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/ContentMgr.h"
#include "os/FileCache.h"
#include "ui/UIPanel.h"
#include "ui/UIScreen.h"
#include "utl/Symbol.h"
#include <vector>

class PreloadPanel : public UIPanel, public ContentMgr::Callback {
public:
    enum PreloadResult {
        kPreloadInProgress = 0,
        kPreloadSuccess = 1,
        kPreloadFailure = 2
    };

    PreloadPanel();
    // Hmx::Object
    virtual ~PreloadPanel();
    OBJ_CLASSNAME(PreloadPanel);
    OBJ_SET_TYPE(PreloadPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void SetTypeDef(DataArray *);

    // UIPanel
    virtual void Load();
    virtual bool IsLoaded() const;
    virtual void Unload();
    virtual void PollForLoading();
    virtual void FinishLoad();

    // ContentMgr::Callback
    virtual void ContentMounted(char const *, char const *);
    virtual void ContentFailed(char const *);

    NEW_OBJ(PreloadPanel)
    static FileCache *sCache;

protected:
    Symbol CurrentSong() const;

    // ⚠ OFFSETS BELOW ARE COMPILER-VERIFIED (cl.exe /d1reportSingleClassLayout,
    // 2026-08-17, lane W44-REQUEUE). They previously read 0x3c/0x40/0x4c/0x50/
    // 0x5c/0x60/0x64/0x6c -- every one stale by 4, because the {vfptr} for the
    // ContentMgr::Callback base at 0x3c was not counted. The LAYOUT was always
    // correct; only these comments were wrong, and they are what made
    // PreloadPanel::Load look like a container-element-type defect to three
    // rounds of triage. Retail confirms all three witnessed offsets:
    //   push_back into this+0x44  (?push_back@?$vector@VString@@...)
    //   stb r26, 0x50(r3)         (mMounted = true at top of Load)
    //   lbz r11, 0x74(r28)        (mSongDoesNotExist)
    // The element type is String and was never in doubt -- do not "fix" it.
    PreloadResult mPreloadResult; // 0x40
    std::vector<String> mPreloadedFiles; // 0x44
    bool mMounted; // 0x50
    std::vector<Symbol> mContentNames; // 0x54
    Hmx::Object *mAppReadFailureHandler; // 0x60
    bool mContentCorrupt; // 0x64
    String mCorruptContentName; // 0x68
    bool mSongDoesNotExist; // 0x74
    // NOTE(laneBQ2): `int mMaxCacheSize` used to be the last member here. Retail RB3
    // does not have it: the rb3-Wii oracle's PreloadPanel ends at `bool unk68`
    // (= mSongDoesNotExist) and our member list matches it 1:1 with mMaxCacheSize as
    // the only extra. Corroborated by ?SetType@PreloadPanel@@ at 0x827b42a8, whose
    // vbase-displacement immediate is 120 -- exactly where the Object vtordisp lands
    // once this 4-byte member is gone. Moved to a file-scope static in the .cpp.

private:
    void CheckTypeDef(Symbol);
    bool CheckFileCached(char const *);
    SongMgr *FindSongMgr() const;
    DataNode OnMsg(const ContentReadFailureMsg &);
    DataNode OnMsg(const UITransitionCompleteMsg &);
    void OnContentMountedOrFailed(char const *);
    void StartCache();
};
