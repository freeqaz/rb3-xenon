// rb3-xenon native -- retail-CRT and LP64 behaviour the engine depends on
// (lane lp64, docs/decomp/NATIVE_LP64_CRT_TRIAGE_2026-11-22.md).
//
// THE DEFECTS, each gated through a retail consumer that rb3-milo links:
//   1. Hx_snprintf, the native stand-in for retail's _snprintf (0x8282D540),
//      returned glibc's C99 "would have written" length on truncation. Retail
//      returns -1, and an exact fit returns the count with no terminator.
//      FormatString::operator<< (0x827C40E8) subtracts the result from mBufSize.
//   2. strtok: retail's cursor lives in the per-thread data (0x8282D8BC
//      `bl _getptd`, cursor at ptd+0x18); glibc's is one per process. A worker
//      thread's DataNode::Print or FileRelativePath moved another thread's
//      cursor.
//   3. rand: retail's seed is per thread at ptd+0x14, 1 at thread start
//      (0x8283091C), stepped *214013 + 2531011, bits 16..30 returned
//      (0x8282E8A8..0x8282E8C4). PseudoRandomPicker picks with it.
//   4. atoi: retail is strtol base 10 (0x8282D9B8) with a 32-bit long, so an
//      out-of-range DTA int literal (ParseNode, atoi at 0x8276B7B0) or
//      `(int 'sym')` (DataInt, atoi at 0x8275F8E4) clamps to INT_MAX/INT_MIN.
//   5. DataArray::SortNodes passes qsort an element size of 8 (0x8274CCA4
//      `li r5,8`), sizeof(DataNode) on X360; a native DataNode is 16 bytes.
//   6. ThreadMemStack walked gThreadIds (int[6]) with an `unsigned long *`;
//      retail steps 4 bytes (lwz / addi r28,r28,4 at 0x827BB92C..0x827BB940).
//
// Every `control-` gate must pass with or without the fixes; a control that
// fails means the model, not the fix, is wrong. Cases that can crash before
// their fix (5 and 6; the symbol sort segfaulted) run in a forked child.

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/MemMgr.h"
#include "utl/PseudoRandomPicker.h"
#include "utl/Str.h"

#include <sys/wait.h>
#include <unistd.h>
#include <climits>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

typedef void (*GateFn)(const char *, bool, const char *);

namespace {

    GateFn gGate = nullptr;

    void Gate(const char *name, bool ok, const char *fmt, ...)
        __attribute__((format(printf, 3, 4)));
    void Gate(const char *name, bool ok, const char *fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        gGate(name, ok, buf);
    }

    // ---- 1. Hx_snprintf returns what retail's _snprintf returns ----
    void SnprintfCases() {
        char b[8];
        memset(b, 'X', sizeof(b));
        int n = Hx_snprintf(b, 4, "%s", "ab");
        Gate("control-snprintf-short-fit", n == 2 && strcmp(b, "ab") == 0,
             "\"ab\" into 4 -> %d \"%s\" (want 2 \"ab\")", n, b);

        memset(b, 'X', sizeof(b));
        n = Hx_snprintf(b, 4, "%s", "abcdef");
        Gate("lp64-snprintf-truncation-minus-one", n == -1 && b[3] == '\0' && b[4] == 'X',
             "\"abcdef\" into 4 -> %d, b[3]=0x%02x b[4]=0x%02x "
             "(want -1, terminated at 3, byte 4 untouched)",
             n, (unsigned char)b[3], (unsigned char)b[4]);

        memset(b, 'X', sizeof(b));
        n = Hx_snprintf(b, 3, "%s", "abc");
        Gate("lp64-snprintf-exact-fit", n == 3 && memcmp(b, "abc", 3) == 0 && b[3] == 'X',
             "\"abc\" into 3 -> %d, bytes %02x %02x %02x %02x "
             "(want 3, \"abc\" unterminated, byte 3 untouched)",
             n, (unsigned char)b[0], (unsigned char)b[1], (unsigned char)b[2],
             (unsigned char)b[3]);

        n = Hx_snprintf(b, 0, "%s", "abc");
        Gate("lp64-snprintf-zero-count", n == -1, "\"abc\" into 0 -> %d (want -1)", n);
        n = Hx_snprintf(b, 0, "%s", "");
        Gate("control-snprintf-zero-count-empty", n == 0, "\"\" into 0 -> %d (want 0)", n);
    }

    // ---- 2. strtok cursor is per thread ----
    // The main thread starts a tokenization, a worker runs `work`, then the
    // main thread continues. Retail's per-thread cursor is not moved by it.
    template <class F>
    void StrtokCase(const char *name, F work) {
        char buf[] = "x|y|z";
        const char *t1 = strtok(buf, "|");
        std::thread t(work);
        t.join();
        const char *t2 = strtok(nullptr, "|");
        Gate(name, t1 && t2 && strcmp(t2, "y") == 0,
             "main-thread strtok after a worker's call continued with \"%s\" (want \"y\")",
             t2 ? t2 : "(null)");
    }

