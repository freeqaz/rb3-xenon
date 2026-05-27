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

    const char *AllocType(unsigned long p1) {
        bool isPhys = (p1 & 0x80000000) != 0;
        unsigned int type = p1 >> 0x10 & 0xff;

        switch (type) {
        case 0:
            if (isPhys) {
                if (gPhysicalType != gNullStr) {
                    return gPhysicalType;
                }
                return "XTL(phys):D3D";
            }
            return "XTL:D3D";
        case 1:
            if (!isPhys) {
                return "XTL:D3DX";
            }
            return "XTL(phys):D3DX";
        case 2:
            if (!isPhys) {
                return "XTL:XAUDIO";
            }
            return "XTL(phys):XAUDIO";
        case 3:
            if (!isPhys) {
                return "XTL:XAPI";
            }
            return "XTL(phys):XAPI";
        case 4:
            if (!isPhys) {
                return "XTL:XACT";
            }
            return "XTL(phys):XACT";
        case 5:
            if (!isPhys) {
                return "XTL:XBOXKERNEL";
            }
            return "XTL(phys):XBOXKERNEL";
        case 6:
            if (!isPhys) {
                return "XTL:XBDM";
            }
            return "XTL(phys):XBDM";
        case 7:
            if (!isPhys) {
                return "XTL:XGRAPHICS";
            }
            return "XTL(phys):XGRAPHICS";
        case 8:
            if (!isPhys) {
                return "XTL:XONLINE";
            }
            return "XTL(phys):XONLINE";
        case 9:
            if (!isPhys) {
                return "XTL:XVOICE";
            }
            return "XTL(phys):XVOICE";
        case 10:
            if (!isPhys) {
                return "XTL:XHV";
            }
            return "XTL(phys):XHV";
        case 0xb:
            if (!isPhys) {
                return "XTL:USB";
            }
            return "XTL(phys):USB";
        case 0xc:
            if (!isPhys) {
                return "XTL:XMV";
            }
            return "XTL(phys):XMV";
        case 0xd:
            if (!isPhys) {
                return "XTL:SHADERCOMPILER";
            }
            return "XTL(phys):SHADERCOMPILER";
        case 0xe:
            if (!isPhys) {
                return "XTL:XUI";
            }
            return "XTL(phys):XUI";
        case 0xf:
            if (!isPhys) {
                return "XTL:XASYNC";
            }
            return "XTL(phys):XASYNC";
        case 0x10:
            if (!isPhys) {
                return "XTL:XCAM";
            }
            return "XTL(phys):XCAM";
        case 0x11:
            if (!isPhys) {
                return "XTL:XVIS";
            }
            return "XTL(phys):XVIS";
        case 0x12:
            if (!isPhys) {
                return "XTL:XIME";
            }
            return "XTL(phys):XIME";
        case 0x13:
            if (!isPhys) {
                return "XTL:XFILECACHE";
            }
            return "XTL(phys):XFILECACHE";
        case 0x14:
            if (!isPhys) {
                return "XTL:XRN";
            }
            return "XTL(phys):XRN";
        case 0x15:
            if (!isPhys) {
                return "XTL:XMCORE";
            }
            return "XTL(phys):XMCORE";
        case 0x16:
            if (!isPhys) {
                return "XTL:XMASSIVE";
            }
            return "XTL(phys):XMASSIVE";
        case 0x17:
            if (!isPhys) {
                return "XTL:XAUDIO2";
            }
            return "XTL(phys):XAUDIO2";
        case 0x18:
            if (!isPhys) {
                return "XTL:XAVATAR";
            }
            return "XTL(phys):XAVATAR";
        case 0x19:
            if (!isPhys) {
                return "XTL:XLSP";
            }
            return "XTL(phys):XLSP";
        case 0x1a:
            if (!isPhys) {
                return "XTL:D3DAlloc";
            }
            return "XTL(phys):D3DAlloc";
        case 0x1b:
            if (!isPhys) {
                return "XTL:NUISPEECH";
            }
            return "XTL(phys):NUISPEECH";
        case 0x1c:
            if (!isPhys) {
                return "XTL:NuiApi";
            }
            return "XTL(phys):NuiApi";
        case 0x1d:
            if (!isPhys) {
                return "XTL:NuiIdentity";
            }
            return "XTL(phys):NuiIdentity";

        case 0x3e:
            if (!isPhys) {
                return "XTL:NuiApi_LargePageReadWrite";
            }
            return "XTL(phys):NuiApi_LargePageReadWrite";
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

    int AllocAlign(unsigned long attrs) {
        unsigned int alignField = (attrs >> 24) & 0xf;
        if (attrs & 0x80000000) {
            // Physical allocation alignment
            switch (alignField) {
            case 0: return 4;
            case 1: return 0x20;
            case 2: return 0x40;
            case 3: return 0x80;
            case 4: return 0x100;
            case 5: return 0x200;
            case 6: return 0x400;
            case 7: return 0x800;
            case 8: return 0x1000;
            case 9: return 0x2000;
            case 10: return 0x4000;
            case 11: return 0x8000;
            case 12:
            case 13:
            case 14:
            case 15:
            default:
                MILO_FAIL("Invalid physical alignment (%d)", alignField);
                return 0;
            }
        } else {
            // Heap allocation alignment
            if (alignField < 1) {
                return 0x10;
            }
            if (alignField < 3) {
                return 8;
            }
            if (alignField != 4) {
                MILO_FAIL("Invalid heap alignment (%d)", alignField);
                return 0;
            }
            return 0x10;
        }
    }

    void MemAllocFailed(unsigned long size, bool physical) {
        MEMORYSTATUS memStatus;
        GlobalMemoryStatus(&memStatus);
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

        char buf[0x800];
        Hx_snprintf(buf, 0x800, "Allocation failure, \"%s\", want %d, have %d, total phys %d",
            allocType, size, memStatus.dwAvailPhys, gPhysicalUsage);
        MemPrintOverview(kNoHeap, buf + strlen(buf));
        MILO_FAIL(buf);
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
        const char *type = AllocType(attrs);
        ptr = _MemAllocTemp(size, __FILE__, 0x107, type, align);

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
        MemTrackAlloc(size, allocSize, AllocType(attrs), ptr, false, 0, __FILE__, 0xf0);
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

void *PhysicalAlloc(int size) {
    void *ptr = XPhysicalAlloc(size, -1, 0, 4);
    if (ptr) {
        auto _tmp0 = XPhysicalSize(ptr);
        gPhysicalUsage += _tmp0;
    } else {
        if (size != 0) {
            MemAllocFailed(size, true);
        }
    }
    return ptr;
}

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

void PhysicalFree(void *address) {
    if (address != 0) {
        gPhysicalUsage -= XPhysicalSize(address);
    }

    XPhysicalFree(address);
}

void PhysicalFreeTracked(void *address, const char *p2, int p3, const char *p4) {
    if (address != 0) {
        gPhysicalUsage -= XPhysicalSize(address);
    }

    XPhysicalFree(address);
    MemTrackFree(address);
}

int PhysicalUsage() { return gPhysicalUsage; }
