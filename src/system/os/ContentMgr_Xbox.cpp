#include "os/ContentMgr_Xbox.h"
#include "ContentMgr.h"
#include "ContentMgr_Xbox.h"
#include "meta/ConnectionStatusPanel.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "os/System.h"
#include "xdk/XAPILIB.h"
#include "xdk/win_types.h"

// Forward declarations for XContent functions
extern "C" {
    long XContentCreateCrossTitleEnumerator(int, void*, int, int, int, int, void*);
    unsigned long XEnumerateCrossTitle(void*, void*, int, int, void*);
}

// Retail's ignored-content list is a static table of 8 C strings in .rdata
// (lbl_82089578), walked with an /Oi-inlined strcmp in PollRefresh -- NOT
// DC3's std::vector<String> filled from SystemConfig (see the note on Init).
static const char *gIgnoredContent[] = { "rbsongcache", "rb2songcache", "band",
                                         "band3",       "netcache",     "Song Export",
                                         "globaloptions", "rbdxcache" };
XboxContentMgr gContentMgr;
const char *kContentRootFormat = "cnt%08x";

XboxContent::XboxContent(const XCONTENT_CROSS_TITLE_DATA &data, int i2, int i3, bool b4)
    : mOverlapped(0), mLicenseBits(0), mValidLicenseBits(0),
      mRoot(MakeString(kContentRootFormat, i2)), mContentPath(MakeString("%s:", mRoot.c_str())),
      mState(kUnmounted), mPadNum(i3), mPendingDelete(0), mLRM(0) {
    MILO_ASSERT(mRoot.size() < kContentRootMaxLength, 0x6F);
    MILO_ASSERT(mPadNum < kNumberOfBuffers, 0x70);
    mXData = data;
    char filename[XCONTENT_MAX_FILENAME_LENGTH + 1];
    memcpy(filename, mXData.szFileName, XCONTENT_MAX_FILENAME_LENGTH);
    filename[XCONTENT_MAX_FILENAME_LENGTH] = 0;
    mFilename = filename;
    mState = (State)(b4 != 0);
}

XboxContent::~XboxContent() {
    switch (mState) {
    case 0:
    case 1:
    case 3:
    case 5:
    case 6:
    case 8:
        break;
    case kMounting:
    case kMounted:
        XContentClose(mRoot.c_str(), nullptr);
        break;
    default:
        int state = mState;
        MILO_LOG("Unknown state: %d", state);
        break;
    }
}

ContentLocT XboxContent::Location() {
    XDEVICE_DATA deviceData;
    XContentGetDeviceData(mXData.DeviceID, &deviceData);
    if (deviceData.DeviceType == 2) {
        return kLocationRemovableMem;
    } else {
        if (deviceData.DeviceType != 1) {
            MILO_NOTIFY(
                "Unknown device type: %d - defaulting to HDD", deviceData.DeviceType
            );
        }
        return kLocationHDD;
    }
}

