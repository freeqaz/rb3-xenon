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
    virtual bool IsCorrupt() { return mState == 8 && mCorrupt; }
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
    String mContentPath; // 0x150
    State mState; // 0x160
    int mPadNum; // 0x164
    bool mPendingDelete; // 0x168
    bool mCorrupt; // 0x161
    Symbol mFilename; // 0x16c
    unsigned int mLRM; // 0x170
};

#define kNumberOfBuffers 7
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
    virtual bool IsCorrupt(Symbol, const char *&);
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

    // Retail evidence (NotifyFailed body stores to this+0x75, confirmed via
    // dtk-extracted retail asm for fn_82520830): the two bools below sit at
    // 0x74/0x75, not 0x70/0x71 as DC3's (newer/pruned) layout has it. This
    // 4-byte filler restores the RB3-only field DC3 no longer carries here.
    unsigned int unk70; // 0x70
    bool unk74; // 0x74
    bool unk75; // 0x75
    void *mEnumHandles[kNumberOfBuffers]; // 0x78
    XCONTENT_CROSS_TITLE_DATA mXDatas[kNumberOfBuffers]; // 0x94
    XOVERLAPPED *mOverlappeds[kNumberOfBuffers]; // 0x91c
    int unk938; // 0x938
    int unk93c; // 0x93c
    bool mEnumerateSaveGameExports; // 0x940
};

extern XboxContentMgr gContentMgr;
