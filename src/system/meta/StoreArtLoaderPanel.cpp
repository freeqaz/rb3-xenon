#include "meta/StoreArtLoaderPanel.h"
#include "obj/ObjMacros.h"
#include "utl/BufStream.h"
#include "utl/NetCacheMgr.h"
#include "meta/StorePanel.h"
#include "utl/NetCacheLoader.h"

StoreArtLoaderPanel::StoreArtLoaderPanel() {}

StoreArtLoaderPanel::~StoreArtLoaderPanel() { ClearArt(); }

void StoreArtLoaderPanel::Poll() {
    UIPanel::Poll();
    for (std::vector<ArtEntry>::iterator it = mArtList.begin(); it != mArtList.end();
         ++it) {
        if (it->unkc) {
            if (it->unkc->IsLoaded()) {
                int size = it->unkc->GetSize();
                void *pBuffer = it->unkc->GetBuffer();
                MILO_ASSERT(pBuffer, 0x31);
                it->unk10 = new RndBitmap();
                BufStream bs(pBuffer, size, true);
                // Retail calls the void Load(BinStream &) and calls SetMip
                // UNCONDITIONALLY -- there is no LoadSafely(bs, 256, 256) and no
                // bool test on it.  LoadSafely comes from the rb3-Wii dev oracle.
                it->unk10->Load(bs);
                it->unk10->SetMip(0);
                TheNetCacheMgr->DeleteNetCacheLoader(it->unkc);
                it->unkc = 0;
            } else {
                if (it->unkc->HasFailed()) {
                    // Retail's failure path does MORE than the oracle's: it reads
                    // the loader's fail type BEFORE deleting the loader, then
                    // reports it to the store panel.  The retail callee resolves to
                    // ?UncompressedSize@AsyncFile@@UAAHXZ, but NetCacheLoader does
                    // not derive from AsyncFile -- that is an ICF fold-alias, since
                    // AsyncFile::UncompressedSize is `return mUCSize;` and folds
                    // with any trivial int getter at the same offset.  The real
                    // callee is GetFailType() (mFailType @ 0x20), which is also the
                    // only reading that makes sense of the argument: the receiver is
                    // StorePanel::HandleNetCacheLoaderFailure(int), and a FAILED
                    // download reports a fail type, not an uncompressed size.
                    NetCacheMgrFailType failType = it->unkc->GetFailType();
                    TheNetCacheMgr->DeleteNetCacheLoader(it->unkc);
                    it->unkc = 0;
                    StorePanel::Instance()->HandleNetCacheLoaderFailure(failType);
                }
            }
        }
    }
}

void StoreArtLoaderPanel::Load() { UIPanel::Load(); }

void StoreArtLoaderPanel::Unload() {
    ClearArt();
    UIPanel::Unload();
}

void StoreArtLoaderPanel::EnsureArtLoader(const String &str) {
    std::vector<ArtEntry>::iterator it = mArtList.begin();
    for (; it != mArtList.end(); ++it) {
        if (it->unk0 == str)
            return;
    }
    ArtEntry entry;
    entry.unk0 = str;
    entry.unkc = TheNetCacheMgr->AddNetCacheLoader(str.c_str(), (NetLoaderPos)1);
    entry.unk10 = 0;
    mArtList.push_back(entry);
}

RndBitmap *StoreArtLoaderPanel::GetBmp(const String &str) {
    // Retail has NO str.empty() early-out -- it loads mArtList.begin() and branches
    // straight to the loop condition.  The guard comes from the rb3-Wii DEV oracle;
    // it emitted 4 instructions (lwz 0x8(str) / lbz / cmplwi / beq) that retail
    // does not have.  Dropping it is behaviour-neutral unless an ArtEntry is itself
    // named "", in which case retail returns that entry -- and retail is the
    // reference.
    for (std::vector<ArtEntry>::iterator it = mArtList.begin(); it != mArtList.end();
         ++it) {
        if (it->unk0 == str)
            return it->unk10;
    }
    MILO_WARN("%s isn't in mArtList\n", str.c_str());
    return nullptr;
}

bool StoreArtLoaderPanel::IsAllArtLoadedOrFailed() {
    for (std::vector<ArtEntry>::iterator it = mArtList.begin(); it != mArtList.end();
         ++it) {
        if (it->unkc)
            return false;
    }
    return true;
}

void StoreArtLoaderPanel::ClearArt() {
    for (std::vector<ArtEntry>::iterator it = mArtList.begin(); it != mArtList.end();
         ++it) {
        TheNetCacheMgr->DeleteNetCacheLoader(it->unkc);
        RELEASE(it->unk10);
    }
    mArtList.clear();
}

BEGIN_HANDLERS(StoreArtLoaderPanel)
    HANDLE_SUPERCLASS(UIPanel)
    HANDLE_CHECK(0xC2)
END_HANDLERS