    void StrtokCases() {
        // Control: DataNode::Print of a string with an embedded quote.
        String printed;
        {
            DataNode n("p\"q");
            n.Print(printed, false);
        }
        Gate("control-print-quoted-string", strcmp(printed.c_str(), "\"p\\qq\"") == 0,
             "Print(\"p\\\"q\") -> %s (want \"p\\qq\")", printed.c_str());
        // Control: no worker -- the main-thread sequence alone.
        StrtokCase("control-strtok-no-worker", [] {});
        StrtokCase("lp64-strtok-worker-print", [] {
            DataNode n("p\"q\"r");
            String s;
            n.Print(s, false);
        });
        StrtokCase("lp64-strtok-worker-file-relative-path", [] {
            FileRelativePath("/a/b/c", "/a/b/d/e");
        });
    }

    // FileGetPath(FileGetPath(x)) (BandSongMgr::AddSongData) hands
    // FileGetPath its own static buffer. Retail's inlined byte copy makes that
    // an identity copy; natively strcpy there is undefined (ASan reports
    // strcpy-param-overlap). Without a sanitizer glibc usually gives the same
    // bytes, so this is a control on the result; the fix is the memmove.
    void FileGetPathCases() {
        const char *p = FileGetPath(FileGetPath("songs/foo/foo.mid"));
        Gate("control-file-get-path-nested", strcmp(p, "songs") == 0,
             "FileGetPath(FileGetPath(\"songs/foo/foo.mid\")) -> \"%s\" (want \"songs\")",
             p);
    }

    // ---- 3. rand: retail's per-thread LCG, seed 1 at thread start ----
    // Reference values from the retail formula (0x8282E8A8..0x8282E8C4).
    std::vector<int> RetailRandFromSeed1(int count) {
        std::vector<int> v;
        unsigned int seed = 1;
        for (int i = 0; i < count; i++) {
            seed = seed * 214013u + 2531011u;
            v.push_back((int)((seed >> 16) & 0x7FFF));
        }
        return v;
    }

    // Mode 3: idx = rand() % Size(); GetItem moves the pick to the end.
    std::vector<int> PickerRun(int picks) {
        PseudoRandomPicker<int> p;
        p.SetMode(3);
        for (int i = 0; i < 7; i++)
            p.AddItem(i);
        std::vector<int> out;
        for (int i = 0; i < picks; i++)
            out.push_back(p.GetNext());
        return out;
    }

    std::vector<int> PickerExpected(int picks) {
        std::vector<int> items;
        for (int i = 0; i < 7; i++)
            items.push_back(i);
        std::vector<int> r = RetailRandFromSeed1(picks), out;
        for (int i = 0; i < picks; i++) {
            int idx = r[i] % 7;
            int v = items[idx];
            items.erase(items.begin() + idx);
            items.push_back(v);
            out.push_back(v);
        }
        return out;
    }

    void RandCases() {
        std::vector<int> ref = RetailRandFromSeed1(3);
        Gate("control-rand-reference-sequence",
             ref[0] == 41 && ref[1] == 18467 && ref[2] == 6334,
             "retail formula from seed 1 -> %d %d %d (want 41 18467 6334)", ref[0],
             ref[1], ref[2]);

        const int kPicks = 6;
        std::vector<int> want = PickerExpected(kPicks);
        std::vector<int> a, b;
        std::thread ta([&] { a = PickerRun(kPicks); });
        ta.join();
        std::thread tb([&] { b = PickerRun(kPicks); });
        tb.join();
        char sa[64], sw[64];
        snprintf(sa, sizeof(sa), "%d %d %d %d %d %d", a[0], a[1], a[2], a[3], a[4], a[5]);
        snprintf(sw, sizeof(sw), "%d %d %d %d %d %d", want[0], want[1], want[2], want[3],
                 want[4], want[5]);
        Gate("lp64-rand-picker-fresh-thread", a == want,
             "PseudoRandomPicker mode 3 on a fresh thread picked %s (want %s)", sa, sw);
        Gate("lp64-rand-per-thread-seed", a == b,
             "a second fresh thread %s the first thread's picks (want the same)",
             a == b ? "repeated" : "did not repeat");
    }

    // ---- 4. atoi clamps like a 32-bit strtol ----
    int LiteralInt(const char *text) {
        DataArray *a = DataReadString(text);
        int v = a->Array(0)->Int(0);
        a->Release();
        return v;
    }

    int EvalInt(const char *text) {
        DataArray *a = DataReadString(text);
        DataNode r = a->Node(0).Evaluate();
        a->Release();
        return r.Type() == kDataInt ? r.UncheckedInt() : -1;
    }

