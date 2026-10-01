#include "char/CharTransDraw.h"
#include "char/Character.h"
#include "obj/Object.h"
#include "rndobj/Draw.h"
#include "utl/Std.h"

CharTransDraw::CharTransDraw() : mChars(this) {}

CharTransDraw::~CharTransDraw() { SetDrawModes(Character::kCharDrawAll); }

void CharTransDraw::SetDrawModes(Character::DrawMode mode) {
    FOREACH (it, mChars) {
        (*it)->SetDrawMode(mode);
    }
}

BEGIN_PROPSYNCS(CharTransDraw)
    SYNC_PROP(chars, mChars)
    SYNC_SUPERCLASS(RndDrawable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharTransDraw)
    /* Retail 0x82493840 (124 B) is ONE body shared by ?Save@RndMotionBlur@@
     * and ?Save@CharTransDraw@@ (their vtordisp thunks folded to 0x82493CA0).
     * Our MotionBlur version is masked-EQUAL to it; our CharTransDraw version
     * was 128 B.  The ENTIRE 4 B surplus is this constant: MotionBlur emits
     * `li r11,1` at +0x14 where we emitted `lis r11,1; ori r11,r11,2`
     * (= packRevs(1,2) = 0x00010002), and from +0x18 on the two bodies are
     * word-identical under a +4 shift.  So retail writes packRevs(0,1)
     * ⇒ SAVE_REVS(1, 0).  (lane W16-U, 2026-09-14) */
    SAVE_REVS(1, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mChars;
END_SAVES

BEGIN_COPYS(CharTransDraw)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(CharTransDraw)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mChars)
    END_COPYING_MEMBERS
END_COPYS

// Retail Load keeps no BinStreamRev: it splits the packed rev into one aligned
// file-scope aggregate (altRev +0, rev +4) and reads everything from the raw
// stream.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharTransDraw;

BEGIN_LOADS(CharTransDraw)
    int rev;
    bs >> rev;
    gRevs_CharTransDraw.rev = getHmxRev(rev);
    gRevs_CharTransDraw.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    RndDrawable::Load(bs);
    bs >> mChars;
    SetDrawModes(Character::kCharDrawOpaque);
END_LOADS

void CharTransDraw::DrawShowing() {
    ObjPtrList<Character>::iterator it = mChars.begin();
    for (; it != mChars.end(); ++it) {
        Character *c = *it;
        if (c->Showing()) {
            c->SetDrawMode(Character::kCharDrawTranslucent);
            c->Draw();
            c->SetDrawMode(Character::kCharDrawOpaque);
        }
    }
}

BEGIN_HANDLERS(CharTransDraw)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
