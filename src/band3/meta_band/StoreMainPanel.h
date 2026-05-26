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

    DataArray *mConfigData; // 0x40
    float mTimeNextEvent; // 0x44
    int mCurrentEntry; // 0x48
    float mDisplayRate; // 0x4c
    float mCrossfadeDuration; // 0x50
    RndMat *mCoverArtMats[6]; // 0x54
    bool unk6c; // 0x6c
    RndTex *mNoneTex; // 0x70
    std::vector<RndTex *> mCoverArtTexs; // 0x74
    RndAnimatable *mScrollAnim; // 0x7c
    AppLabel *mLabel1; // 0x80
    AppLabel *mLabel2; // 0x84
    AppLabel *mLabel3; // 0x88
    std::vector<NewReleaseEntry> mNewReleaseList; // 0x8c
};
