#include "game/TrainerGemTab.h"
#include "decomp.h"
#include "bandobj/BandLabel.h"
#include "rndobj/Dir.h"
#include "os/Debug.h"
#include "rndobj/Anim.h"
#include "rndobj/Group.h"
#include "rndobj/Mesh.h"
#include "game/GameMode.h"

// Retail RB3-360 KEPT this TU's MILO_ASSERTs as bare condition evaluation
// (the fail-call was stripped, but the asserted-expression side-effects
// survive). The global Debug.h no-op (void)sizeof discards even the
// evaluation; evaluate-and-discard locally to restore the surviving-assert
// instruction shape without calling the failer.
#ifndef HX_NATIVE
#undef MILO_ASSERT
#undef MILO_ASSERT_RANGE
#define MILO_ASSERT(cond, line) ((void)(cond))
#define MILO_ASSERT_RANGE(value, min, max, line)                                         \
    ((void)((min) <= (value) && (value) < (max)))
#endif

// fn_801703DC
TrainerGemTab::TrainerGemTab()
    : mGemTab(0), mLanes(0), mConfigAnim(0), mVerticalTrans(0), mDrawOrderGroup(0),
      unk48(0), unk54(0), mTrackGroup(0), mGemSustainCyan(0), mStartLabel(0),
      mFinishLabel(0), unk12c(0), mLefty(0) {
    for (int i = 0; i < 6; i++)
        mTails[i] = 0;
    for (int i = 0; i < 9; i++)
        mGems[i] = 0;
    for (int i = 0; i < 25; i++)
        mInstLanes[i] = 0;
    for (int i = 0; i < 4; i++)
        mNumLabels[i] = 0;
}

TrainerGemTab::~TrainerGemTab() {}

