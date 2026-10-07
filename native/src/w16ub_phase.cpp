// rb3-xenon native -- W16-UB: sizes and offsets written as X360 literals.
//
// W16-TY found KerningTable clearing a literal 0x80 bytes of a 32-pointer table,
// which on a 64-bit host left half its bucket heads uninitialised. W16-UB
// audited every source the native targets compile for the same class (literal
// memset/memcpy lengths, struct offsets, pointer arithmetic by constant, stream
// widths, and the byte-order cousins of those) and fixed each real one in an
// HX_NATIVE arm. This phase checks the fixes whose effect can be observed
// without a live Game, panel or GPU draw. The rest are recorded with a reason
// in docs/decomp/W16UB_LP64_LITERAL_AUDIT_2026-10-07.md.
//
// Each gate's reference is independent of the code under test: the members the
// literal was meant to cover (named, so the compiler sizes them), the heap's own
// invariants (every block keeps its header; freeing everything restores the one
// initial free block), and a payload built and read back through the engine's
// own serialiser and compressor. Gates that could crash on a broken body run in
// a forked child, so a fault fails that gate instead of the whole run.

#include "midi/MidiParser.h"
#include "midi/MidiParserMgr.h"
#include "math/Mtx.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "os/CDReader.h"
#include "rndobj/Bitmap.h"
#include "rndobj/VelocityBuffer.h"
#include "ui/UILabel.h"
#include "utl/BufStream.h"
#include "utl/Compress.h"
#include "utl/MemHeap.h"
#include "utl/MemTrack.h"
#include "utl/MemTracker.h"
#include "utl/TextStream.h"
#include "xdk/XAPILIB.h"

extern MemTracker *gMemTracker; // defined in utl/MemTrack.cpp, not declared in a header

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <string>
#include <vector>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

DataArray *LoadDtz(const char *, int); // obj/DataFile.cpp; not declared in a header

typedef void (*GateFn)(const char *, bool, const char *);

namespace {

GateFn gGate = nullptr;
char gBuf[1024];
int gRan = 0;

void Gate(const char *name, bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Gate(const char *name, bool ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gBuf, sizeof(gBuf), fmt, ap);
    va_end(ap);
    gGate(name, ok, gBuf);
    gRan++;
    fflush(stdout);
}

// Runs fn in a forked child with a deadline. The child prints its own detail
// line and exits 0 on pass; any other exit, a signal or the deadline fails the
// gate, and the parent reports which.
struct ChildResult {
    bool ok;
    std::string how;
};
ChildResult RunInChild(const std::function<bool(std::string &)> &fn, unsigned seconds) {
    fflush(stdout);
    fflush(stderr);
    int fds[2];
    if (pipe(fds) != 0)
        return {false, "pipe failed"};
    pid_t pid = fork();
    if (pid < 0)
        return {false, "fork failed"};
    if (pid == 0) {
        close(fds[0]);
        alarm(seconds);
        std::string detail;
        bool ok = fn(detail);
        (void)!write(fds[1], detail.data(), detail.size());
        close(fds[1]);
        fflush(stdout);
        fflush(stderr);
        _exit(ok ? 0 : 1);
    }
    close(fds[1]);
    std::string detail;
    char buf[512];
    ssize_t n;
    while ((n = read(fds[0], buf, sizeof(buf))) > 0)
        detail.append(buf, n);
    close(fds[0]);
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status))
        return {WEXITSTATUS(status) == 0, detail.empty() ? "no detail" : detail};
    if (WIFSIGNALED(status)) {
        char s[96];
        snprintf(s, sizeof(s), "child killed by signal %d (%s)%s", WTERMSIG(status),
                 strsignal(WTERMSIG(status)), WTERMSIG(status) == SIGALRM ? ", deadline" : "");
        return {false, s};
    }
    return {false, "child ended abnormally"};
}

