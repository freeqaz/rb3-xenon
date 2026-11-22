// rb3-xenon native -- DTA script operations on string nodes, and the other
// places where the 8-byte native DataNode union is read as retail's 4-byte word.
//
// THE DEFECTS (lane strnode, docs/decomp/STRNODE_STRING_NODE_AUDIT_2026-11-22.md):
//   1. DataNode::Equal compared same-type pointer nodes (object, var, func,
//      array) with UncheckedInt(), the low 4 of 8 bytes. Retail operator==
//      (0x8274ABF0..0x8274AC00) compares the whole word, which on X360 is the
//      whole pointer. Two objects 4 GiB apart read equal natively.
//   2. has_substr returned `(int)strstr(...)`. Retail (0x82761C08) stores the
//      match pointer as the int; natively the cast keeps the low half, which is 0
//      for a match at a 4 GiB-aligned address, so a found substring read false.
//   3. DataNode::Load's int/float cases write 4 of the union's 8 bytes, and
//      DataArray::Load reads the node after #ifdef/#ifndef/#define/#else into the
//      same slot. An int 0 after a directive kept the directive symbol's high
//      half, so NotNull (retail 0x8274B178: one `lwz` word test) read it as true.
//
// THE STRING CASES are the operations that read a kDataString's characters
// through the storage DataArray (==, !=, find_elem, switch, has_substr, strieq).
// Lane dtafail fixed Equal's string branches; they are here as controls and as a
// guard on that fix. Every `control-` gate must pass with or without this lane's
// fixes; a control that fails means the model, not the fix, is wrong.
//
// The two object "pointers" in case 1 are addresses 4 GiB apart that are never
// dereferenced: a same-type compare reads only the union, and an object node's
// copy and destructor do not touch the object.

#include "obj/Data.h"
#include "obj/DataFile.h"
#include "utl/MemStream.h"

