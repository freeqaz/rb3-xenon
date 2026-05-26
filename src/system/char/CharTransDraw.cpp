#include "char/CharTransDraw.h"
#include "char/Character.h"
#include "obj/Object.h"
#include "rndobj/Draw.h"
#include "utl/Std.h"

CharTransDraw::CharTransDraw() : mChars(this), mForceDraw(false) {}

CharTransDraw::~CharTransDraw() { SetDrawModes(Character::kCharDrawAll); }

void CharTransDraw::SetDrawModes(Character::DrawMode mode) {
    FOREACH (it, mChars) {
        (*it)->SetDrawMode(mode);
    }
}

BEGIN_PROPSYNCS(CharTransDraw)
    SYNC_PROP(chars, mChars)
    SYNC_PROP(force_draw, mForceDraw)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(CharTransDraw)
    SAVE_REVS(2, 1)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    bs << mChars;
    bs << mForceDraw;
END_SAVES

BEGIN_COPYS(CharTransDraw)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY(CharTransDraw)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mChars)
        COPY_MEMBER(mForceDraw)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(2, 1)

void CharTransDraw::Load(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(2, 1)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndDrawable)
    d >> mChars;
    if (d.altRev > 0) {
        d >> mForceDraw;
    }
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
        } else if (mForceDraw) {
            c->SetDrawMode(Character::kCharDrawTranslucent);
            c->SetShowing(true);
            c->Draw();
            c->SetShowing(false);
            c->SetDrawMode(Character::kCharDrawOpaque);
        }
    }
}

BEGIN_HANDLERS(CharTransDraw)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