// ---- RndVelocityBuffer ctor: the clear must reach the end of mCam ----------
void GateVelocityBuffer() {
    const size_t sz = sizeof(RndVelocityBuffer);
    void *mem = malloc(sz);
    memset(mem, 0xA5, sz);
    RndVelocityBuffer *vb = new (mem) RndVelocityBuffer();
    const unsigned char *p = (const unsigned char *)&vb->mViewProjXfm;
    const size_t span = (const unsigned char *)(&vb->mCam + 1) - p;
    int dirty = 0;
    for (size_t i = 0; i < span; i++)
        dirty += p[i] != 0;
    const bool camNull = vb->mCam == nullptr;
    Gate("ub-velbuf-ctor", dirty == 0 && camNull,
         "mViewProjXfm..mCam is %zu bytes here (0xa4 on X360); %d left non-zero over 0xA5 fill, "
         "mCam %s",
         span, dirty, camNull ? "null" : "NOT null");
    vb->~RndVelocityBuffer();
    free(mem);
}

struct CollectStream : public TextStream {
    std::string text;
    virtual void Print(const char *str) { text += str; }
};

int CountOf(const std::string &hay, const char *needle) {
    int n = 0;
    for (size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + 1))
        n++;
    return n;
}

// ---- MemHeap: a freed minimum-size block must hold a whole FreeBlock -------
bool HeapChild(std::string &detail) {
    const int kBytes = 64 * 1024;
    int *buf = (int *)aligned_alloc(16, kBytes);
    memset(buf, 0, kBytes);
    MemHeap h;
    h.Init("w16ub", 0, buf, kBytes / 4, false, MemHeap::kFirstFit, 1, false);
    const int initialFree = h.mFreeBlockChain ? (int)h.mFreeBlockChain->mSizeWords : -1;
    const int sizeWords = MemHeap::GetSizeWords(4); // the minimum block
    const int alignWords = MemHeap::GetAlignWords(4);
    const int kBlocks = 256;
    std::vector<int *> blocks;
    std::vector<int> sizes;
    for (int i = 0; i < kBlocks; i++) {
        int got = 0;
        int *b = h.Alloc(sizeWords, alignWords, got);
        if (!b) {
            detail = "Alloc returned null";
            return false;
        }
        *b = 0x55000000 | i;
        blocks.push_back(b);
        sizes.push_back(h.AllocSize(b));
    }
    // Free every other block: each freed block sits between two live ones, so
    // it cannot merge and keeps exactly the minimum size.
    for (int i = 0; i < kBlocks; i += 2)
        h.Free(blocks[i]);
    int badHeader = 0, badWord = 0;
    for (int i = 1; i < kBlocks; i += 2) {
        badHeader += h.AllocSize(blocks[i]) != sizes[i];
        badWord += *blocks[i] != (0x55000000 | i);
    }
    int chain = 0;
    bool chainOk = true;
    for (FreeBlock *f = h.mFreeBlockChain; f; f = f->mNextBlock) {
        if ((int *)f < h.Start() || (int *)f >= h.End() || ++chain > kBlocks + 4) {
            chainOk = false;
            break;
        }
    }
    // Print walks the same chain through its own reads of mNextBlock.
    CollectStream dump;
    h.Print(dump, true);
    const int printedFree = CountOf(dump.text, " FREE ");
    for (int i = 1; i < kBlocks; i += 2)
        h.Free(blocks[i]);
    const int finalFree = h.mFreeBlockChain ? (int)h.mFreeBlockChain->mSizeWords : -1;
    const bool single = h.mFreeBlockChain && !h.mFreeBlockChain->mNextBlock;
    char s[400];
    snprintf(s, sizeof(s),
             "sizeof(FreeBlock)=%zu, min block %d words; after freeing every other one of %d: "
             "%d live headers changed, %d live words changed, free chain %s (%d nodes), "
             "Print listed %d free blocks; after freeing all: %s, %d of %d words free",
             sizeof(FreeBlock), sizeWords, kBlocks, badHeader, badWord,
             chainOk ? "in bounds" : "LEFT THE HEAP", chain, printedFree,
             single ? "one block" : "MORE THAN ONE BLOCK", finalFree, initialFree);
    detail = s;
    free(buf);
    return sizeWords * 4 >= (int)sizeof(FreeBlock) && badHeader == 0 && badWord == 0 && chainOk
        && chain == kBlocks / 2 + 1 && printedFree == chain && single && finalFree == initialFree;
}

