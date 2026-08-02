#include "char/CharWeightable.h"
#include "obj/Object.h"
#include "synth_xbox/PitchCorrectedVoice.h"

void TrueColor::ExposureRecipe::SetGlobalGain(float f) { mField_0x08 = f; }

CharWeightable::CharWeightable() : mWeight(1), mWeightOwner(this, this) {}

void CharWeightable::Replace(ObjRef *ref, Hmx::Object *obj) {
    if (RefIs(ref, mWeightOwner)) {
        if (!mWeightOwner.SetObj(obj)) {
            mWeightOwner = this;
        }
        return;
    } else {
        Hmx::Object::Replace(ref, obj);
    }
}

BEGIN_HANDLERS(CharWeightable)
    HANDLE_VIRTUAL_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharWeightable)
    SYNC_PROP_SET(weight, mWeight, SetWeight(_val.Float()))
    SYNC_PROP_SET(
        weight_owner, mWeightOwner.Ptr(), SetWeightOwner(_val.Obj<CharWeightable>())
    )
    /* Retail's ?SyncProperty@CharWeightable@@UAA... (0x823AEF98, 472 B, 118
     * instrs) has no superclass sync: it ends at 0x823AF164 with `li r3,0` and
     * its only indirect call is a slot-0 vtable dispatch in the weight_owner
     * arm -- not the slot-4 ClassName() dispatch that SYNC_VIRTUAL_SUPERCLASS
     * emits, and there is no StaticClassName() call anywhere in the body.
     * Same defect as RndTransformable::SyncProperty. */
END_PROPSYNCS

BEGIN_SAVES(CharWeightable)
    SAVE_REVS(3, 0)
    SAVE_VIRTUAL_SUPERCLASS(Hmx::Object)
    bs << mWeight;
    bs << mWeightOwner;
END_SAVES

BEGIN_COPYS(CharWeightable)
    /* Retail (0x823AEE30, 0x88 B) has NO superclass copy: the body opens
     * straight into __RTDynamicCast, saves only r29-r31 and uses a 0x70 frame.
     * COPY_VIRTUAL_SUPERCLASS expands to `if (ClassName() == StaticClassName())
     * Hmx::Object::Copy(o, ty);`, which cost 26 surplus instructions, two Symbol
     * temps (frame 0x80) and one extra callee-save. Same defect class as this
     * TU's SyncProperty (see the END_PROPSYNCS note above).
     * NOT a blanket removal: SAVE_/LOAD_VIRTUAL_SUPERCLASS below are CORRECT --
     * Save and Load are both already at 100% with them. */
    CREATE_COPY(CharWeightable)
    BEGIN_COPYING_MEMBERS
        if (ty == kCopyShallow) {
            SetWeightOwner(c->mWeightOwner);
        } else {
            SetWeightOwner(this);
            mWeight = c->mWeightOwner->mWeight;
        }
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(3, 0)

BEGIN_LOADS(CharWeightable)
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    if (d.rev > 2) {
        LOAD_VIRTUAL_SUPERCLASS(Hmx::Object)
    }
    d >> mWeight;
    if (d.rev > 1) {
        d >> mWeightOwner;
    }
END_LOADS
