#pragma once
#include "obj/Object.h"
#include "obj/Msg.h"
#include "utl/Str.h"
#include <list>
#include <map>
#include <utility>
#include <vector>

// Minimal slice of the rb3-Wii meta/StorePackedMetadata.h, declaring only the
// pieces StoreMainPanel.cpp depends on, with ABI-correct offsets. The full
// header pulls in the complete StoreOffer.h packed-offer type family, which our
// tree only carries in trimmed form; widening it would ripple across the other
// pinned Store TUs (header edits are the #1 cross-TU regression source). Keep
// this minimal and offset-faithful.
//
// ── DRAINED VEIN (lane BT-1, 2026-07-30). Do not re-port the packed family. ──
// A 2,014-line meta/StorePackedMetadata.cpp + 248-line
// band3/meta_band/StoreOfferContentsProvider.cpp were ported from the rb3-Wii
// DEV oracle and preserved at commit f69d26fa (tag
// salvage-storepackedmetadata-20260730). Evaluated and rejected:
//
//   * The packed-metadata subsystem DOES NOT EXIST in RB3-360 retail. Tested
//     against orig/45410914/band.exe with exact-match `strings`, controls
//     passing (marquee_path/play_preview/dlc_store/`dlc_store/%s/dlc_upsell_
//     %s_%s.dta` all found): all 4 StoreMetadataManager handler tokens absent
//     (check_content_size, debug_download, debug_purchase, exit_error); 9 of 11
//     StoreOfferContentsProvider handler tokens absent (only the generic
//     build_list/clear_list hit, used elsewhere); all 4 path format strings
//     absent (`/preview_art/%s_nomip.png_%s`, `/preview_audio/%s_prev.bik`,
//     `/album_art/UGC_%d_keep.png_%s`, `/audio_prev/UGC_%s_prev.bik`);
//     `ECContentCatalogInfo`, `Store: file %s is missing`, `%sversion` absent.
//     autoid.json proposes 0 clusters for either file (21 for StoreOffer).
//     Retail's store is DTA/marketplace-driven (`ml_store_*`, dlc_upsell dta),
//     not the Wii packed-binary blob + EC_*/NAND stack the port re-declares.
//     => No .text span exists to pin. These TUs can never score.
//
//   * Measured anyway on 3384ec22 in a worktree: the port COMPILES clean on
//     current main, and both objs build (256 KB / 71 KB) -- but
//     Delta(matched_functions - masked_equal_functions) = 0, matched_code = 0.000,
//     because both units are unpinned (the salvage's splits.txt diff is four
//     blank lines, zero pins).
//
//   * Useful negative: widening this header is metric-NEUTRAL. The cascade was
//     verified to fire (all 5 pinned includers -- BandStorePanel,
//     SetlistToStorePanel, StoreMenuPanel, TokenRedemptionPanel, StoreMainPanel
//     -- actually recompiled) and the metric did not move. So the "would ripple
//     across the other pinned Store TUs" warning above is over-cautious for a
//     pure widening; it is still true that the widening buys nothing.
//
// Live frontier nearby, if you want one: meta/StoreOffer.cpp (the real 360
// DataArray store family) is compiled but UNPINNED, with 21 autoid proposals.

class StoreMarqueeTable {
public:
    ~StoreMarqueeTable();
    bool Load(const char *);

    char *mBuffer;     // 0x0
    char *mMarquees;   // 0x4
    int mNumMarquees;  // 0x8
};

class StoreSingleStringTable {
public:
    ~StoreSingleStringTable() {
        if (mBuffer)
            MemFree(mBuffer);
    }
    bool LoadFile(const char *);
    const char *GetString(int idx) const {
        if (idx < 0 || idx >= mNumStrings)
            return "STRING INDEX OUT OF BOUNDS";
        else
            return mStrings[idx];
    }

    int mNumStrings;  // 0x0
    char *mBuffer;    // 0x4
    char **mStrings;  // 0x8
};

class StoreStringTable {
public:
    ~StoreStringTable() {}
    bool Load(const char *);
    bool IsValid(int);

    StoreSingleStringTable mNonLocalized;  // 0x0
    StoreSingleStringTable mLocalized;     // 0xc
};

class StoreMetadataManager : public Hmx::Object {
public:
    ~StoreMetadataManager();
    virtual DataNode Handle(DataArray *, bool);

    const char *GetString(int idx) const {
        StoreStringTable *table = mStringTable;
        if (idx & 0x8000)
            return table->mLocalized.GetString((idx & 0x7FFF) - 1);
        else
            return table->mNonLocalized.GetString(idx - 1);
    }

    void AddSetlistOffer(int);
    void ClearSetlistOffers();

    unsigned int mFlags;                // 0x28
    int mLoadingState;                  // 0x2c
    int mContentSize;                   // 0x30
    String mBasePath;                   // 0x34
    void *mVersion;                     // 0x40
    StoreStringTable *mStringTable;     // 0x44
    void *mSongTable;                   // 0x48
    void *mOfferTable;                  // 0x4c
    void *mRbnOfferTable;               // 0x50
    void *mPageTable;                   // 0x54
    void *mCurrentPage;                 // 0x58
    StoreMarqueeTable *mMarqueeTable;   // 0x5c
    void *mRedemptionsTable;            // 0x60
    std::map<unsigned long long, void *> unk58;  // 0x64
    int unk70;
    int unk74;
    int unk78;
    int unk7c;
    int unk80;
    int mErrorMsg;                      // 0x90
    unsigned long long unk88;           // 0x98
    unsigned short unk90;               // 0xa0
    int unk94;
    std::list<std::pair<unsigned long long, unsigned short> > unk98;
    int unka0;
};

extern StoreMetadataManager TheStoreMetadata;