// fn_80170944
void TrainerGemTab::Init(RndDir *gemTab, TrackType ty) {
    MILO_ASSERT(gemTab, 0x3F);
    mGemTab = gemTab;
    mTrackType = ty;
    mConfigAnim = mGemTab->Find<RndAnimatable>("instrument_config.anim", true);
    mDrawOrderGroup = mGemTab->Find<RndGroup>("draw_order.grp", true);
    mTrackGroup = mGemTab->Find<RndGroup>("track.grp", true);
    mVerticalTrans = mGemTab->Find<RndAnimatable>("gem_vertical_trans.anim", true);
    mStartLabel = mGemTab->Find<BandLabel>("start.lbl", true);
    mFinishLabel = mGemTab->Find<BandLabel>("finish.lbl", true);
    mNumLabels[0] = mGemTab->Find<BandLabel>("num_1.lbl", true);
    mNumLabels[1] = mGemTab->Find<BandLabel>("num_2.lbl", true);
    mNumLabels[2] = mGemTab->Find<BandLabel>("num_3.lbl", true);
    mNumLabels[3] = mGemTab->Find<BandLabel>("num_4.lbl", true);
    mGemTab->SetShowing(false);
    for (int i = 0; i < 4; i++) {
        mNumLabels[i]->SetInt(i + 1, false);
        mNumLabels[i]->SetShowing(false);
    }
    mGemChord2Lane = mGemTab->Find<RndMesh>("gem_chord_2lane.mesh", true);
    mGemChord3Lane = mGemTab->Find<RndMesh>("gem_chord_3lane.mesh", true);
    mGemChord4Lane = mGemTab->Find<RndMesh>("gem_chord_4lane.mesh", true);
    mGemChord5Lane = mGemTab->Find<RndMesh>("gem_chord_5lane.mesh", true);
    mGemChord6Lane = mGemTab->Find<RndMesh>("gem_chord_6lane.mesh", true);
    mGemSustainCyan = mGemTab->Find<RndMesh>("gem_sustain_cyan.mesh", true);
    switch (ty) {
    case kTrackDrum:
        mLanes = 5;
        mGems[0] = mGemTab->Find<RndMesh>("gem_drum_kick.mesh", true);
        mGems[1] = mGemTab->Find<RndMesh>("gem_drum_red.mesh", true);
        mGems[2] = mGemTab->Find<RndMesh>("gem_drum_yellow.mesh", true);
        mGems[3] = mGemTab->Find<RndMesh>("gem_drum_blue.mesh", true);
        mGems[4] = mGemTab->Find<RndMesh>("gem_drum_green.mesh", true);
        mGems[5] = mGemTab->Find<RndMesh>("gem_drum_cymbal_red.mesh", true);
        mGems[6] = mGemTab->Find<RndMesh>("gem_drum_cymbal_yellow.mesh", true);
        mGems[7] = mGemTab->Find<RndMesh>("gem_drum_cymbal_blue.mesh", true);
        mGems[8] = mGemTab->Find<RndMesh>("gem_drum_cymbal_green.mesh", true);
        mInstLanes[0] = 0;
        mInstLanes[1] = mGemTab->Find<RndTransformable>("drum_lane_1.trans", true);
        mInstLanes[2] = mGemTab->Find<RndTransformable>("drum_lane_2.trans", true);
        mInstLanes[3] = mGemTab->Find<RndTransformable>("drum_lane_3.trans", true);
        mInstLanes[4] = mGemTab->Find<RndTransformable>("drum_lane_4.trans", true);
        mConfigAnim->SetFrame(2.0f, 1.0f);
        break;
    case kTrackGuitar:
        mLanes = 5;
        mGems[0] = mGemTab->Find<RndMesh>("gem_green.mesh", true);
        mGems[1] = mGemTab->Find<RndMesh>("gem_red.mesh", true);
        mGems[2] = mGemTab->Find<RndMesh>("gem_yellow.mesh", true);
        mGems[3] = mGemTab->Find<RndMesh>("gem_blue.mesh", true);
        mGems[4] = mGemTab->Find<RndMesh>("gem_orange.mesh", true);
        mTails[0] = mGemTab->Find<RndMesh>("gem_sustain_green.mesh", true);
        mTails[1] = mGemTab->Find<RndMesh>("gem_sustain_red.mesh", true);
        mTails[2] = mGemTab->Find<RndMesh>("gem_sustain_yellow.mesh", true);
        mTails[3] = mGemTab->Find<RndMesh>("gem_sustain_blue.mesh", true);
        mTails[4] = mGemTab->Find<RndMesh>("gem_sustain_orange.mesh", true);
        mInstLanes[0] = mGemTab->Find<RndTransformable>("gtrbass_lane_1.trans", true);
        mInstLanes[1] = mGemTab->Find<RndTransformable>("gtrbass_lane_2.trans", true);
        mInstLanes[2] = mGemTab->Find<RndTransformable>("gtrbass_lane_3.trans", true);
        mInstLanes[3] = mGemTab->Find<RndTransformable>("gtrbass_lane_4.trans", true);
        mInstLanes[4] = mGemTab->Find<RndTransformable>("gtrbass_lane_5.trans", true);
        mConfigAnim->SetFrame(0.0f, 1.0f);
        break;
    case kTrackBass:
        mLanes = 4;
        mGems[0] = mGemTab->Find<RndMesh>("gem_green.mesh", true);
        mGems[1] = mGemTab->Find<RndMesh>("gem_red.mesh", true);
        mGems[2] = mGemTab->Find<RndMesh>("gem_yellow.mesh", true);
        mGems[3] = mGemTab->Find<RndMesh>("gem_blue.mesh", true);
        mTails[0] = mGemTab->Find<RndMesh>("gem_sustain_green.mesh", true);
        mTails[1] = mGemTab->Find<RndMesh>("gem_sustain_red.mesh", true);
        mTails[2] = mGemTab->Find<RndMesh>("gem_sustain_yellow.mesh", true);
        mTails[3] = mGemTab->Find<RndMesh>("gem_sustain_blue.mesh", true);
        mInstLanes[0] = mGemTab->Find<RndTransformable>("gtrbass_lane_1.trans", true);
        mInstLanes[1] = mGemTab->Find<RndTransformable>("gtrbass_lane_2.trans", true);
        mInstLanes[2] = mGemTab->Find<RndTransformable>("gtrbass_lane_3.trans", true);
        mInstLanes[3] = mGemTab->Find<RndTransformable>("gtrbass_lane_4.trans", true);
        mConfigAnim->SetFrame(0.0f, 1.0f);
        break;
    case kTrackKeys:
        break;
    case kTrackRealKeys:
        mLanes = 25;
        mGems[0] = mGemTab->Find<RndMesh>("gem_keys_black.mesh", true);
        mGems[1] = mGemTab->Find<RndMesh>("gem_keys_white.mesh", true);
        mTails[0] = mGemTab->Find<RndMesh>("gem_sustain_black.mesh", true);
        mTails[1] = mGemTab->Find<RndMesh>("gem_sustain_white.mesh", true);
        mInstLanes[0] = mGemTab->Find<RndTransformable>("key_lane_1.trans", true);
        mInstLanes[1] = mGemTab->Find<RndTransformable>("key_rail_1.trans", true);
        mInstLanes[2] = mGemTab->Find<RndTransformable>("key_lane_2.trans", true);
        mInstLanes[3] = mGemTab->Find<RndTransformable>("key_rail_2.trans", true);
        mInstLanes[4] = mGemTab->Find<RndTransformable>("key_lane_3.trans", true);
        mInstLanes[5] = mGemTab->Find<RndTransformable>("key_lane_4.trans", true);
        mInstLanes[6] = mGemTab->Find<RndTransformable>("key_rail_3.trans", true);
        mInstLanes[7] = mGemTab->Find<RndTransformable>("key_lane_5.trans", true);
        mInstLanes[8] = mGemTab->Find<RndTransformable>("key_rail_4.trans", true);
        mInstLanes[9] = mGemTab->Find<RndTransformable>("key_lane_6.trans", true);
        mInstLanes[10] = mGemTab->Find<RndTransformable>("key_rail_5.trans", true);
        mInstLanes[11] = mGemTab->Find<RndTransformable>("key_lane_7.trans", true);
        mInstLanes[12] = mGemTab->Find<RndTransformable>("key_lane_8.trans", true);
        mInstLanes[13] = mGemTab->Find<RndTransformable>("key_rail_6.trans", true);
        mInstLanes[14] = mGemTab->Find<RndTransformable>("key_lane_9.trans", true);
        mInstLanes[15] = mGemTab->Find<RndTransformable>("key_rail_7.trans", true);
        mInstLanes[16] = mGemTab->Find<RndTransformable>("key_lane_10.trans", true);
        mInstLanes[17] = mGemTab->Find<RndTransformable>("key_lane_11.trans", true);
        mInstLanes[18] = mGemTab->Find<RndTransformable>("key_rail_8.trans", true);
        mInstLanes[19] = mGemTab->Find<RndTransformable>("key_lane_12.trans", true);
        mInstLanes[20] = mGemTab->Find<RndTransformable>("key_rail_9.trans", true);
        mInstLanes[21] = mGemTab->Find<RndTransformable>("key_lane_13.trans", true);
        mInstLanes[22] = mGemTab->Find<RndTransformable>("key_rail_10.trans", true);
        mInstLanes[23] = mGemTab->Find<RndTransformable>("key_lane_14.trans", true);
        mInstLanes[24] = mGemTab->Find<RndTransformable>("key_lane_15.trans", true);
        mConfigAnim->SetFrame(4.0f, 1.0f);
        break;
    case kTrackRealGuitar:
        mLanes = 6;
        mGems[0] = mGemTab->Find<RndMesh>("gem_red.mesh", true);
        mGems[1] = mGemTab->Find<RndMesh>("gem_green.mesh", true);
        mGems[2] = mGemTab->Find<RndMesh>("gem_orange.mesh", true);
        mGems[3] = mGemTab->Find<RndMesh>("gem_blue.mesh", true);
        mGems[4] = mGemTab->Find<RndMesh>("gem_yellow.mesh", true);
        mGems[5] = mGemTab->Find<RndMesh>("gem_purple.mesh", true);
        mTails[0] = mGemTab->Find<RndMesh>("gem_sustain_red.mesh", true);
        mTails[1] = mGemTab->Find<RndMesh>("gem_sustain_green.mesh", true);
        mTails[2] = mGemTab->Find<RndMesh>("gem_sustain_orange.mesh", true);
        mTails[3] = mGemTab->Find<RndMesh>("gem_sustain_blue.mesh", true);
        mTails[4] = mGemTab->Find<RndMesh>("gem_sustain_yellow.mesh", true);
        mTails[5] = mGemTab->Find<RndMesh>("gem_sustain_purple.mesh", true);
        mInstLanes[0] = mGemTab->Find<RndTransformable>("rg_lane_1.trans", true);
        mInstLanes[1] = mGemTab->Find<RndTransformable>("rg_lane_2.trans", true);
        mInstLanes[2] = mGemTab->Find<RndTransformable>("rg_lane_3.trans", true);
        mInstLanes[3] = mGemTab->Find<RndTransformable>("rg_lane_4.trans", true);
        mInstLanes[4] = mGemTab->Find<RndTransformable>("rg_lane_5.trans", true);
        mInstLanes[5] = mGemTab->Find<RndTransformable>("rg_lane_6.trans", true);
        mConfigAnim->SetFrame(3.0f, 1.0f);
        break;
    case kTrackRealBass:
        mLanes = 4;
        mGems[0] = mGemTab->Find<RndMesh>("gem_red.mesh", true);
        mGems[1] = mGemTab->Find<RndMesh>("gem_green.mesh", true);
        mGems[2] = mGemTab->Find<RndMesh>("gem_orange.mesh", true);
        mGems[3] = mGemTab->Find<RndMesh>("gem_blue.mesh", true);
        mTails[0] = mGemTab->Find<RndMesh>("gem_sustain_red.mesh", true);
        mTails[1] = mGemTab->Find<RndMesh>("gem_sustain_green.mesh", true);
        mTails[2] = mGemTab->Find<RndMesh>("gem_sustain_orange.mesh", true);
        mTails[3] = mGemTab->Find<RndMesh>("gem_sustain_blue.mesh", true);
        mInstLanes[0] = mGemTab->Find<RndTransformable>("rg_lane_1.trans", true);
        mInstLanes[1] = mGemTab->Find<RndTransformable>("rg_lane_2.trans", true);
        mInstLanes[2] = mGemTab->Find<RndTransformable>("rg_lane_3.trans", true);
        mInstLanes[3] = mGemTab->Find<RndTransformable>("rg_lane_4.trans", true);
        mConfigAnim->SetFrame(3.0f, 1.0f);
        break;
    default:
        MILO_ASSERT(false, 0xEC);
        break;
    }
    mVerticalTrans->SetFrame(10.0f, 1.0f);
    if (mGems[0]) {
        unk12c = mGems[0]->WorldXfm().v.z;
    }
}