void XboxContent::Poll() {
    if (mState == 1) {
        Symbol name = FileName();
        MILO_LOG("Mounting content '%s'\n", name);
        int pad = mPadNum;
        if (pad == 4 || pad == 5) {
            pad = 0xFF;
        }
        mOverlapped = new XOVERLAPPED;
        memset(mOverlapped, 0, sizeof(XOVERLAPPED));
        // MEASURED NEGATIVE (W16-FI): retail materialises this 8-byte argument on
        // the stack (`stw 0,0x60(r1)`, `stw 0,0x64(r1)`, `ld r10,0x60(r1)`) while
        // `QuadPart = 0` keeps it in a register.  Assigning HighPart/LowPart
        // separately to force the stack form scored WORSE, not better --
        // 87.764 -> 85.189 fuzzy -- so the stack spill is driven by something
        // other than the shape of this initialisation.  Left as QuadPart.
        ULARGE_INTEGER contentSize;
        contentSize.QuadPart = 0;
        if (XContentCrossTitleCreate(
                pad,
                mRoot.c_str(),
                &mXData,
                3,
                nullptr,
                &mLicenseBits,
                0,
                contentSize,
                mOverlapped
            )
            != 0x3E5) {
            RELEASE(mOverlapped);
            mState = kContentDeleting;
        } else {
            mState = kMounting;
        }
    }
    // Retail (0x8251fdc8) RELEASEs mOverlapped BEFORE branching on the result,
    // not after, and its failure arm is a bare `mState = kContentDeleting`.
    // DC3 additionally calls XGetOverlappedExtendedError and records
    // `mCorrupt = err == 0x570`; retail emits neither -- consistent with the
    // note on IsCorrupt above, which found NOT ONE access to 0x169 anywhere in
    // this TU including Poll.  That block was DC3-era and is dropped.
    if (mState == 2 || mState == 3) {
        DWORD res = XGetOverlappedResult(mOverlapped, nullptr, false);
        if (res == 0x3E4)
            return;
        RELEASE(mOverlapped);
        if (res == 0) {
            mValidLicenseBits = true;
            mState = mState == kMounting ? kMounted : kUnmounted;
            if (mPendingDelete) {
                Delete();
            }
        } else {
            mState = kContentDeleting;
        }
    }
    if (mState == 6) {
        DWORD res = XGetOverlappedResult(mOverlapped, nullptr, false);
        if (res != 0x3E4) {
            RELEASE(mOverlapped);
            // `xori r11,r11,1 ; addi r11,r11,7` -- kBackingUp(7) on success and
            // kContentDeleting(8) on failure, NOT the 0/1 this used to compute.
            // The guard above is mState == kNeedsBackup(6), so the backup states
            // are the semantically right pair too.
            mState = res == 0 ? kBackingUp : kContentDeleting;
        }
    }
}

void XboxContent::Mount() {
    if (mState == kUnmounted) {
        mState = kNeedsMounting;
        static unsigned int count = 0;
        count++;
        mLRM = count;
    }
}

void XboxContent::Unmount() {
    if (mState == kMounted) {
        mOverlapped = new XOVERLAPPED;
        memset(mOverlapped, 0, sizeof(XOVERLAPPED));
        if (XContentClose(mRoot.c_str(), mOverlapped) != 0x3E5) {
            RELEASE(mOverlapped);
            mState = kContentDeleting;
            return;
        }
        mState = kUnmounting;
    } else if (mState == kNeedsMounting || mState == kContentDeleting) {
        mState = kUnmounted;
    }
}

void XboxContent::Delete() {
    mPendingDelete = true;
    if (mState == 4 || mState == 1) {
        Unmount();
    } else if (mState == 0) {
        mOverlapped = new XOVERLAPPED;
        memset(mOverlapped, 0, sizeof(XOVERLAPPED));
        if (XContentCrossTitleDelete(0xFF, &mXData, mOverlapped) != 0x3E5) {
            RELEASE(mOverlapped);
            mState = kContentDeleting;
        } else {
            mState = kNeedsBackup;
        }
    }
}

BEGIN_HANDLERS(XboxContentMgr)
    HANDLE_MESSAGE(SigninChangedMsg)
    HANDLE_MESSAGE(StorageChangedMsg)
    HANDLE_MESSAGE(ContentInstalledMsg)
    HANDLE_SUPERCLASS(ContentMgr)
END_HANDLERS

// Retail's Init (0x82521a30, 120 B) is the clear-loop, AddSink and the base
// call -- and NOTHING else.  DC3's copy additionally reads SystemConfig
// ("content_mgr"), FindData()s `enumerate_save_game_exports` and fills
// gIgnoredContent from `ignored_content`; retail's 120 bytes contain no
// SystemConfig/FindData/FindArray/push_back call at all (those 50
// instructions were pure base-side insert), so that block is DC3-era and is
// dropped here rather than transcribed.
void XboxContentMgr::Init() {
    unk7f8 = 0;
    unk7fc = 0;
    for (int i = 0; i < kNumberOfBuffers; i++) {
        mOverlappeds[i] = nullptr;
    }
    ThePlatformMgr.AddSink(this);
    ContentMgr::Init();
}

void XboxContentMgr::Terminate() { ThePlatformMgr.RemoveSink(this); }

