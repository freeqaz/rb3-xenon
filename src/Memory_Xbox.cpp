#include "Memory.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/MemTracker.h"
#include "xdk/xapilibi/winbase.h"
#include "xdk/xapilibi/xbox.h"
#include <cstdio>
#include <cstring>

extern "C" {
    void *XMemAllocDefault(unsigned long size, unsigned long attrs);
    void XMemFreeDefault(void *ptr, unsigned long attrs);
    int XMemSizeDefault(void *ptr, unsigned long attrs);
}

extern MemTracker *gMemTracker;
void MemDeltaFullReport();

namespace {
    int gPhysicalUsage;
    char *gPhysicalType = (char *)gNullStr;

    // The allocator id is the eXALLOCATOR_ID byte (attrs >> 16): 0x00-0x7F are
    // game ids, 0x80-0xBF the XTL's own (eXALLOCATOR_ID_D3D = 0x80 ...), 0xC0-0xFF
    // middleware. Retail switches on `id - 0x80` through a 27-entry byte table
    // (0x80-0x9A); ids 0x95 and 0x96 have no case and fall to the default, and
    // there are no Kinect-era cases (NUISPEECH/NuiApi/NuiIdentity, XMCORE,
    // XMASSIVE): none of those strings exists in retail, while "XTL:XLSP",
    // "XTL:D3DAlloc" and "XTL:Unknown" each occur once.
    const char *AllocType(unsigned long p1) {
        // Signed test: retail keeps the masked bit and normalises it with
        // addic/subfe; an unsigned `!= 0` folds to a single srwi instead.
        bool isPhys = (int)(p1 & 0x80000000) != 0;
        unsigned int type = p1 >> 0x10 & 0xff;

        switch (type) {
        case 0x80:
            if (isPhys) {
                if (gPhysicalType != gNullStr) {
                    return gPhysicalType;
                }
                return "XTL(phys):D3D";
            }
            return "XTL:D3D";
        case 0x81:
            if (!isPhys) {
                return "XTL:D3DX";
            }
            return "XTL(phys):D3DX";
        case 0x82:
            if (!isPhys) {
                return "XTL:XAUDIO";
            }
            return "XTL(phys):XAUDIO";
        case 0x83:
            if (!isPhys) {
                return "XTL:XAPI";
            }
            return "XTL(phys):XAPI";
        case 0x84:
            if (!isPhys) {
                return "XTL:XACT";
            }
            return "XTL(phys):XACT";
        case 0x85:
            if (!isPhys) {
                return "XTL:XBOXKERNEL";
            }
            return "XTL(phys):XBOXKERNEL";
        case 0x86:
            if (!isPhys) {
                return "XTL:XBDM";
            }
            return "XTL(phys):XBDM";
        case 0x87:
            if (!isPhys) {
                return "XTL:XGRAPHICS";
            }
            return "XTL(phys):XGRAPHICS";
        case 0x88:
            if (!isPhys) {
                return "XTL:XONLINE";
            }
            return "XTL(phys):XONLINE";
        case 0x89:
            if (!isPhys) {
                return "XTL:XVOICE";
            }
            return "XTL(phys):XVOICE";
        case 0x8a:
            if (!isPhys) {
                return "XTL:XHV";
            }
            return "XTL(phys):XHV";
        case 0x8b:
            if (!isPhys) {
                return "XTL:USB";
            }
            return "XTL(phys):USB";
        case 0x8c:
            if (!isPhys) {
                return "XTL:XMV";
            }
            return "XTL(phys):XMV";
        case 0x8d:
            if (!isPhys) {
                return "XTL:SHADERCOMPILER";
            }
            return "XTL(phys):SHADERCOMPILER";
        case 0x8e:
            if (!isPhys) {
                return "XTL:XUI";
            }
            return "XTL(phys):XUI";
        case 0x8f:
            if (!isPhys) {
                return "XTL:XASYNC";
            }
            return "XTL(phys):XASYNC";
        case 0x90:
            if (!isPhys) {
                return "XTL:XCAM";
            }
            return "XTL(phys):XCAM";
        case 0x91:
            if (!isPhys) {
                return "XTL:XVIS";
            }
            return "XTL(phys):XVIS";
        case 0x92:
            if (!isPhys) {
                return "XTL:XIME";
            }
            return "XTL(phys):XIME";
        case 0x93:
            if (!isPhys) {
                return "XTL:XFILECACHE";
            }
            return "XTL(phys):XFILECACHE";
        case 0x94:
            if (!isPhys) {
                return "XTL:XRN";
            }
            return "XTL(phys):XRN";
        case 0x97:
            if (!isPhys) {
                return "XTL:XAUDIO2";
            }
            return "XTL(phys):XAUDIO2";
        case 0x98:
            if (!isPhys) {
                return "XTL:XAVATAR";
            }
            return "XTL(phys):XAVATAR";
        case 0x99:
            if (!isPhys) {
                return "XTL:XLSP";
            }
            return "XTL(phys):XLSP";
        case 0x9a:
            if (!isPhys) {
                return "XTL:D3DAlloc";
            }
            return "XTL(phys):D3DAlloc";
        default:
            if (type <= 0x7f) {
                if (!isPhys) {
                    return "XTL:Game";
                }
                return "XTL(phys):Game";
            }
            if (type >= 0xc0) {
                if (!isPhys) {
                    return "XTL:Middleware";
                }
                return "XTL(phys):Middleware";
            }
            return "XTL:Unknown";
        }
    }