void TrainerGemTab::SetLefty(bool lefty) { mLefty = lefty; }

void TrainerGemTab::SetPattern(const TrainerSection *section, const std::vector<GameGem> &gems) {
    unk54 = section;
    unk4c = gems;
    int totalTicks = section->GetEndTick() - section->GetStartTick();
    if (totalTicks > 0xf00) {
        unk48 = 4;
    } else if (totalTicks > 0x780) {
        unk48 = 2;
    } else {
        unk48 = 1;
    }
}

DECOMP_FORCEBLOCK(
    TrainerGemTab, (TrainerGemTab * dummy, const TrainerSection *s, const std::vector<GameGem> &g),
    dummy->SetPattern(s, g);
)

int TrainerGemTab::SlotToGemIndex(int slot) const {
    static int keyGems[25] = {
        1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1,
        0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1,
    };
    if (mTrackType != kTrackRealKeys) {
        return slot;
    }
    return keyGems[slot];
}

int TrainerGemTab::GetLane(int slot) const {
    if (mTrackType == kTrackDrum) {
        if (slot == 0) return 0;
        if (mLefty) return mLanes - slot;
        return slot;
    }
    if (mLefty) return mLanes - slot - 1;
    return slot;
}

void TrainerGemTab::DrawStartFinish() {
    mStartLabel->SetShowing(true);
    mStartLabel->Draw();
    mStartLabel->SetShowing(false);
    mFinishLabel->SetShowing(true);
    mFinishLabel->Draw();
    mFinishLabel->SetShowing(false);
}