// ---- LoadDtz: the size trailer is little-endian -----------------------------
bool DtzChild(std::string &detail) {
    // Grow a string until the serialised size's low byte is >= 0x80, so the
    // big-endian reading of the trailer is negative and cannot pass by luck.
    std::vector<char> raw(8192);
    int rawLen = 0;
    DataArray *src = nullptr;
    for (int pad = 0; pad < 300; pad++) {
        std::string text = "(w16ub_dtz (a 1) (b \"two\") 3.5 \"" + std::string(pad, 'x') + "\")";
        if (src)
            src->Release();
        src = DataReadString(text.c_str());
        BufStream out(raw.data(), raw.size(), true);
        out << src;
        rawLen = out.Tell();
        if ((rawLen & 0xFF) >= 0x80)
            break;
    }
    std::vector<char> comp(raw.size() + 64);
    int compLen = (int)comp.size() - 4;
    CompressMem(raw.data(), rawLen, comp.data(), compLen, nullptr);
    for (int i = 0; i < 4; i++)
        comp[compLen + i] = (char)((unsigned)rawLen >> (8 * i)); // little-endian trailer
    DataArray *got = LoadDtz(comp.data(), compLen + 4);
    std::vector<char> back(raw.size());
    int backLen = -1;
    if (got) {
        BufStream out(back.data(), back.size(), true);
        out << got;
        backLen = out.Tell();
    }
    const bool same = got && backLen == rawLen && memcmp(back.data(), raw.data(), rawLen) == 0;
    char s[300];
    snprintf(s, sizeof(s),
             "payload %d bytes (low byte 0x%02x), %d compressed; LoadDtz %s, re-serialised %d bytes, %s",
             rawLen, rawLen & 0xFF, compLen, got ? "returned an array" : "returned null", backLen,
             same ? "identical" : "DIFFERENT");
    detail = s;
    return same;
}

// ---- RndBitmap::LoadBmp: bottom-up rows land top row first ---------------
void Le16(std::vector<unsigned char> &v, unsigned x) {
    v.push_back(x & 0xFF);
    v.push_back((x >> 8) & 0xFF);
}
void Le32(std::vector<unsigned char> &v, unsigned x) {
    Le16(v, x & 0xFFFF);
    Le16(v, x >> 16);
}
bool BmpChild(std::string &detail) {
    const int w = 3, hgt = 4, bpp = 32, row = w * 4;
    // Pixel (x, y) with y counted from the top: bytes {x, y, 0x40|x, 0x80|y}.
    std::vector<unsigned char> f;
    Le16(f, 0x4D42); // "BM"
    Le32(f, 14 + 40 + row * hgt);
    Le16(f, 0);
    Le16(f, 0);
    Le32(f, 14 + 40); // bfOffBits
    Le32(f, 40); // biSize
    Le32(f, w);
    Le32(f, hgt); // positive: rows stored bottom-up
    Le16(f, 1);
    Le16(f, bpp);
    Le32(f, 0); // BI_RGB
    Le32(f, row * hgt);
    Le32(f, 0xB11); // biXPelsPerMeter: the loader's "leave alpha alone" marker
    Le32(f, 0);
    Le32(f, 0);
    Le32(f, 0);
    for (int fileRow = 0; fileRow < hgt; fileRow++) {
        int y = hgt - 1 - fileRow;
        for (int x = 0; x < w; x++) {
            f.push_back(x);
            f.push_back(y);
            f.push_back(0x40 | x);
            f.push_back(0x80 | y);
        }
    }
    BufStream bs(f.data(), f.size(), true);
    RndBitmap bm;
    if (!bm.LoadBmp(&bs)) {
        detail = "LoadBmp returned false";
        return false;
    }
    int wrong = 0;
    for (int y = 0; y < bm.Height() && y < hgt; y++)
        for (int x = 0; x < w; x++) {
            const unsigned char *p = bm.Pixels() + y * bm.RowBytes() + x * 4;
            wrong += p[0] != x || p[1] != y || p[2] != (0x40 | x) || p[3] != (0x80 | y);
        }
    char s[200];
    snprintf(s, sizeof(s), "%dx%d %d bpp bottom-up BMP loaded as %dx%d, %d of %d pixels misplaced",
             w, hgt, bpp, bm.Width(), bm.Height(), wrong, w * hgt);
    detail = s;
    return bm.Width() == w && bm.Height() == hgt && wrong == 0;
}

