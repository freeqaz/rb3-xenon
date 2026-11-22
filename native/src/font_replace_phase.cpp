// rb3-xenon native -- A18: RndFont::Replace is retail's body plus the one
// forward native's MsgSinks needs.
//
// THE REFERENCE is band.exe (TU5, base 0x82000000), fn_82472D78 (0x74),
// ?Replace@RndFont@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z. It compares the texture
// owner (lwz 0x3c) against `from`; on a match a null replacement makes the font
// its own owner (mr r4, r31) and any other adopts the replacement's owner
// (dynamic_cast, then lwz 0x8 of its 0x34 ObjOwnerPtr). It never calls
// Hmx::Object::Replace.
//
// WHY NATIVE STILL FORWARDS. HX_NATIVE gives every Hmx::Object a MsgSinks
// (obj/Object.h mSinks, absent from retail's layout). Its entries are
// ObjOwnerPtr<Hmx::Object> owned by the object that holds them, so a dying
// sink's notice arrives as owner->Replace(entry, nullptr), and only
// Hmx::Object::Replace hands it to MsgSinks::Replace, which erases the entry.
// Native ReplaceRefs takes the entry out of the dead object's ring first, so a
// RndFont::Replace that swallows the notice leaves the font holding a sink
// aimed at freed memory.
//
// The cases, each on fresh fonts:
//   font-replace-sink-dies     A->AddSink(S); delete S; A has no sinks left.
//                              Retail's body unchanged on native fails this.
//   font-replace-owner-adopt   self-owned A, A->ReplaceRefs(B): A's owner is
//                              B's owner (retail). The old native arm kept A.
//   font-replace-owner-dies    F owned by C; delete C: F owns itself (retail
//                              and old arm agree; a control).
// Run in a forked child: a crash is a failed gate, not a dead runner.

#include "obj/Msg.h"
#include "obj/Object.h"
#include "rndobj/Font.h"

#include <cstdarg>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>

typedef void (*GateFn)(const char *, bool, const char *);

namespace {
    GateFn gGate;

    void Gate(const char *name, bool pass, const char *fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        gGate(name, pass, buf);
    }

    struct FontPeek : RndFont {
        static void SetOwner(RndFont *f, RndFont *owner) {
            static_cast<FontPeek *>(f)->mTextureOwner = owner;
        }
    };

    enum {
        kSinkAdded = 1 << 0, // A held exactly one sink before S died
        kSinkGone = 1 << 1,  // ... and none after
        kAdopt = 1 << 2,     // ReplaceRefs(B) on self-owned A: owner B
        kAdoptPeer = 1 << 3, // a font owned by A followed to B too
        kOwnerDies = 1 << 4, // owner deleted: the font owns itself
        kRan = 1 << 5,
    };

    int SinkCount(Hmx::Object *o) {
        MsgSinks *s = o->Sinks();
        return s ? (int)s->Sinks().size() : 0;
    }

    int FontReplaceChild() {
        int bits = 0;
        RndFont::Init();

        // 1. A sink of a font dies.
        RndFont *a = Hmx::Object::New<RndFont>();
        Hmx::Object *s = Hmx::Object::New<RndFont>();
        a->AddSink(s);
        int before = SinkCount(a);
        delete s;
        int after = SinkCount(a);
        fprintf(stderr, "  [font-replace] sinks of A: %d before S died, %d after\n", before, after);
        if (before == 1)
            bits |= kSinkAdded;
        if (after == 0)
            bits |= kSinkGone;

        // 2. A self-owned font is replaced by another.
        RndFont *b = Hmx::Object::New<RndFont>();
        RndFont *peer = Hmx::Object::New<RndFont>();
        FontPeek::SetOwner(peer, a);
        bool selfOwned = a->TextureOwner() == a && b->TextureOwner() == b;
        a->ReplaceRefs(b);
        fprintf(stderr,
                "  [font-replace] ReplaceRefs(B) on A (self-owned %d): A's owner is %s, "
                "peer's owner is %s\n",
                (int)selfOwned,
                a->TextureOwner() == b ? "B" : (a->TextureOwner() == a ? "A" : "other"),
                peer->TextureOwner() == b ? "B" : (peer->TextureOwner() == a ? "A" : "other"));
        if (selfOwned && a->TextureOwner() == b)
            bits |= kAdopt;
        if (peer->TextureOwner() == b)
            bits |= kAdoptPeer;

        // 3. A font's owner dies.
        RndFont *c = Hmx::Object::New<RndFont>();
        RndFont *f = Hmx::Object::New<RndFont>();
        FontPeek::SetOwner(f, c);
        delete c;
        fprintf(stderr, "  [font-replace] owner C deleted: F owns %s\n",
                f->TextureOwner() == f ? "itself" : "something else");
        if (f->TextureOwner() == f)
            bits |= kOwnerDies;

        delete f;
        delete peer;
        delete a;
        delete b;
        bits |= kRan;
        return bits;
    }
}

int RunFontReplacePhase(GateFn gate) {
    gGate = gate;
    printf("\n=== A18: RndFont::Replace is retail's body (band.exe 0x82472D78) + the MsgSinks forward ===\n");
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid == 0) {
        alarm(60);
        _exit(FontReplaceChild());
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool exited = WIFEXITED(status);
    int bits = exited ? WEXITSTATUS(status) : 0;
    const char *how = exited ? "exited" : "crashed";
    Gate("font-replace-sink-dies", exited && (bits & kSinkAdded) && (bits & kSinkGone),
         "a dead sink leaves the font's MsgSinks (needs the Hmx::Object::Replace forward) "
         "(child %s, bits 0x%02x)",
         how, bits);
    Gate("font-replace-owner-adopt", exited && (bits & kRan) && (bits & kAdopt) && (bits & kAdoptPeer),
         "ReplaceRefs(B) on a self-owned font adopts B's owner, as retail 0x82472D78 does");
    Gate("font-replace-owner-dies", exited && (bits & kRan) && (bits & kOwnerDies),
         "a font whose owner dies owns itself (retail: null replacement -> this)");
    return 0;
}
