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

    PreloadResult mPreloadResult; // 0x3c
    std::vector<String> mPreloadedFiles; // 0x40
    bool mMounted; // 0x4c
    std::vector<Symbol> mContentNames; // 0x50
    Hmx::Object *mAppReadFailureHandler; // 0x5c
    bool mContentCorrupt; // 0x60
    String mCorruptContentName; // 0x64
    bool mSongDoesNotExist; // 0x6c
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