// ---- CDReadExternal: the handle is positioned at the 64-bit offset ---------
bool CdChild(std::string &detail) {
    const u64 kOffset = 0x12345;
    void *h = nullptr;
    if (!CDReadExternal(h, 0, kOffset)) {
        detail = "CDReadExternal returned false";
        return false;
    }
    DWORD pos = SetFilePointer(h, 0, nullptr, 1 /* FILE_CURRENT */);
    char s[200];
    snprintf(s, sizeof(s), "ark 0 handle positioned at 0x%lx, expected 0x%llx", (unsigned long)pos,
             (unsigned long long)kOffset);
    detail = s;
    return pos == kOffset;
}

// ---- MemTrack name stacks: 65 pointer slots each ----------------------------
bool MemTrackChild(std::string &detail) {
    MemTrackInit(0, 64, false);
    if (!gMemTracker) {
        detail = "MemTrackInit left gMemTracker null";
        return false;
    }
    char names[65][16];
    for (int i = 1; i <= 64; i++) {
        snprintf(names[i], sizeof(names[i]), "w16ub_f%02d", i);
        BeginMemTrackFileName(names[i]);
    }
    // Each Begin pushed the previous name; each End pops back to it.
    int wrong = 0;
    for (int i = 64; i >= 1; i--) {
        EndMemTrackFileName();
        const char *want = i > 1 ? names[i - 1] : "";
        wrong += strcmp(gMemTracker->unk181ac.c_str(), want) != 0;
    }
    char s[160];
    snprintf(s, sizeof(s), "64 nested file names pushed and popped, %d pops restored the wrong name",
             wrong);
    detail = s;
    return wrong == 0;
}

void ChildGate(const char *name, const std::function<bool(std::string &)> &fn) {
    ChildResult r = RunInChild(fn, 60);
    Gate(name, r.ok, "%s", r.how.c_str());
}

} // namespace

// The X360 offsets the raw-offset sites used. These name different members on
// this host, which is why the fixed sites name the member instead.
void W16UBLayout() {
    const size_t pbe = offsetof(MidiParserMgr, mPlaybackEnabled);
    const size_t evt = offsetof(MidiParser, mEvents);
    const size_t alpha = offsetof(UILabel, mAlpha);
    const size_t slice = offsetof(MemTracker, mTimeSlice);
    const bool ok = pbe != 0x69 && evt != 0x18 && alpha != 0x1BC && slice == 0x10
        && sizeof(Hmx::Matrix3) < 0x40;
    Gate("ub-layout", ok,
         "MidiParserMgr::mPlaybackEnabled 0x%zx (X360 0x69), MidiParser::mEvents 0x%zx (0x18), "
         "UILabel::mAlpha 0x%zx (0x1BC), MemTracker::mTimeSlice 0x%zx (AllocInfo's native arm "
         "reads 0x10), sizeof(Hmx::Matrix3) 0x%zx (Flare copied 0x40)",
         pbe, evt, alpha, slice, sizeof(Hmx::Matrix3));
}

int RunW16UBPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== W16-UB: X360 literal sizes and offsets ===\n");
    fflush(stdout);
    W16UBLayout();
    GateVelocityBuffer();
    ChildGate("ub-memheap-minblock", HeapChild);
    ChildGate("ub-loaddtz-trailer", DtzChild);
    ChildGate("ub-loadbmp-rows", BmpChild);
    ChildGate("ub-cdreadexternal-seek", CdChild);
    ChildGate("ub-memtrack-stacks", MemTrackChild);
    printf("W16-UB: %d gates run\n", gRan);
    fflush(stdout);
    return gRan;
}