void TrainerGemTab::DrawExtraTails() {
    for (int i = 0; (unsigned int)i < unk130.size(); i++) {
        RndMesh *mesh;
        if (unk130[i].mIsRGChord) {
            mesh = mGemSustainCyan;
        } else {
            mesh = mTails[SlotToGemIndex(unk130[i].mSlot)];
        }
        mesh->SetShowing(true);
        mesh->SetWorldXfm(unk130[i].mXfm);
        mesh->Draw();
        mesh->SetShowing(false);
    }
}

void TrainerGemTab::Draw(int i) {
    if (!mGemTab)
        return;
    if (unk4c.empty())
        return;
    if (!mGems[0])
        return;
    unk130.erase(unk130.begin(), unk130.end());
    mGemTab->SetShowing(true);
    switch (unk48) {
    case 1:
        Render(unk54->GetStartTick(), unk54->GetEndTick(), 2.5f, 7.5f, i);
        DrawStartFinish();
        break;
    case 2:
        Render(unk54->GetStartTick(), unk54->GetEndTick(), 0.0f, 10.0f, i);
        DrawStartFinish();
        break;
    case 4: {
        Transform orig = mGemTab->WorldXfm();
        Transform xfm = orig;
        unsigned int tickRange = unk54->GetEndTick() - unk54->GetStartTick();
        xfm.v.z -= 60.0f;
        mGemTab->SetWorldXfm(xfm);
        int half = (int)tickRange / 2;
        Render(unk54->GetStartTick(), unk54->GetStartTick() + half, 0.0f, 10.0f, i);
        mStartLabel->SetShowing(true);
        mStartLabel->Draw();
        mStartLabel->SetShowing(false);
        xfm.v.z += 120.0f;
        mGemTab->SetWorldXfm(xfm);
        Render(unk54->GetStartTick() + half, unk54->GetEndTick(), 0.0f, 10.0f, i);
        DrawExtraTails();
        mFinishLabel->SetShowing(true);
        mFinishLabel->Draw();
        mFinishLabel->SetShowing(false);
        mGemTab->SetWorldXfm(orig);
        break;
    }
    }
    mGemTab->SetShowing(false);
}

