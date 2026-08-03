#pragma once
#include "meta_band/MetaPerformer.h"
#include "os/ContentMgr.h"
#include "ui/UIPanel.h"

class SelectDifficultyPanel : public UIPanel, public ContentMgr::Callback {
public:
    SelectDifficultyPanel();
    OBJ_CLASSNAME(SelectDifficultyPanel);
    OBJ_SET_TYPE(SelectDifficultyPanel);
    NEW_OBJ(SelectDifficultyPanel);
    virtual DataNode Handle(DataArray *, bool);
    // No user-declared dtor: UIPanel's is already virtual. A user-declared
    // `virtual ~SelectDifficultyPanel() {}` makes MSVC emit a separate
    // ??1SelectDifficultyPanel COMDAT and CALL it from ??_G -- retail has no
    // such function row and inlines the body (Callback vptr store @0x3c +
    // base dtor call) straight into the scalar deleting destructor. Same
    // pattern documented in SongSelectPanel.h.
    virtual void Enter();
    virtual void Exit();
    virtual void Poll();
    virtual bool IsLoaded() const;
    virtual void PollForLoading();
    virtual void ContentMounted(const char *, const char *);
    virtual const char *ContentDir() { return nullptr; }

    void PushSongDetailsToScreen(const MetaPerformer *);
    int GetNumSongs() const;
    bool IsBattle() const;

    float mMarqueeRotationMs; // 0x3c
    Timer mMarqueeTimer; // 0x40
    unsigned int mCurrentSongIx; // 0x70
    int unk74; // 0x74
};