// Retail 0x82520fd0.  Two structural differences from DC3, both read off the
// retail bytes rather than transcribed:
//   1. the whole body is gated on the 0x70 bool (`lbz r11,0x70(r3) / cmplwi /
//      beq <epilogue>` before mDirty at 0x44 is ever touched);
//   2. the enumerator dispatch has THREE arms, not DC3's four -- there is no
//      i == 6 arm (kNumberOfBuffers is 6 here, so i == 6 is unreachable) and no
//      `mEnumerateSaveGameExports` test on i == 5.  DC3 emits
//      `lbz r11,0x800(r30)` for that test; retail emits nothing there, which
//      agrees with retail's Init never assigning the member.
void XboxContentMgr::StartRefresh() {
    if (unk70) {
        bool b10 = mDirty || (unk74 && unk75);

        if (b10) {
            mDirty = false;
            unk74 = false;
            unk75 = false;
            if (mState == 2) {
                for (int i = 0; i < kNumberOfBuffers; i++) {
                    if (mOverlappeds[i]) {
                        XCancelOverlapped(mOverlappeds[i]);
                        RELEASE(mOverlappeds[i]);
                        CloseHandle(mEnumHandles[i]);
                    }
                }
            } else if (mState != 1 && mState != 0) {
                RELEASE(mLoader);
                mCallbackFiles.clear();
            }
            FOREACH (it, mContents) {
                if ((*it)->GetState() == 4) {
                    NotifyUnmounted(*it);
                }
            }
            DeleteAll(mContents);
            unk7fc = 0;
            mRootLoaded = 0;
            DataArray *cfg = SystemConfig("content_mgr", "roots");
            for (int i = 1; i < cfg->Size(); i++) {
                mContents.push_back(new RootContent(cfg->Str(i)));
                mRootLoaded++;
            }
            FOREACH (it, mExtraContents) {
                mContents.push_back(new RootContent(it->c_str()));
                mRootLoaded++;
            }
            mState = kDiscoveryMounting;
            FOREACH (it, mCallbacks) {
                (*it)->ContentStarted();
            }
            for (int i = 0; i < kNumberOfBuffers; i++) {
                if (i >= 4 || ThePlatformMgr.IsSignedIn(i)) {
                    DWORD result;
                    if (i == 4) {
                        result = XContentCreateCrossTitleEnumerator(
                            0xff, 0, 2, 0, 1, 0, &mEnumHandles[4]
                        );
                    } else if (i == 5) {
                        result = XContentCreateCrossTitleEnumerator(
                            0xff, 0, 1, 0, 1, 0, &mEnumHandles[5]
                        );
                    } else {
                        result = XContentCreateCrossTitleEnumerator(
                            i, 0, 2, 0, 1, 0, &mEnumHandles[i]
                        );
                    }
                    if (result == 0) {
                        mOverlappeds[i] = new XOVERLAPPED;
                        memset(mOverlappeds[i], 0, sizeof(XOVERLAPPED));
                        if (XEnumerateCrossTitle(
                                mEnumHandles[i], &mXDatas[i], 0x138, 0, mOverlappeds[i]
                            )
                            != 0x3E5) {
                            RELEASE(mOverlappeds[i]);
                            CloseHandle(mEnumHandles[i]);
                        }
                    }
                }
            }
        }
    }
}

bool XboxContentMgr::IsMounted(Symbol name) {
    bool ret = false;
    FOREACH (it, mContents) {
        if (name == (*it)->FileName()) {
            ret = (*it)->GetState() == Content::kMounted;
            break;
        }
    }
    return ret;
}

// Retail body, read off 0x82520668.  Two things our DC3-derived version did
// that RB3 retail demonstrably does NOT do:
//   - no null check on the dynamic_cast: retail does `bl __RTDynamicCast`
//     and then `lwz r11,0x0(r3)` on the result with no intervening compare,
//     so the source dereferences it unconditionally;
//   - no displayName write: the whole body contains exactly TWO vcalls,
//     slot 11 (FileName) and slot 14 (XboxContent::IsCorrupt).  There is no
//     slot-12 (DisplayName) call, and r5 -- the displayName reference -- is
//     never read or written anywhere in the 140 bytes.
// DC3 has `ret = (*it)->IsCorrupt(); displayName = (*it)->DisplayName();`
// and needs no cast because DC3 moved IsCorrupt onto the Content base.  RB3
// is the OLDER form: IsCorrupt lives only on XboxContent, hence the cast.
bool XboxContentMgr::IsCorrupt(Symbol contentName) {
    FOREACH (it, mContents) {
        if (contentName == (*it)->FileName()) {
            return dynamic_cast<XboxContent *>(*it)->IsCorrupt();
        }
    }
    return false;
}

