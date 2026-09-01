#pragma once

#include "meta/StoreArtLoaderPanel.h"
#include "obj/Data.h"
#include "utl/Str.h"

class RndMat;
class RndTex;
class RndAnimatable;
class AppLabel;

class StoreMainPanel : public StoreArtLoaderPanel {
public:
    class NewReleaseEntry {
    public:
        String mStrName; // 0x0
        String mText1; // 0xC
        String mText2; // 0x18
        String mText3; // 0x24
        String mText4; // 0x30
        // sizeof == 0x3C
    };

    StoreMainPanel();
    virtual ~StoreMainPanel();
    OBJ_CLASSNAME(StoreMainPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual void Poll();
    virtual void Load();
    virtual void Unload();
    virtual void FinishLoad();
    NEW_OBJ(StoreMainPanel);

    DataNode OnMsg(const class MetadataLoadedMsg &);
    virtual void SetType(Symbol);
    void ParseConfigData();
    void ClearConfigData();
    const NewReleaseEntry *CurrentEntry() const;
    const char *MarqueePath() const;

    DataArray *mConfigData; // 0x48
    float mTimeNextEvent; // 0x4c
    int mCurrentEntry; // 0x50
    float mDisplayRate; // 0x54
    float mCrossfadeDuration; // 0x58
    RndMat *mCoverArtMats[6]; // 0x5c
    bool unk6c; // 0x74
    RndTex *mNoneTex; // 0x78
    std::vector<RndTex *> mCoverArtTexs; // 0x7c
    RndAnimatable *mScrollAnim; // 0x88
    AppLabel *mLabel1; // 0x8c
    AppLabel *mLabel2; // 0x90
    AppLabel *mLabel3; // 0x94
    std::vector<NewReleaseEntry> mNewReleaseList; // 0x98
};