#include <sys/mman.h>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

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

    // Parses `text` (one command) and returns the value it evaluates to.
    DataNode Eval(const char *text) {
        DataArray *a = DataReadString(text);
        DataNode r = a->Node(0).Evaluate();
        a->Release();
        return r;
    }

    bool IsInt(const DataNode &n, int v) {
        return n.Type() == kDataInt && n.UncheckedInt() == v;
    }

    // `text` must evaluate to the int `want`.
    void IntCase(const char *name, const char *text, int want) {
        DataNode r = Eval(text);
        Gate(name, IsInt(r, want), "%s -> type %d value %d (want %d)", text,
             (int)r.Type(), r.Type() == kDataInt ? r.UncheckedInt() : -1, want);
    }

    // `text` must evaluate to a nonzero int (has_substr's "found" is a pointer
    // in retail, so only nonzero is the contract).
    void TrueCase(const char *name, const char *text, bool want) {
        DataNode r = Eval(text);
        const bool found = r.Type() == kDataInt && r.UncheckedInt() != 0;
        Gate(name, found == want, "%s -> type %d value %d (want %s)", text,
             (int)r.Type(), r.Type() == kDataInt ? r.UncheckedInt() : -1,
             want ? "nonzero" : "0");
    }

    void StringCases() {
        IntCase("control-strnode-eq-string-string", "{== \"abc\" \"abc\"}", 1);
        IntCase("control-strnode-eq-string-string-differ", "{== \"abc\" \"abd\"}", 0);
        IntCase("control-strnode-eq-symbol-string", "{== abc \"abc\"}", 1);
        IntCase("control-strnode-eq-empty-symbol-string", "{== '' \"\"}", 1);
        IntCase("control-strnode-ne-string-string", "{!= \"\" \"\"}", 0);
        IntCase("control-strnode-find-elem-string", "{find_elem (\"x\" \"abc\") \"abc\"}", 1);
        IntCase("control-strnode-switch-string", "{switch \"b\" (\"a\" 1) (\"b\" 2) 3}", 2);
        IntCase("control-strnode-strieq-string-symbol", "{strieq \"ABC\" abc}", 1);
        TrueCase("control-strnode-has-substr", "{has_substr \"haystack\" \"st\"}", true);
        TrueCase("control-strnode-has-substr-absent", "{has_substr \"haystack\" \"zz\"}", false);
    }

    // Case 2: a haystack whose match sits at a 4 GiB-aligned address.
    void HasSubstrCase() {
        char *const want = (char *)0x700000000000ULL; // low 32 bits are 0
        void *map = mmap(want - 0x1000, 0x2000, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (map != want - 0x1000) {
            Gate("strnode-has-substr-4gib-aligned-match", false,
                 "could not map the probe pages at %p (got %p)", want - 0x1000, map);
            return;
        }
        memcpy(want - 3, "abcneedle", 10);
        // A symbol node can carry any character pointer; Str(1) returns it as-is.
        DataVariable("strnode_hay") = DataNode(kDataSymbol, (const char *)(want - 3));
        DataNode r = Eval("{has_substr $strnode_hay \"needle\"}");
        const bool found = r.Type() == kDataInt && r.UncheckedInt() != 0;
        Gate("strnode-has-substr-4gib-aligned-match", found,
             "haystack \"abcneedle\" at %p, match at %p: has_substr -> %d (want nonzero; "
             "retail 0x82761C48 stores the match pointer)",
             want - 3, want, r.Type() == kDataInt ? r.UncheckedInt() : -1);
        DataVariable("strnode_hay") = DataNode(0);
        munmap(map, 0x2000);
    }

    // Case 1: same-type pointer nodes that differ only above bit 31.
    void PointerEqCases() {
        Hmx::Object *o1 = (Hmx::Object *)0x600000001000ULL;
        Hmx::Object *o2 = (Hmx::Object *)0x600100001000ULL; // o1 + 4 GiB
        DataVariable("strnode_o1") = DataNode(o1);
        DataVariable("strnode_o2") = DataNode(o2);
        DataArray *list = new DataArray(1);
        list->Node(0) = DataNode(o1);
        DataVariable("strnode_list") = DataNode(list, kDataArray);
        list->Release();

        IntCase("control-strnode-eq-same-object", "{== $strnode_o1 $strnode_o1}", 1);
        IntCase("strnode-eq-objects-4gib-apart", "{== $strnode_o1 $strnode_o2}", 0);
        IntCase("strnode-ne-objects-4gib-apart", "{!= $strnode_o1 $strnode_o2}", 1);
        IntCase("control-strnode-find-elem-same-object",
                "{find_elem $strnode_list $strnode_o1}", 1);
        IntCase("strnode-find-elem-object-4gib-apart",
                "{find_elem $strnode_list $strnode_o2}", 0);

        DataVariable("strnode_o1") = DataNode(0);
        DataVariable("strnode_o2") = DataNode(0);
        DataVariable("strnode_list") = DataNode(0);
    }

    // Serialises `n` nodes in DataArray::Load's layout and loads them back.
    struct RawNode {
        DataType type;
        const char *sym; // symbol-valued types
        int value;       // int-valued types
    };
    DataArray *LoadRaw(const RawNode *nodes, int n) {
        MemStream ms;
        ms << (short)n << (short)1 << (short)0;
        for (int i = 0; i < n; i++) {
            ms << (int)nodes[i].type;
            if (nodes[i].sym)
                ms << Symbol(nodes[i].sym);
            else
                ms << nodes[i].value;
        }
        ms.Seek(0, BinStream::kSeekBegin);
        DataArray *a = new DataArray(0);
        a->Load(ms);
        return a;
    }

    // Case 3: an int read into a slot a directive symbol used first.
    void LoadCase(const char *name, bool control, const RawNode *nodes, int n, int want) {
        DataArray *a = LoadRaw(nodes, n);
        const bool shape = a->Size() == 1 && a->Type(0) == kDataInt;
        const int v = shape ? a->Node(0).UncheckedInt() : -1;
        const uint64_t raw = shape ? (uint64_t)(uintptr_t)a->Node(0).UncheckedObj() : 0;
        const bool notNull = shape && a->Node(0).NotNull();
        DataVariable("strnode_loaded") = shape ? a->Node(0) : DataNode(0);
        DataNode bang = Eval("{! $strnode_loaded}");
        const bool ok = shape && v == want && notNull == (want != 0)
            && IsInt(bang, want == 0 ? 1 : 0);
        char full[96];
        snprintf(full, sizeof(full), "%s%s", control ? "control-" : "", name);
        Gate(full, ok,
             "loaded (%d) as %d node(s), union bytes 0x%016llx: NotNull %s, {! $v} -> %d "
             "(want NotNull %s; retail 0x8274B178 tests the one 4-byte word)",
             v, a->Size(), (unsigned long long)raw, notNull ? "true" : "false",
             bang.Type() == kDataInt ? bang.UncheckedInt() : -1,
             want ? "true" : "false");
        DataVariable("strnode_loaded") = DataNode(0);
        a->Release();
    }

    void LoadCases() {
        const RawNode plain[] = { { kDataInt, nullptr, 0 } };
        LoadCase("strnode-load-int-zero", true, plain, 1, 0);
        const RawNode ifndefSeven[] = { { kDataIfndef, "STRNODE_NEVER_DEFINED", 0 },
                                        { kDataInt, nullptr, 7 },
                                        { kDataEndif, nullptr, 0 } };
        LoadCase("strnode-load-ifndef-int-seven", true, ifndefSeven, 3, 7);
        const RawNode ifndefZero[] = { { kDataIfndef, "STRNODE_NEVER_DEFINED", 0 },
                                       { kDataInt, nullptr, 0 },
                                       { kDataEndif, nullptr, 0 } };
        LoadCase("strnode-load-ifndef-int-zero", false, ifndefZero, 3, 0);
    }

} // namespace

int RunStringNodePhase(GateFn gate) {
    gGate = gate;
    printf("\n=== string_node_phase: DTA string ops and native-width union reads ===\n");
    StringCases();
    HasSubstrCase();
    PointerEqCases();
    LoadCases();
    return 0;
}