bool XboxContentMgr::DeleteContent(Symbol contentName) {
    bool notFound = true;
    FOREACH (it, mContents) {
        if (contentName == (*it)->FileName()) {
            notFound = false;
            (*it)->Delete();
            mState = kContentMgrState6;
            mDirty = true;
            break;
        }
    }
    if (notFound) {
        MILO_NOTIFY("\"%s\" not found to delete.", contentName.Str());
    }
    return notFound;
}

bool XboxContentMgr::IsDeleteDone(Symbol contentName) {
    bool ret = false;
    FOREACH (it, mContents) {
        Content *cur = *it;
        if (contentName == cur->FileName()) {
            Content::State state = cur->GetState();
            ret = state == 7 || state == 8;
            break;
        }
    }
    return ret;
}

bool XboxContentMgr::GetLicenseBits(Symbol contentName, unsigned long &licenseBits) {
    FOREACH (it, mContents) {
        Content *cur = *it;
        if (contentName == cur->FileName()) {
            licenseBits = cur->LicenseBits();
            return cur->HasValidLicenseBits();
        }
    }
    return false;
}

void XboxContentMgr::NotifyMounted(Content *c) {
    XboxContent *xc = dynamic_cast<XboxContent *>(c);
    MILO_ASSERT(xc, 0x2C1);
    FOREACH (it, mCallbacks) {
        (*it)->ContentMounted(xc->FileName().Str(), xc->Root());
    }
}

void XboxContentMgr::NotifyUnmounted(Content *c) {
    XboxContent *xc = dynamic_cast<XboxContent *>(c);
    MILO_ASSERT(xc, 0x2CB);
    FOREACH (it, mCallbacks) {
        (*it)->ContentUnmounted(xc->FileName().Str());
    }
}

void XboxContentMgr::NotifyDeleted(Content *c) {
    XboxContent *xc = dynamic_cast<XboxContent *>(c);
    MILO_ASSERT(xc, 0x2D5);
}

void XboxContentMgr::NotifyFailed(Content *c) {
    XboxContent *xc = dynamic_cast<XboxContent *>(c);
    MILO_ASSERT(xc, 0x2E0);
    if (!RefreshDone()) {
        unk75 = true;
    }
    FOREACH (it, mCallbacks) {
        (*it)->ContentFailed(xc->FileName().Str());
    }
}

DataNode XboxContentMgr::OnMsg(const SigninChangedMsg &msg) {
    for (int i = 0; i < 4; i++) {
        unsigned int changedMask = (unsigned int)msg.GetChangedMask() >> i;
        if (changedMask & 1) {
            if (ThePlatformMgr.IsSignedIn(i)) {
                unk74 = true;
            }
        }
    }
    return 0;
}

DataNode XboxContentMgr::OnMsg(const ConnectionStatusChangedMsg &msg) {
    if (msg.Connected()) {
        unk74 = true;
    }
    return 0;
}

DataNode XboxContentMgr::OnMsg(const StorageChangedMsg &msg) {
    mDirty = true;
    return 0;
}

DataNode XboxContentMgr::OnMsg(const ContentInstalledMsg &msg) {
    mDirty = true;
    return 0;
}

