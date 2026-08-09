// Ported from rb3-Wii src/system/bandobj/BandLeadMeter.cpp (MWCC -> MSVC X360).
#include "bandobj/BandLeadMeter.h"
#include "decomp.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "utl/Symbols.h"
#include <cmath>

// Retail addresses both rev statics off ONE base register with a +0/+4
// displacement, which requires an INTERNAL-linkage adjacent pair. DECLARE_REVS
// makes them CLASS statics (external symbols) and costs a second `lis`.
// gAltRev FIRST (retail stores hi16 at +0, lo16 at +4 and reads gRev from +4);
// explicit `= 0` is required or they land in .bss where MSVC picks the order.
static unsigned short gAltRev = 0;
static unsigned short gRev = 0;

BandLeadMeter::BandLeadMeter()
    : mNeedleAnim(this, 0), mLogoGlowAnim(this, 0), mGlowMesh1(this, 0),
      mGlowMesh2(this, 0), mPeggedAnim1(this, 0), mPeggedAnim2(this, 0),
      mLensMesh(this, 0), mLensMatNeutral(this, 0), mLensMat1(this, 0),
      mLensMat2(this, 0), unk204(10000), mScoreDiff(0) {
    SyncScores();
}

int BandLeadMeter::GetColor(int i) {
    if (i == 0)
        return 0;
    int color = 2;
    if (i > 0)
        color = 1;
    return color;
}

void BandLeadMeter::SyncScores() {
    int color = GetColor(mScoreDiff);
    if (mLogoGlowAnim)
        mLogoGlowAnim->SetFrame(color, 1.0f);
    if (mGlowMesh1)
        mGlowMesh1->SetShowing(color == 1);
    if (mGlowMesh2)
        mGlowMesh2->SetShowing(color == 2);
    if (mLensMesh) {
        RndMat *mat;
        if (mScoreDiff < 0)
            mat = mLensMat1;
        else if (mScoreDiff > 0)
            mat = mLensMat2;
        else
            mat = mLensMatNeutral;
        mLensMesh->SetMat(mat);
    }
    int neg = -mScoreDiff;
    float min = Min<float>(std::fabs((float)neg) / (float)unk204, 1.0f);
    float min50 = min * 50.0f;
    int i = -1;
    if (neg > 0)
        i = 1;
    float frame = min50 * i + 50.0f;
    if (mScoreDiff >= -unk204 && mScoreDiff <= unk204 && mNeedleAnim) {
        mNeedleAnim->SetFrame(frame, 1.0f);
    }
}

void BandLeadMeter::Enter() {
    RndDir::Enter();
    SyncScores();
}

void BandLeadMeter::Poll() {
    if (mScoreDiff > unk204 && mPeggedAnim1) {
        mPeggedAnim1->SetFrame(TheTaskMgr.Seconds(TaskMgr::kRealTime) * 30.0f, 1.0f);
    } else if (mScoreDiff < -unk204 && mPeggedAnim2) {
        mPeggedAnim2->SetFrame(TheTaskMgr.Seconds(TaskMgr::kRealTime) * 30.0f, 1.0f);
    }
}

BEGIN_HANDLERS(BandLeadMeter)
    HANDLE_ACTION(refresh, SyncScores())
    HANDLE_SUPERCLASS(RndDir)
    HANDLE_CHECK(0x80)
END_HANDLERS

BEGIN_PROPSYNCS(BandLeadMeter)
    SYNC_PROP_MODIFY_ALT(score_diff, mScoreDiff, SyncScores())
    SYNC_PROP(needle_anim, mNeedleAnim)
    SYNC_PROP(logo_glow_anim, mLogoGlowAnim)
    SYNC_PROP(glow_mesh_1, mGlowMesh1)
    SYNC_PROP(glow_mesh_2, mGlowMesh2)
    SYNC_PROP(pegged_anim_1, mPeggedAnim1)
    SYNC_PROP(pegged_anim_2, mPeggedAnim2)
    SYNC_PROP(lens_mesh, mLensMesh)
    SYNC_PROP(lens_mat_neutral, mLensMatNeutral)
    SYNC_PROP(lens_mat_1, mLensMat1)
    SYNC_PROP(lens_mat_2, mLensMat2)
    SYNC_SUPERCLASS(RndDir)
END_PROPSYNCS

BEGIN_SAVES(BandLeadMeter)
    SAVE_REVS(8, 0)
    bs << mScoreDiff;
    if (!IsProxy()) {
        bs << mNeedleAnim;
        bs << mLogoGlowAnim << mGlowMesh1 << mGlowMesh2;
        bs << mPeggedAnim1 << mPeggedAnim2;
        bs << mLensMesh << mLensMatNeutral << mLensMat1 << mLensMat2;
    }
    SAVE_SUPERCLASS(RndDir)
END_SAVES

void BandLeadMeter::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(8, 0);
    if (gRev < 6) {
        MILO_WARN("can't load old %s of rev %d", StaticClassName(), gRev);
    } else {
        if (gRev >= 8) {
            if (gLoadingProxyFromDisk) {
                int i;
                bs >> i;
            } else
                bs >> mScoreDiff;
            if (!IsProxy()) {
                bs >> mNeedleAnim;
                bs >> mLogoGlowAnim >> mGlowMesh1 >> mGlowMesh2;
                bs >> mPeggedAnim1 >> mPeggedAnim2;
                bs >> mLensMesh >> mLensMatNeutral >> mLensMat1 >> mLensMat2;
            }
        }
        RndDir::PreLoad(bs);
        bs.PushRev(packRevs(gAltRev, gRev), this);
    }
}

void BandLeadMeter::PostLoad(BinStream &bs) {
    int revs = bs.PopRev(this);
    unsigned short therev = getHmxRev(revs);
    gRev = therev;
    gAltRev = getAltRev(revs);
    if (therev >= 6) {
        RndDir::PostLoad(bs);
        gRev = therev;
        if (gRev < 8) {
            bs >> mScoreDiff;
            bs >> mNeedleAnim;
            bs >> mLogoGlowAnim >> mGlowMesh1 >> mGlowMesh2;
            bs >> mPeggedAnim1 >> mPeggedAnim2;
            if (gRev >= 7) {
                bs >> mLensMesh >> mLensMatNeutral >> mLensMat1 >> mLensMat2;
            }
        }
    }
}

BEGIN_COPYS(BandLeadMeter)
    COPY_SUPERCLASS(RndDir)
    CREATE_COPY(BandLeadMeter)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mScoreDiff)
            COPY_MEMBER(mNeedleAnim)
            COPY_MEMBER(mLogoGlowAnim)
            COPY_MEMBER(mGlowMesh1)
            COPY_MEMBER(mGlowMesh2)
            COPY_MEMBER(mPeggedAnim1)
            COPY_MEMBER(mPeggedAnim2)
            COPY_MEMBER(mLensMesh)
            COPY_MEMBER(mLensMatNeutral)
            COPY_MEMBER(mLensMat1)
            COPY_MEMBER(mLensMat2)
        }
    END_COPYING_MEMBERS
END_COPYS