    // The alignment nibble is (attrs >> 24) & 0xF. For a physical allocation it
    // uses the XALLOC_PHYSICAL_ALIGNMENT_* encoding: 0 is the 4 KB default, 1 is
    // unused, and 2..15 are 4 bytes .. 32 KB (1 << n). For a heap allocation it
    // uses XALLOC_ALIGNMENT_*: 0 (default) and 4 (16) give 16, 1 and 2 (4, 8)
    // give 8.
    int AllocAlign(unsigned long attrs) {
        unsigned int alignField = (attrs >> 24) & 0xf;
        if (attrs & 0x80000000) {
            switch (alignField) {
            case 2: return 4;
            case 3: return 8;
            case 4: return 0x10;
            case 5: return 0x20;
            case 6: return 0x40;
            case 7: return 0x80;
            case 8: return 0x100;
            case 9: return 0x200;
            case 10: return 0x400;
            case 11: return 0x800;
            case 0:
            case 12: return 0x1000;
            case 13: return 0x2000;
            case 14: return 0x4000;
            case 15: return 0x8000;
            default:
                MILO_FAIL("Invalid physical alignment (%d)", alignField);
                return 0;
            }
        } else {
            switch (alignField) {
            case 0: return 0x10;
            case 1:
            case 2: return 8;
            case 4: return 0x10;
            default:
                MILO_FAIL("Invalid heap alignment (%d)", alignField);
                return 0;
            }
        }
    }

    // Retail/match: retail's failure path is ONLY the GlobalMemoryStatus call.
    // Inside retail's XMemAlloc (fn_822735B0) the whole branch is two
    // instructions -- `addi r3, r1, 0x50 ; bl fn_8283C980` -- with NO argument
    // setup, and retail's frame is 0x90 where ours is 0x70: the +0x20 delta is
    // exactly sizeof(MEMORYSTATUS), i.e. the local was inlined into XMemAlloc's
    // own frame. `size` and `physical` are therefore unused in retail, which is
    // why retail sets up no arguments for the call.
    //
    // Everything below the GlobalMemoryStatus call is dev-build code retail
    // never compiled. SETTLED ON RETAIL BYTES (this was an open question in
    // docs/decomp/handoff/allocator-xmemalloc-audit-2026-08-17.md §4.5, "where
    // retail's MemAllocFailed lives is UNRESOLVED"): a band.exe string scan
    // finds NONE of this function's distinguishing literals --
    // "want %d, have %d", "total phys", "out_of_mem_alloc_info.csv", "devkit:"
    // -- while the CONTROLS in the same .rdata neighbourhood all fire
    // ("XTL:D3DX", "XTL(phys):Middleware", "POOL REPORT"), so the scan is
    // capable of finding strings here and the absences are real.
    //
    // ⚠ The near-miss that had to be ruled out by hand: band.exe DOES contain
    // "Allocation failure, " at 0x117460, which looks like this function's
    // format string. Dumping the bytes shows it is
    // 'Allocation failure, heap "%s", want %d bytes\n   lFrags=...' -- MemHeap's
    // report, a coincidental shared prefix. A prefix probe was too weak here;
    // only reading the whole string settled it.
    //
    // Retail also has no failure branch at ALL in PhysicalAlloc /
    // PhysicalAllocTracked: retail's fn_82273350 runs XPhysicalAlloc ->
    // XPhysicalSize -> MemTrackAlloc with no null test in between.
    void MemAllocFailed(unsigned long size, bool physical) {
        MEMORYSTATUS memStatus;
        GlobalMemoryStatus(&memStatus);
#ifdef HX_NATIVE
        MemDeltaFullReport();

        if (gMemTracker && !gMemTracker->GetHeapOnly()) {
            FILE *file = fopen("devkit:\\out_of_mem_alloc_info.csv", "w");
            if (file) {
                MemTracker::SpitAllocInfo((TextStream *)file);
                fclose(file);
            }
        }

        const char *allocType;
        if (physical) {
            allocType = "physical";
        } else {
            allocType = "XMV";
        }

        String msg(MakeString(
            "Allocation failure, \"%s\", want %d, have %d, total phys %d",
            allocType, size, memStatus.dwAvailPhys, gPhysicalUsage
        ));
        MemPrintOverview(kNoHeap, msg);
        MILO_FAIL(msg.c_str());
#else
        (void)size;
        (void)physical;
#endif
    }
}