void TrainerGemTab::Render(int startTick, int endTick, float startY, float endY, int) {
    mTrackGroup->SetShowing(true);
    mTrackGroup->DrawShowing();
    float yRange = endY - startY;
    float tickRange = (float)endTick - (float)startTick;
    mTrackGroup->SetShowing(false);
    for (int i = 0; (unsigned int)i < unk4c.size(); i++) {
        const GameGem &gem = unk4c[i];
        int tick = gem.GetTick();
        if (tick >= startTick && tick < endTick) {
            float y = (((float)tick - (float)startTick) / tickRange) *
                    yRange +
                startY;
            mVerticalTrans->SetFrame(y, 1.0f);
            if (gem.IsRealGuitarChord()) {
                DrawRealGuitarChord(gem);
            } else {
                unsigned int slots = gem.GetSlots();
                for (int slot = 0; slot < mLanes; slot++) {
                    int gemIndex = SlotToGemIndex(slot);
                    if ((slots & (1 << slot)) && mGems[gemIndex]) {
                        int lane = GetLane(slot);
                        if (mTrackType == kTrackDrum) {
                            if (TheGameMode->Property("force_use_cymbals", true)->Int() &&
                                gem.IsCymbal()) {
                                gemIndex = lane + 4;
                            } else {
                                gemIndex = lane;
                            }
                        }
                        if (mInstLanes[lane]) {
                            Vector3 pos;
                            pos = mGems[gemIndex]->WorldXfm().v;
                            pos.x = mInstLanes[lane]->WorldXfm().v.x;
                            mGems[gemIndex]->SetWorldPos(pos);
                        }
                        mGems[gemIndex]->SetShowing(true);
                        mGems[gemIndex]->Draw();
                        mGems[gemIndex]->SetShowing(false);
                    }
                }
            }
            DrawTails(gem, startTick, endTick, startY, endY);
        }
    }
}