bool XboxContentMgr::MountContent(Symbol name) {
    bool alreadyMounted = false;
    bool found = false;
    FOREACH (it, mContents) {
        if (name == (*it)->FileName()) {
            found = true;
            (*it)->Mount();
            mState = kContentMgrState7;
            if ((*it)->GetState() == Content::kMounted) {
                alreadyMounted = true;
            }
            break;
        }
    }
    if (!found) {
        MILO_NOTIFY("\"%s\" not found to mount.", name.Str());
    }
    int mountingCount = 0;
    int prevCount = 0;
    bool done = false;
    do {
        Content *oldest = nullptr;
        unsigned int oldestLRM = 0xFFFFFFFF;
        FOREACH (it, mContents) {
            Content::State state = (*it)->GetState();
            if (state == Content::kMounted || state == Content::kMounting
                || state == Content::kNeedsMounting) {
                mountingCount++;
                if (name != (*it)->FileName()
                    && (*it)->GetLRM() < oldestLRM
                    && (*it)->GetState() != Content::kMounting) {
                    oldest = *it;
                    oldestLRM = oldest->GetLRM();
                }
            }
        }
        if (mountingCount == prevCount) {
            done = true;
        } else if (mountingCount > 6 && oldest) {
            oldest->Unmount();
            mState = kContentMgrState7;
        }
        prevCount = mountingCount;
        mountingCount = 0;
    } while (!done);
    return alreadyMounted;
}

void XboxContentMgr::PollRefresh() {
    if (mState == kDiscoveryMounting) {
        mState = kDiscoveryLoading;
        for (int i = 0; i < kNumberOfBuffers; i++) {
            if (mOverlappeds[i]) {
                DWORD numItems = 0;
                DWORD res = XGetOverlappedResult(mOverlappeds[i], &numItems, false);
                if (res == 0x3E4) {
                    // retail: sets the state and jumps straight to the
                    // epilogue -- no ContentMountBegun, no base PollRefresh.
                    mState = kDiscoveryMounting;
                    return;
                }
                if (res == 0) {
                    for (unsigned int j = 0; j < numItems; j++) {
                        XCONTENT_CROSS_TITLE_DATA *xdata =
                            (XCONTENT_CROSS_TITLE_DATA *)((char *)&mXDatas[i] + j * 0x138);
                        char *filename = xdata->szFileName;
                        bool ignored = false;
                        for (unsigned int k = 0; k < DIM(gIgnoredContent); k++) {
                            if (strcmp(filename, gIgnoredContent[k]) == 0) {
                                ignored = true;
                                break;
                            }
                        }
                        if (ignored)
                            continue;

                        bool discovered = false;
                        FOREACH (it, mCallbacks) {
                            discovered =
                                !(*it)->ContentDiscovered(Symbol(filename)) || discovered;
                        }
                        if (discovered) {
                            unk7fc++;
                        }
                        Content *newContent =
                            new XboxContent(*xdata, unk7f8++, i, discovered);
                        std::list<Content *>::iterator end = mContents.end();
                        mContents.insert(end, newContent);
                    }
                    memset(mOverlappeds[i], 0, 0x1c);
                    DWORD enumRes = XEnumerateCrossTitle(
                        mEnumHandles[i], &mXDatas[i], 0x138, 0, mOverlappeds[i]
                    );
                    if (enumRes == 0x3E5) {
                        mState = kDiscoveryMounting;
                        return;
                    }
                }
                operator delete(mOverlappeds[i]);
                mOverlappeds[i] = nullptr;
                CloseHandle(mEnumHandles[i]);
            }
        }
        FOREACH (it, mCallbacks) {
            (*it)->ContentMountBegun(unk7fc);
        }
    } else if (mState == kDiscoveryLoading) {
        int mountedCount = 0;
        FOREACH (it, mContents) {
            if ((*it)->GetState() == Content::kMounted) {
                mountedCount++;
            }
        }
        if (mountedCount >= 6) {
            mState = kDiscoveryCheckIfDone;
        }
    } else if (mState == kMounting) {
        bool allDone = true;
        FOREACH (it, mContents) {
            Content::State state = (*it)->GetState();
            if (state == Content::kMounted) {
                (*it)->Unmount();
                allDone = false;
            } else {
                allDone = (state != Content::kNeedsMounting) & allDone;
            }
        }
        if (allDone) {
            mState = kDiscoveryLoading;
        }
    }
    ContentMgr::PollRefresh();
}

// sw2 scatter-include (default/ContentMgr_Xbox <- char/CharPollGroup.cpp)
#define gRev gRev_CharPollGroup
#define gAltRev gAltRev_CharPollGroup
#include "char/CharPollGroup.cpp"
#undef gRev
#undef gAltRev