int ForceLinkXMemFuncs() {
    return 42;
}

VOID *XMemAlloc(SIZE_T size, DWORD attrs) {
    void *ptr;
    if (!(attrs & 0x80000000) && (attrs & 0x00FF0000) != 0x008C0000) {
        // Not physical and not the special attribute
        MILO_ASSERT((attrs & 0x30000000) != 0x20000000, 0xf9);

        int align = AllocAlign(attrs);
#ifdef HX_NATIVE
        const char *type = AllocType(attrs);
        ptr = MemAlloc(size, __FILE__, 0x107, type, align);
#else
        // Retail/match: retail's heap branch is `bl AllocAlign` at +0x30 then
        // `bl ?MemAlloc@@YAPAXHH@Z` at +0x3c -- the PERSISTENT allocator, not
        // the temp one. Verified on retail bytes: band.exe file offset 0x2683EC
        // holds 48 54 97 4D, which decodes as bl -> 0x827BCD38
        // (?MemAlloc@@YAPAXHH@Z); 0x827BCFF0 (?_MemAllocTemp@@YAPAXHH@Z) does
        // not appear anywhere in the 204-byte body.
        //
        // XMemAlloc is the XDK's GLOBAL allocation hook, so calling the temp
        // allocator here routed every non-physical XDK allocation -- D3D, D3DX,
        // XAudio, XAPI, XACT, XGRAPHICS, XUI, XMV -- through the temp heap's
        // MemHeap::kLastFit (top-down) placement instead of the default
        // bottom-up one. That is a behavioural bug, not naming noise.
        //
        // align is genuinely non-zero here (AllocAlign returns 0x10 or 8), so
        // the parenthesized form is REQUIRED to bypass MemMgr.h's align-0-
        // forcing macro. House pattern: src/system/synth/Mic.cpp:38.
        //
        // No AllocType on this path: retail's single AllocType call is at +0xa8,
        // in the physical branch below. Our compiled COMDAT already had exactly
        // one AllocType relocation (at +0xb4, the physical branch) because MSVC
        // dead-code-eliminated the heap-branch call once the macro swallowed
        // `type` -- so dropping the local here is a SOURCE-HONESTY change worth
        // exactly ZERO bytes, and must not be sold as part of the fix.
        ptr = (MemAlloc)(size, align);
#endif

        // Assert allocation succeeded if zero-init requested
        if (attrs & 0x00004000) {
            MILO_ASSERT(ptr, 0x10d);
        }

        // Zero-init if requested
        if ((attrs & 0x40000000) && ptr) {
            memset(ptr, 0, size);
        }
    } else {
        // Physical or special: use default XDK allocator
        ptr = XMemAllocDefault(size, attrs);
        if (!ptr) {
            MemAllocFailed(size, (bool)(attrs & 0x80000000));
        }
        int allocSize = XMemSizeDefault(ptr, attrs);
        gPhysicalUsage += allocSize;
#ifdef HX_NATIVE
        MemTrackAlloc(size, allocSize, AllocType(attrs), ptr, false, 0, __FILE__, 0xf0);
#else
        // Retail passes r3..r8 only -- no __FILE__, no line. See MemMgr.h.
        MemTrackAlloc(size, allocSize, AllocType(attrs), ptr, false, 0);
#endif
    }

    return ptr;
}

VOID XMemFree(LPVOID ptr, DWORD attrs) {
    if (!(attrs & 0x80000000) && (attrs & 0x00FF0000) != 0x008C0000) {
        MemFree(ptr, "unknown", 0, "unknown");
    } else {
        if (ptr != 0) {
            int allocSize = XMemSizeDefault(ptr, attrs);
            gPhysicalUsage -= allocSize;
        }
        MemTrackFree(ptr);
        XMemFreeDefault(ptr, attrs);
    }
}

INT XMemSize(LPVOID ptr, DWORD attrs) {
    if (!(attrs & 0x80000000) && (attrs & 0x00FF0000) != 0x008C0000) {
        return MemAllocSize(ptr);
    }
    return XMemSizeDefault(ptr, attrs);
}

PhysMemTypeTracker::PhysMemTypeTracker(Symbol name) {
    if (gPhysicalType == gNullStr) {
        gPhysicalType = (char *)name.Str();
        mActive = true;
    } else {
        mActive = false;
    }
}

