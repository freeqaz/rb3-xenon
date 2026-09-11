#pragma once

class NgStats {
public:
    int mFaces; // 0x0
    int mParts; // 0x4
    int mPartSys; // 0x8
    int mRegMeshes; // 0xc
    int mMutMeshes; // 0x10
    int mBones; // 0x14
    int mMats; // 0x18
    int mCams; // 0x1c
    int mLightsReal; // 0x20
    int mLightsApprox; // 0x24
    int mMultiMeshInsts; // 0x28
    int mFlares; // 0x2c
    int mMotionBlurs; // 0x30
    // RB3 retail's 14th NgStats field is mSpotlights, NOT DC3's
    // mMultiMeshBatches (which sat at 0x2c here and pushed the tail +4).
    // Three retail instruments agree and one of them could have refuted it:
    //  (1) `NgRnd::UpdateOverlay` @0x82B87598 emits one `lwz r4, 0xNN(r31)` per
    //      overlay line at 0x4,0x8,0xc,0x10,0x14,0x18,0x1c,0x20,0x24,0x28,0x2c,
    //      0x30,0x34 -- FOURTEEN fields, so the struct does have a 0x34;
    //  (2) the format string each of those calls pairs with reads
    //      0x28 -> "multimesh %d\n"  (ONE line, not DC3's two)
    //      0x2c -> "flares %d\n"   0x30 -> "motion blur %d\n"
    //      0x34 -> "spotlights %d\n";
    //  (3) `EstimateDraw` @0x82B87200 touches the twelve fields its expression
    //      names at 0x4..0x30 CONSECUTIVELY -- no gap at 0x2c -- which is only
    //      consistent with the extra field being at the TAIL.
    // "multimesh %d %d\n", "multimesh %d\n", "spotlights %d %d\n" and
    // "spotlights %d\n" each occur exactly ONCE in retail band.exe;
    // "multimesh instances" and "multimesh batches" occur ZERO times.
    // ⚠ A probe for the literal "multimesh batches" alone reads as a decisive
    // negative for the DC3 field while being SILENT about what replaced it --
    // that is how this lane first mis-read the layout as "drop one field".
    int mSpotlights; // 0x34
};

extern NgStats *TheNgStats;
