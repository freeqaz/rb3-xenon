// rb3-xenon native -- CharClip transitions hold their clips by reference.
//
// THE DEFECT. Retail CharClip::Transitions ref-counts every clip it names
// against its owning clip: AddNode / Load AddRef the clip (0x8275BD08), Clear
// and RemoveClip (fn_8237D3C8) Release it (0x8275B378), and when a named clip
// dies, ~Object's ReplaceRefs hands the owner Replace(dying, 0), which
// CharClip::Replace (fn_8237D810) forwards to Transitions::Replace
// (fn_8237D470): a NULL replacement drops the clip's node vector, a CharClip
// replacement retargets it. The native arm registered nothing, so deleting a
// clip left every transition table naming it holding a dangling pointer (Save
// then calls Name() on it, CharDriver's FindNodes compares against it).
//
// THE GATES. Each builds clip A with a transition to clip B, then:
//   * ring-holds-ref       B's ref ring names A after AddNode (retail AddRef).
//   * delete-target        a plain `delete B` (ReplaceRefs(nullptr) path):
//                          A's table must be empty afterwards.
//   * cascade-target       B dies in its dir's ~ObjectDir cascade
//                          (NullifyAllRefs path): A's table must be empty.
//   * replace-target       B->ReplaceRefs(C): A's node vector names C, not B,
//                          and C's ring holds A (retail's CharClip branch).
//   * delete-owner         `delete A` releases A's ref from B's ring (retail
//                          Clear); a control, it passes with or without the fix.
//   * cascade-both         A and B in one dir, the dir deleted: survives (an
//                          ASan smoke case, no assertion of its own).

#include "char/CharClip.h"
#include "obj/Dir.h"
#include "obj/Object.h"

#include <cstdarg>
#include <cstdio>

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

    // Refs in `target`'s ring whose owner is `owner`.
    int RefsOwnedBy(const Hmx::Object *target, const Hmx::Object *owner) {
        int n = 0;
        const ObjRefList &refs = target->Refs();
        for (ObjRefList::const_iterator it = refs.begin(); it != refs.end(); ++it) {
            if (it->RefOwner() == owner)
                n++;
        }
        return n;
    }

    CharClip *NewClip() { return Hmx::Object::New<CharClip>(); }

    void Link(CharClip *from, CharClip *to) {
        CharGraphNode a = { 1.0f, 2.0f };
        CharGraphNode b = { 3.0f, 4.0f };
        from->GetTransitions().AddNode(to, a);
        from->GetTransitions().AddNode(to, b); // same vector: one ref, two nodes
    }

    void RingCase() {
        CharClip *a = NewClip();
        CharClip *b = NewClip();
        Link(a, b);
        const int refs = RefsOwnedBy(b, a);
        CharClip::NodeVector *v = a->GetTransitions().FindNodes(b);
        Gate("charclip-ring-holds-ref", refs == 1 && v && v->size == 2,
             "after AddNode x2: B's ring holds %d ref(s) owned by A (retail: 1), "
             "A's vector for B has %d node(s)",
             refs, v ? v->size : -1);
        delete a;
        const int after = RefsOwnedBy(b, a);
        Gate("charclip-delete-owner", after == 0,
             "after delete A: B's ring holds %d ref(s) owned by A (retail Clear: 0)",
             after);
        delete b;
    }

    void DeleteTargetCase(bool cascade) {
        CharClip *a = NewClip();
        CharClip *b = NewClip();
        ObjectDir *dir = nullptr;
        if (cascade) {
            dir = Hmx::Object::New<ObjectDir>();
            b->SetName("charclip_refs_b.clp", dir);
        }
        Link(a, b);
        const int before = a->GetTransitions().Size();
        if (cascade)
            delete dir;
        else
            delete b;
        const int after = a->GetTransitions().Size();
        Gate(cascade ? "charclip-cascade-target" : "charclip-delete-target",
             before == 1 && after == 0,
             "%s killed the transition target: A names %d clip(s) before, %d after "
             "(retail Transitions::Replace drops it: 0)%s",
             cascade ? "~ObjectDir's cascade" : "a plain delete", before, after,
             after ? " -- DANGLING" : "");
        delete a;
    }

    void ReplaceCase() {
        CharClip *a = NewClip();
        CharClip *b = NewClip();
        CharClip *c = NewClip();
        Link(a, b);
        b->ReplaceRefs(c);
        CharClip::Transitions &t = a->GetTransitions();
        CharClip::NodeVector *vc = t.FindNodes(c);
        const bool noB = t.FindNodes(b) == nullptr;
        const int cRefs = RefsOwnedBy(c, a);
        const int bRefs = RefsOwnedBy(b, a);
        Gate("charclip-replace-target",
             t.Size() == 1 && vc && vc->size == 2 && noB && cRefs == 1 && bRefs == 0,
             "B->ReplaceRefs(C): A names %d clip(s), C %s (%d node(s)), B %s; refs "
             "owned by A: C %d, B %d (retail: 1 / 0)",
             t.Size(), vc ? "named" : "NOT named", vc ? vc->size : 0,
             noB ? "gone" : "STILL NAMED", cRefs, bRefs);
        delete b;
        delete c;
        const int after = t.Size();
        Gate("charclip-replace-then-delete", after == 0,
             "then delete C: A names %d clip(s) (retail: 0)", after);
        delete a;
    }

    void CascadeBothCase() {
        ObjectDir *dir = Hmx::Object::New<ObjectDir>();
        CharClip *a = NewClip();
        CharClip *b = NewClip();
        a->SetName("charclip_refs_a.clp", dir);
        b->SetName("charclip_refs_b.clp", dir);
        Link(a, b);
        Link(b, a);
        Link(a, a); // a self-transition
        delete dir;
        Gate("charclip-cascade-both", true,
             "a dir naming A<->B (and A->A) deleted without incident");
    }
}

int RunCharClipRefsPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== CharClip transitions hold their clips by reference ===\n");
    RingCase();
    DeleteTargetCase(false);
    DeleteTargetCase(true);
    ReplaceCase();
    CascadeBothCase();
    return 0;
}
