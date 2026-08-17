#pragma once
#include "meta/StoreOffer.h"
#include "obj/Object.h"
#include "stl/_vector.h"

class DataArray;
class DataNetLoader;
class NetCacheLoader;
class RndTex;
class StorePreviewMgr;

/** Retail-only (Xbox 360) DLC store-preview async op, polled every frame by
    MusicLibrary::Poll. NO oracle exists (absent from the rb3-Wii dev branch and
    from DC3). Reconstructed from the retail XEX (title 45410914).

    RTTI: `.?AVMusicLibraryStore@@`, vtable @0x820adf94, Complete Object Locator
    @0x821e1a88 (attributes 0x0 = single inheritance, no virtual bases).
    (ADDRESSES CORRECTED, lane W16-HEADERTRUTH: this used to cite vtable
    @0x820abc8c / COL @0x821da4e0 / CHD @0x821da4f4. None of the three is what
    it claimed -- 0x820abc8c has no COL at [-1] and is EH data, the word at
    +4 being the MSVC FuncInfo magic 0x19930522. ★ The DECODE BELOW IS
    UNAFFECTED AND IS NOW CONFIRMED against the real COL: it does decode to
    exactly 3 classes in that order. Only the addresses were wrong.)
    The CHD decodes to a plain single-inheritance chain (3 classes, all
    mdisp=0/pdisp=-1):

        MusicLibraryStore  ->  Hmx::Object  ->  ObjRefOwner (RTTI: "ObjRef")

    i.e. MusicLibraryStore is an ordinary Hmx::Object subclass; it overrides
    only the destructor among the standard vtable slots (slots 1-9 inherit
    Object's / ICF-folded generic impls). Poll/Finish/SetStorePreview/
    ClearPreview are NON-virtual (MusicLibrary calls them directly).

    Size 0x64. Members (this-relative, Object base occupies 0x00-0x27):
      0x28 int                         mState  (0=idle, 2=done, 3=clearing, 4=failed)
      0x2c std::vector<StoreOffer*>     mOffers (OWNED; dtor DeleteAll's them)
      0x38 DataNetLoader*              mPreviewLoader (preview_audio download)
      0x3c DataArray*                  mPreviewData
      0x40 NetCacheLoader*             mCacheLoader
      0x44 <loader/stream, vtable obj> mCacheStream
      0x48 RndTex*                     mPreviewTex (store thumbnail; SetBitmap)
      0x4c StorePreviewMgr*            mPreviewMgr (heap, 0x60 bytes)
      0x50 <DataResultList*, vtable>   mResults
      0x54 std::vector<OverlappedIO>   mOverlapped (element size 8)
      0x60 int                         mUnk60
*/
class MusicLibraryStore : public Hmx::Object {
public:
    struct OverlappedIO {
        int mSongID; // 0x0
        void *mLoader; // 0x4
    };

    MusicLibraryStore();
    virtual ~MusicLibraryStore();

    void Poll(); // retail 0x825A50F8
    void Finish(); // retail 0x825A3ED0
    void ClearPreview(); // retail 0x825A3DD0
    /** retail 0x825BC900 — a frameless 2-instruction tail-jump thunk
        `lwz r3,0x4c(r3); b 0x827B1B78`, i.e. `{ mPreviewMgr->ClearCurrentPreview(); }`.
        Its single retail caller is MusicLibrary::ClearSongPreview (0x8253AD58), and
        offset 0x4c is mPreviewMgr below, so the OWNING CLASS is evidenced even though
        the METHOD NAME is not: neither oracle has a MusicLibraryStore at all. Named
        after its address per the Unk825BCA38 precedent in MusicLibrary.h rather than
        guessed. It has NO .pdata entry of its own — a live instance of
        ".pdata-absence is not a not-a-function test". */
    void Unk825BC900(); // retail 0x825BC900
    void SetStorePreview(int); // retail 0x825A4288
    /** retail 0x825A3E70 — linear search of mOffers by single-song id. */
    StoreOffer *FindOfferBySongID(int) const;

    int mState; // 0x28
    std::vector<StoreOffer *> mOffers; // 0x2c
    DataNetLoader *mPreviewLoader; // 0x38
    DataArray *mPreviewData; // 0x3c
    NetCacheLoader *mCacheLoader; // 0x40
    void *mCacheStream; // 0x44
    RndTex *mPreviewTex; // 0x48
    StorePreviewMgr *mPreviewMgr; // 0x4c
    void *mResults; // 0x50
    std::vector<OverlappedIO> mOverlapped; // 0x54
    int mUnk60; // 0x60
};
