#include "game/KeysFx.h"
#include "beatmatch/TrackType.h"
#include "math/Rot.h"
#include "math/Utl.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/ObjMacros.h"
#include "synth/FxSend.h"
#include "utl/Loader.h"
#include "utl/Symbols.h"
#include "utl/Symbols2.h"

KeysFx::KeysFx(TrackType ty) : mTrackType(ty), unk20(0x78), unk31(1), unk34(-1.0f) {}

KeysFx::~KeysFx() {}

void KeysFx::Load() {
    FilePath fp(".", "sfx/keys_fx.milo");
    mFxDir.LoadFile(fp, true, false, kLoadFront, false);
}

void KeysFx::PostLoad() { mFxDir.PostLoad(0); }

void KeysFx::Poll(bool b1, bool b2, float f3, float f4, float f5) {
    static DataNode &capstrip_motion = DataVariable("capstrip_motion");
    static DataNode &dump_wah = DataVariable("dump_wah");
    if (capstrip_motion.Int()) {
        f5 = (float)capstrip_motion.Int() / 100.0f;
    }
    // The clamp is OUTSIDE the if: retail's `beq 0xdc` lands on the `fneg` that
    // starts the clamp, so both paths flow through it and the 0.0f/1.0f loads
    // come after the merge.  Putting the Clamp inside the if instead makes MSVC
    // hoist both constant loads above the branch, which shifts the whole region
    // and costs more than the clamp gains.  Bounds read from band.exe:
    // lbl_82000D78 = 0.0f, lbl_820009FC = 1.0f.
    f5 = Clamp(0.0f, 1.0f, f5);
    if (f5 == 0)
        unk20++;
    else
        unk20 = 0;
    bool b54 = true;
    if (unk20 >= 0x78)
        b54 = false;
    // minprod/freq are computed at point of use, NOT before the loop: retail
    // emits them after the `cmplwi/beq` iterator-empty test (i.e. in the loop
    // preheader, where /O1 LICM hoists them).  Declaring them ahead of the loop
    // puts the fmuls/fsubs before that branch and shifts the whole loop body.
    ObjDirItr<FxSend> it(mFxDir, true);
    for (; it != 0; ++it) {
        it->EnableUpdates(false);
        // Retail declares these as FUNCTION-LOCAL statics, NOT as the interned
        // globals from utl/Symbols*.h.  Each emits an MSVC static-init guard
        // (test bit / set bit / bl Symbol::Symbol) inside the loop -- 99
        // instructions, ~28% of this function, that the global form never
        // generates.  Declaration order is pinned by the retail guard-bit
        // indices 0x4..0x100: tempo_sync, tempo, beat_frac, dump, frequency,
        // wet_gain, dry_gain (0x1/0x2 are the two DataVariables above, which is
        // why those must stay first).  They are declared in PAIRS ahead of the
        // block that uses them: retail runs the `tempo` guard (0x8) BEFORE the
        // Property(tempo_sync) call and the `dump` guard (0x20) before the
        // beat_frac use, so declaring them inside their `if` bodies emits the
        // guards after the calls instead.
        static Symbol tempo_sync("tempo_sync");
        static Symbol tempo("tempo");
        const DataNode *tempoprop = it->Property(tempo_sync, false);
        if (tempoprop && tempoprop->Int()) {
            it->SetProperty(tempo, f3);
        }
        static Symbol beat_frac("beat_frac");
        static Symbol dump("dump");
        if (it->Property(beat_frac, false)) {
            it->SetProperty(beat_frac, f4);
        }
        if (it->Property(dump, false)) {
            if (dump_wah.Float() != 0) {
                it->SetProperty(dump, dump_wah.Float());
            }
        }
        if (unk34 != f5) {
            float min = Min(1.0f, f5 * 4.0f);
            float cos6 = std::cos(min * 1.5707964f);
            float log6 = (float)std::log10(cos6 + 0.001f) * 20.0f;
            float cos7 = std::cos((1.0f - min) * 1.5707964f);
            float log7 = (float)std::log10(cos7 + 0.001f) * 20.0f;
            static Symbol frequency("frequency");
            if (it->Property(frequency, false)) {
                it->SetProperty(frequency, 1.0f - f5);
            }
            static Symbol wet_gain("wet_gain");
            if (it->Property(wet_gain, false)) {
                it->SetProperty(wet_gain, log7);
            }
            static Symbol dry_gain("dry_gain");
            if (it->Property(dry_gain, false)) {
                it->SetProperty(dry_gain, log6);
            }
        }
        if (it->CanPushParameters()) {
            it->EnableUpdates(true);
        }
    }
    unk31 = b54;
    unk34 = f5;
}

FxSend *KeysFx::GetFxSend() {
    for (ObjDirItr<FxSend> it(mFxDir, true); it != 0; ++it) {
        if (it->Stage() == 0)
            return it;
    }
    MILO_WARN("couldn't find stage 0 keys fx");
    return nullptr;
}

BEGIN_HANDLERS(KeysFx)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE_CHECK(0x9B)
END_HANDLERS