void TrainerGemTab::DrawRealGuitarChord(const GameGem &gem) {
    int lowest = gem.GetLowestString();
    int highest = gem.GetHighestString();
    int idx = highest - lowest - 1;
    Vector3 pos = mGemChordLanes[idx]->WorldXfm().v;
    if (mLefty) {
        pos.x = mInstLanes[GetLane(highest)]->WorldXfm().v.x;
    } else {
        pos.x = mInstLanes[GetLane(lowest)]->WorldXfm().v.x;
    }
    mGemChordLanes[idx]->SetWorldPos(pos);
    mGemChordLanes[idx]->SetShowing(true);
    mGemChordLanes[idx]->Draw();
    mGemChordLanes[idx]->SetShowing(false);
}

void TrainerGemTab::DrawTails(
    const GameGem &gem, int startTick, int endTick, float startY, float endY
) {
    float fStartY = startY;
    if (gem.IgnoreDuration())
        return;
    float yRange = endY - fStartY;
    float tickRange = (float)endTick - (float)startTick;
    unsigned int slots = gem.GetSlots();
    for (int slot = 0; slot < mLanes; slot++) {
        if (slots & (1 << slot)) {
            mVerticalTrans->SetFrame(
                ((float)gem.GetTick() - (float)startTick) / tickRange *
                        yRange +
                    fStartY,
                1.0f
            );
            RndMesh *tail;
            if (gem.IsRealGuitarChord()) {
                tail = mGemSustainCyan;
            } else {
                tail = mTails[SlotToGemIndex(slot)];
            }
            MILO_ASSERT(tail, 0x1BE);
            if (!mInstLanes[GetLane(slot)])
                continue;
            tail->SetShowing(true);
            const Transform &cur = tail->WorldXfm();
            Transform orig = cur;
            Transform xfm = orig;
            xfm.v.x = mInstLanes[GetLane(slot)]->WorldXfm().v.x;
            float scale = 2.5f * ((float)gem.GetDurationTicks() / 480.0f);
            float scaleX10 = 10.0f * scale;
            float endZ = xfm.v.z + scaleX10;
            if (endZ > unk12c) {
                float overhang = 0.1f * (endZ - unk12c);
                float drawScale = scale - overhang;
                xfm.m.y.x *= drawScale;
                xfm.m.y.y *= drawScale;
                xfm.m.y.z *= drawScale;
                tail->SetWorldXfm(xfm);
                tail->Draw();
                tail->SetWorldXfm(orig);
                mVerticalTrans->SetFrame(0.0f, 1.0f);
                ExtraTail extra;
                extra.mIsRGChord = gem.IsRealGuitarChord();
                extra.mSlot = slot;
                const Transform &tw = tail->WorldXfm();
                extra.mXfm = tw;
                extra.mXfm.m.y.x *= overhang;
                extra.mXfm.m.y.y *= overhang;
                extra.mXfm.m.y.z *= overhang;
                extra.mXfm.v.x = 100.0f + mInstLanes[GetLane(slot)]->WorldXfm().v.x;
                unk130.push_back(extra);
            } else {
                xfm.m.y.x *= scale;
                xfm.m.y.y *= scale;
                xfm.m.y.z *= scale;
                tail->SetWorldXfm(xfm);
                tail->Draw();
            }
            tail->SetShowing(false);
        }
    }
}

// enum TrackType {
//     kTrackDrum,
//     kTrackGuitar,
//     kTrackBass,
//     kTrackVocals,
//     kTrackKeys,
//     kTrackRealKeys,
//     kTrackRealGuitar,
//     kTrackRealGuitar22Fret,
//     kTrackRealBass,
//     kTrackRealBass22Fret,
//     kTrackNone,
//     kNumTrackTypes,
//     kTrackPending,
//     kTrackPendingVocals
// };
