#include "synth/FxSendFlanger.h"
#include "obj/Object.h"
#include "synth/FxSend.h"
#include "utl/BinStream.h"

BEGIN_COPYS(FxSendFlanger)
    COPY_SUPERCLASS(FxSend)
    CREATE_COPY(FxSendFlanger)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mDelayMs)
        COPY_MEMBER(mRate)
        COPY_MEMBER(mDepthPct)
        COPY_MEMBER(mFeedbackPct)
        COPY_MEMBER(mOffsetPct)
        COPY_MEMBER(mTempoSync)
        COPY_MEMBER(mSyncType)
        COPY_MEMBER(mTempo)
    END_COPYING_MEMBERS
END_COPYS

FxSendFlanger::FxSendFlanger()
    : mDelayMs(2.0f), mRate(0.5f), mDepthPct(50), mFeedbackPct(50), mOffsetPct(10),
      mTempoSync(0), mSyncType(), mTempo(120.0f) {
    static Symbol quarter("quarter");
    mSyncType = quarter;
    mDryGain = -3.0f;
    mWetGain = -3.0f;
}

void FxSendFlanger::Save(BinStream &bs) {
    // RB3 retail is rev 6; DC3 (newer) bumped this to 7.  Target
    // fn_82722E08 emits `li r11, 0x6`.
    bs << 6;
    SAVE_SUPERCLASS(FxSend)
    bs << mDelayMs;
    bs << mRate;
    bs << mDepthPct;
    bs << mFeedbackPct;
    bs << mOffsetPct;
    bs << mTempoSync;
    bs << mSyncType;
    bs << mTempo;
}

// RB3 retail is rev 6 and uses the rb3-Wii ObjMacros LOAD dialect, NOT this
// tree's DC3-derived `BinStreamRev d` wrapper — written LONGHAND here rather
// than by switching the shared macro (which 280+ other TUs depend on).
//
// Adjudicated on retail bytes at fn_82722F18 (332 B), not on the oracle:
//   * every read passes the RAW incoming BinStream (`mr r3,r31`) and the
//     superclass call is `bl ?Load@FxSend@@` with r4=bs — there is no
//     BinStreamRev stack object anywhere in the body.  The `d` wrapper would
//     dispatch ReadEndian on `&d` (`addi r3,r1,N`) instead.
//   * the revs are stored to a pair of FILE-SCOPE `unsigned short`s four bytes
//     apart (`sth r10,0x0(r29)` / `sth r11,0x4(r29)` on lbl_82E03D7C) and
//     RELOADED with `lhz` after every call, because they are globals the callee
//     could touch.  A stack-local `int rev` can never produce that shape.  The
//     `__declspec(align(4))` still carried by Object.h's INIT_REVS is a fossil
//     of exactly this layout.
//   * the three percent members are read DIRECTLY into `this` (0x5c/0x60/0x64)
//     under plain `>=` tests.  DC3 (newer, rev 7) reads them through an `int
//     pct` temp for revs 4..6 and natively only at rev >= 7; retail has no
//     upper bound and no temp.  That rev-7 arm is the whole 488 -> 332 B gap.
// Declared gAltRev-first: MSVC emits these .bss statics in REVERSE declaration
// order, and retail wants gRev at lbl+0 / gAltRev at lbl+4 (measured — the
// natural order put gRev at +4 and cost 7 instruction-immediate diffs).
static __declspec(align(4)) unsigned short gAltRev;
static __declspec(align(4)) unsigned short gRev;

void FxSendFlanger::Load(BinStream &bs) {
    int revs;
    bs >> revs;
    gRev = getHmxRev(revs);
    gAltRev = getAltRev(revs);
    FxSend::Load(bs);
    if (gRev <= 4) {
        mDryGain = -3.0f;
        mWetGain = -3.0f;
        UpdateMix();
    }
    bs >> mDelayMs;
    bs >> mRate;
    int dummy;
    if (gRev >= 4) {
        bs >> mDepthPct;
    } else {
        bs >> dummy;
    }
    if (gRev >= 2) {
        bs >> mFeedbackPct;
    }
    if (gRev >= 3) {
        bs >> mOffsetPct;
    }
    if (gRev >= 6) {
        bs >> mTempoSync;
        bs >> mSyncType;
        bs >> mTempo;
    }
    OnParametersChanged();
}

BEGIN_HANDLERS(FxSendFlanger)
    HANDLE_SUPERCLASS(FxSend)
END_HANDLERS

BEGIN_PROPSYNCS(FxSendFlanger)
    SYNC_PROP_MODIFY(delay_ms, mDelayMs, OnParametersChanged())
    SYNC_PROP_MODIFY(rate, mRate, OnParametersChanged())
    SYNC_PROP_MODIFY(depth_pct, mDepthPct, OnParametersChanged())
    SYNC_PROP_MODIFY(feedback_pct, mFeedbackPct, OnParametersChanged())
    SYNC_PROP_MODIFY(offset_pct, mOffsetPct, OnParametersChanged())
    SYNC_PROP_MODIFY(tempo_sync, mTempoSync, OnParametersChanged())
    SYNC_PROP_MODIFY(sync_type, mSyncType, OnParametersChanged())
    SYNC_PROP_MODIFY(tempo, mTempo, OnParametersChanged())
    SYNC_SUPERCLASS(FxSend)
END_PROPSYNCS