PhysMemTypeTracker::~PhysMemTypeTracker() {
    if (mActive) {
        gPhysicalType = (char *)gNullStr;
    }
}

// Retail 0x82273420 (80 bytes), reached only from MemAlloc's `heap == -2`
// branch (fn_827BCD38: `cmpwi r11,-2` -> `mr r3,size; bl 0x82273420`), is
// XPhysicalAlloc(size, -1, 0, 4) -> XPhysicalSize(ptr) -> gPhysicalUsage +=,
// with NO null test and no MemAllocFailed call: the dev-build failure branch
// was never compiled into retail. Callee identities come from the named
// neighbours in the same TU: fn_8283C8D8 has PhysicalAllocTracked's
// (-1, 0, align) argument shape, and fn_8283C960 feeds PhysicalFree's
// `gPhysicalUsage -=`. The failure branch is kept for the native build only.
void *PhysicalAlloc(int size) {
    void *ptr = XPhysicalAlloc(size, -1, 0, 4);
#ifdef HX_NATIVE
    if (!ptr) {
        if (size != 0) {
            MemAllocFailed(size, true);
        }
        return ptr;
    }
#endif
    gPhysicalUsage += XPhysicalSize(ptr);
    return ptr;
}

// Retail 0x82273350 (100 bytes) is PhysicalAllocTracked, although the map
// used to name it ??1CXLrcTransport@@QAA@XZ (a BinDiff structural hit; DC3's
// ~CXLrcTransport is a 16-byte `vptr = ...; b Close`). Its three retail
// callers -- 0x82734C50 (0x404, "Tex(phys)"), 0x82735074 (LiveCameraInput's
// "Tex(phys)") and 0x82B6AC44 ("XMABuffer(phys)") -- set up r3/r4/r5 only, and
// the body forwards the incoming r5 to MemTrackAlloc as its name, so retail's
// signature is (size, alignment, name): the file/line pair is dev-only, like
// MemTrackAlloc's (see MemMgr.h). The body is XPhysicalAlloc(size, -1, 0,
// alignment) -> XPhysicalSize(ptr) -> gPhysicalUsage += -> MemTrackAlloc, with
// NO null test and no MemAllocFailed call. The failure branch and file/line are
// kept for the native build only.
#ifdef HX_NATIVE
void *PhysicalAllocTracked(unsigned long size, unsigned long alignment, const char *file, int line, const char *name) {
    int allocSize = 0;
    void *ptr = XPhysicalAlloc(size, -1, 0, alignment);
    if (ptr) {
        allocSize = XPhysicalSize(ptr);
        gPhysicalUsage += allocSize;
    } else {
        if (size > 0) {
            MemAllocFailed(size, true);
        }
    }
    MemTrackAlloc(size, allocSize, name, ptr, false, 0, file, line);
    return ptr;
}
#else
void *PhysicalAllocTracked(unsigned long size, unsigned long alignment, const char *name) {
    void *ptr = XPhysicalAlloc(size, -1, 0, alignment);
    int allocSize = XPhysicalSize(ptr);
    gPhysicalUsage += allocSize;
    MemTrackAlloc(size, allocSize, name, ptr, false, 0);
    return ptr;
}
#endif

// Retail has two physical frees. 0x82273470 (68 bytes) is PhysicalFree: no
// null test and no MemTrackFree. Its only caller is MemFree, which has already
// tested the pointer and calls MemTrackFree itself after the heap walk (DC3's
// MemFree calls PhysicalFree the same way).
void PhysicalFree(void *address) {
    gPhysicalUsage -= XPhysicalSize(address);
    XPhysicalFree(address);
}

// 0x822733B8 (84 bytes) is PhysicalFreeTracked: null test, XPhysicalSize,
// XPhysicalFree, MemTrackFree. DC3's DxRnd and SynthSample call it as
// PhysicalFreeTracked(p, __FILE__, __LINE__, ""); retail's callers set only r3,
// so the match build takes the pointer alone.
#ifdef HX_NATIVE
void PhysicalFreeTracked(void *address, const char *, int, const char *) {
#else
void PhysicalFreeTracked(void *address) {
#endif
    if (address != 0) {
        gPhysicalUsage -= XPhysicalSize(address);
    }

    XPhysicalFree(address);
    MemTrackFree(address);
}

int PhysicalUsage() { return gPhysicalUsage; }

// sw2 scatter-include (default/Memory_Xbox <- bandobj/BandCrowdMeter.cpp)
#define gRev gRev_BandCrowdMeter
#define gAltRev gAltRev_BandCrowdMeter
#include "bandobj/BandCrowdMeter.cpp"
#undef gRev
#undef gAltRev
