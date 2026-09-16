#pragma once
#include "meta/ConnectionStatusPanel.h"
#include "os/ContentMgr.h"
#include "os/PlatformMgr.h"
#include "utl/UTF8.h"
#include "xdk/XAPILIB.h"

class XboxContent : public Content {
public:
    XboxContent(const XCONTENT_CROSS_TITLE_DATA &, int, int, bool);
    virtual ~XboxContent();
    virtual const char *Root() { return mContentPath.c_str(); }
    virtual bool OnMemcard() { return Location() == kLocationRemovableMem; }
    virtual ContentLocT Location();
    virtual unsigned long LicenseBits() { return mLicenseBits; }
    virtual bool HasValidLicenseBits() { return mValidLicenseBits; }
    // VIRTUAL, and it lands on the TRAILING vtable slot [14].  Retail's
    // ??_7XboxContent@@6B@ (0x8208968c) has 15 slots -- slot[15] is the
    // 0xffffffff sentinel, so the table demonstrably ends -- while the base
    // Content (0x8208959c) and the sibling RootContent (0x820895d8) have 14
    // each.  That sibling count is the control: were IsCorrupt declared on
    // Content (as DC3 does, at ITS slot 6), RootContent would have inherited a
    // 15th slot too.  It has 14, so the 15th virtual is introduced HERE, and
    // MSVC appends it after GetLRM regardless of where it sits among the
    // overrides above.
    // Dispatch is proven by retail's own machine code, not by any name:
    // XboxContentMgr::IsCorrupt (0x82520668, ContentMgr vtable slot [34])
    // walks the content list, dynamic_casts Content->XboxContent via
    // __RTDynamicCast, and calls `lwz r11,0x38(r11)` = slot 14 -- the same
    // shape our ContentMgr_Xbox.cpp already implements.
    // Body is retail's, read off 0x8251f8f0 (20-byte leaf, no .pdata entry):
    //   lwz r11,0xc(r3); addi r11,r11,-1; cntlzw r11,r11
    //   rlwinm r3,r11,0x1b,0x1f,0x1f; blr        => return field_0xc == 1
    // (the same (x-1)/cntlzw/rlwinm idiom this compiler emits for `== 1` in
    // OnMemcard just above).  this+0xc is mXData.dwContentType: the ctor
    // 0x8251fb40 does `addi r3,r30,8; li r5,0x138; memcpy` -- mXData is at
    // this+8 and is the full 0x138-byte struct, so +0xc is its dwContentType
    // at +0x4.  Independently anchored by DisplayName (slot 12) tail-calling
    // WideCharToChar on `this+0x10` = mXData.szDisplayName at +0x8, and by
    // mLicenseBits landing at 0x8+0x138 = 0x140.
    // NOT `mState == 8 && mCorrupt` (the DC3 body): retail's XboxContent has
    // no mCorrupt at all -- a scan of the whole TU (0x8251f800-0x82520200)
    // finds constant traffic on 0x160 (mState) and 0x168 (mPendingDelete) and
    // NOT ONE access to 0x161 or 0x169, in any function including Poll.
    virtual bool IsCorrupt() { return mXData.dwContentType == 1; }
    virtual State GetState() { return mState; }
    virtual void Poll();
    virtual void Mount();
    virtual void Unmount();
    virtual void Delete();
    virtual Symbol FileName() { return mFilename; }
    virtual const char *DisplayName() {
        const unsigned short *displayName =
            reinterpret_cast<const unsigned short *>(mXData.szDisplayName);
        return WideCharToChar(displayName);
    }
    virtual unsigned int GetLRM() { return mLRM; }

private:
    XOVERLAPPED *mOverlapped; // 0x4
    XCONTENT_CROSS_TITLE_DATA mXData; // 0x8
    unsigned long mLicenseBits; // 0x140
    bool mValidLicenseBits; // 0x144
    String mRoot; // 0x148
    String mContentPath; // 0x154
    State mState; // 0x160
    int mPadNum; // 0x164
    bool mPendingDelete; // 0x168
    bool mCorrupt; // 0x169
    Symbol mFilename; // 0x16c
    unsigned int mLRM; // 0x170
};

#define kNumberOfBuffers 6
#define kContentRootMaxLength 12

class XboxContentMgr : public ContentMgr {
public:
    // Hmx::Object
    XboxContentMgr() {}
    virtual DataNode Handle(DataArray *, bool);
    // ContentMgr
    virtual void Init();
    virtual void Terminate();
    virtual void StartRefresh();
    virtual void PollRefresh();
    virtual const char *TitleContentPath() { return ContentPath(0); }
    virtual const char *ContentPath(int) { return MakeString("UPDATE:"); }
    virtual bool MountContent(Symbol);
    virtual bool IsMounted(Symbol);
    virtual bool IsCorrupt(Symbol);
    virtual bool DeleteContent(Symbol);
    virtual bool IsDeleteDone(Symbol);
    virtual bool GetLicenseBits(Symbol, unsigned long &ul);

protected:
    virtual void NotifyMounted(Content *);
    virtual void NotifyUnmounted(Content *);
    virtual void NotifyDeleted(Content *);
    virtual void NotifyFailed(Content *);

private:
    DataNode OnMsg(const SigninChangedMsg &);
    DataNode OnMsg(const ConnectionStatusChangedMsg &);
    DataNode OnMsg(const StorageChangedMsg &);
    DataNode OnMsg(const ContentInstalledMsg &);

    // 0x70 is XboxContentMgr's FIRST member -- ContentMgr ends there, its last
    // member being mReadFailureHandler at 0x6c.
    //
    // It is a BYTE, not the `unsigned int` filler that used to hold this slot.
    // StartRefresh (0x82520fd0) opens with
    //     lbz r11,0x70(r3) / cmplwi r11,0x0 / beq <epilogue>
    // i.e. a bool gating the entire body, and the constructor (fn_825213D0)
    // closes with `li r9,0x1 ; stb r9,0x70(r30)` -- initialised true.  A 4-byte
    // load would have been `lwz`; it is `lbz`, so the type is byte-sized.
    //
    // The 0x74/0x75 pair below is unchanged and still correct (NotifyFailed
    // fn_82520830 stores this+0x75; StartRefresh reads both with lbz and clears
    // both with stb).  DC3 has no 0x70 member at all -- ITS unk70/unk71 are our
    // unk74/unk75, carrying the same `mDirty || (unk74 && unk75)` expression,
    // which retail places four bytes later.
    //
    // 0x71-0x73 are UNOBSERVED anywhere in this TU.  They are spelled as bools
    // purely to reproduce the measured 0x74 offset -- any three bytes would do,
    // and nothing here should be read as a claim that three more flags exist.
    bool unk70; // 0x70
    bool unk71; // 0x71  (unobserved -- padding to the measured 0x74)
    bool unk72; // 0x72  (unobserved)
    bool unk73; // 0x73  (unobserved)
    bool unk74; // 0x74
    bool unk75; // 0x75
    void *mEnumHandles[kNumberOfBuffers]; // 0x78
    XCONTENT_CROSS_TITLE_DATA mXDatas[kNumberOfBuffers]; // 0x90
    XOVERLAPPED *mOverlappeds[kNumberOfBuffers]; // 0x7e0
    int unk7f8; // 0x7f8
    int unk7fc; // 0x7fc
    bool mEnumerateSaveGameExports; // 0x800
};

extern XboxContentMgr gContentMgr;
