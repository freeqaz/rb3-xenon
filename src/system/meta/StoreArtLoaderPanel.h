#pragma once
#include "ui/UIPanel.h"
#include "utl/NetCacheLoader.h"
#include "rndobj/Bitmap.h"

class StoreArtLoaderPanel : public UIPanel {
public:
    class ArtEntry {
    public:
        String unk0;
        NetCacheLoader *unkc;
        RndBitmap *unk10;
    };

    StoreArtLoaderPanel();
    OBJ_CLASSNAME(StoreArtLoaderPanel);
    OBJ_SET_TYPE(StoreArtLoaderPanel);
    virtual DataNode Handle(DataArray *, bool);
    virtual ~StoreArtLoaderPanel();
    virtual void Poll();
    // No Load override: retail's vtable keeps UIPanel::Load (0x82812CB8) in that
    // slot, and StoreMainPanel::Load calls UIPanel::Load directly.
    virtual void Unload();

    void ClearArt();
    void EnsureArtLoader(const String &);
    bool IsAllArtLoadedOrFailed();
    RndBitmap *GetBmp(const String &);

    std::vector<ArtEntry> mArtList; // 0x3c
};