    void AtoiCases() {
        int v = LiteralInt("(123)");
        Gate("control-atoi-literal-in-range", v == 123, "(123) -> %d", v);
        v = LiteralInt("(-5)");
        Gate("control-atoi-literal-negative", v == -5, "(-5) -> %d", v);
        v = LiteralInt("(2147483647)");
        Gate("control-atoi-literal-int-max", v == INT_MAX, "(2147483647) -> %d", v);
        v = LiteralInt("(3000000000)");
        Gate("lp64-atoi-literal-clamps-high", v == INT_MAX,
             "(3000000000) -> %d (want %d)", v, INT_MAX);
        v = LiteralInt("(-3000000000)");
        Gate("lp64-atoi-literal-clamps-low", v == INT_MIN,
             "(-3000000000) -> %d (want %d)", v, INT_MIN);
        v = EvalInt("{int '42'}");
        Gate("control-atoi-int-symbol", v == 42, "{int '42'} -> %d", v);
        v = EvalInt("{int '5000000000'}");
        Gate("lp64-atoi-int-symbol-clamps", v == INT_MAX,
             "{int '5000000000'} -> %d (want %d)", v, INT_MAX);
    }

    // Runs `body` in a forked child (an alarm bounds it) and gates on its exit
    // code: 0 passes. A case that can crash before its fix runs here, so a
    // crash fails one gate instead of ending the phase. `body` prints its own
    // detail line.
    template <class F>
    void ForkGate(const char *name, F body) {
        fflush(stdout);
        fflush(stderr);
        pid_t pid = fork();
        if (pid == 0) {
            alarm(20);
            int code = body();
            fflush(stdout);
            _exit(code);
        }
        int status = 0;
        waitpid(pid, &status, 0);
        const bool exited = WIFEXITED(status);
        Gate(name, exited && WEXITSTATUS(status) == 0, "forked child %s %d",
             exited ? "exited" : "died on signal",
             exited ? WEXITSTATUS(status) : WTERMSIG(status));
    }

    // ---- 5. SortNodes sorts whole DataNodes ----
    int SortChild(const char *text, const char *want) {
        DataArray *a = DataReadString(text);
        DataArray *arr = a->Array(0);
        arr->SortNodes();
        String got;
        for (int i = 0; i < arr->Size(); i++) {
            if (i)
                got += " ";
            DataType t = arr->Node(i).Type();
            if (t == kDataInt)
                got += MakeString("%d", arr->Node(i).UncheckedInt());
            else if (t == kDataSymbol)
                got += arr->Node(i).UncheckedStr();
            else
                got += MakeString("<type %d>", (int)t);
        }
        printf("    sort %s -> %s (want %s)\n", text, got.c_str(), want);
        return strcmp(got.c_str(), want) == 0 ? 0 : 1;
    }

    void SortCases() {
        ForkGate("control-sort-single", [] { return SortChild("(7)", "7"); });
        ForkGate("lp64-sort-ints", [] { return SortChild("(3 1 2)", "1 2 3"); });
        ForkGate("lp64-sort-symbols", [] { return SortChild("(c a b d)", "a b c d"); });
    }

    // ---- 6. ThreadMemStack finds a second thread again ----
    // Child: worker W registers, the main thread looks itself up, W looks
    // itself up again. Exit 0 when W got the same stack both times and the
    // main thread's differs.
    int MemStackChild() {
        std::mutex m;
        std::condition_variable cv;
        int step = 0;
        MemHeapStack *w1 = nullptr, *w2 = nullptr;
        std::thread w([&] {
            w1 = &ThreadMemStack(true);
            {
                std::lock_guard<std::mutex> l(m);
                step = 1;
            }
            cv.notify_all();
            std::unique_lock<std::mutex> l(m);
            cv.wait(l, [&] { return step == 2; });
            l.unlock();
            w2 = &ThreadMemStack(true);
        });
        MemHeapStack *mainStack;
        {
            std::unique_lock<std::mutex> l(m);
            cv.wait(l, [&] { return step == 1; });
        }
        mainStack = &ThreadMemStack(true);
        {
            std::lock_guard<std::mutex> l(m);
            step = 2;
        }
        cv.notify_all();
        w.join();
        if (w1 != w2)
            return 2;
        if (w1 == mainStack)
            return 3;
        return 0;
    }

    void MemStackCase() {
        // Child exit: 0 = W got the same stack both times; 2 = W re-registered;
        // 3 = W shared the main thread's stack; a signal = crashed.
        ForkGate("lp64-memstack-second-thread-found-again", [] {
            int code = MemStackChild();
            printf("    ThreadMemStack: worker/main/worker -> code %d\n", code);
            return code;
        });
    }

} // namespace

int RunLp64CrtPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== lp64_crt_phase: retail CRT and LP64 behaviour ===\n");
    SnprintfCases();
    StrtokCases();
    FileGetPathCases();
    RandCases();
    AtoiCases();
    SortCases();
    MemStackCase();
    return 0;
}
