#pragma once
#include "game/Defines.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "ui/UILabel.h"

class Instarank : public virtual Hmx::Object {
public:
    // for SOME reason, for this specific TU, the class members are declared before the
    // virtual funcs
    // laneDN-4: retail's type here is byte-sized but NOT bool. Evidence:
    // `MetaPerformer::HasValidBattleInstarank` (inlined into
    // MetaPerformer::Handle) returns this field from a bool-returning function
    // and retail emits the branchless !=0 normalization `subic r10,r11,0x1;
    // subfe r11,r10,r11` -- a conversion that only exists if the source type is
    // not already bool.
    // ⛔ The prior refutation in MetaPerformer.cpp (laneAY-B, re-affirmed by
    // laneCN-3) is INVERTED. It argued the sibling site 0x82583FF4 stores the
    // same field RAW (`lbz r11,-0x2a0(r24); stw r11,0(r29)`) and so a char flip
    // "breaks that site". It does not: an unsigned char loaded with lbz and
    // stored into a DataNode's int slot IS exactly `lbz; stw` with no
    // normalization. The sibling site is consistent with BOTH types, so it
    // refutes neither -- while only the non-bool type explains our site.
    unsigned char mIsValid; // 0x4
    int unk8; // 0x8 - id?
    bool unkc; // 0xc - is_boi?
    ScoreType mScoreType; // 0x10
    int mInstaRank; // 0x14
    bool mIsPercentile; // 0x18
    String mStr1; // 0x1c
    String mStr2; // 0x28

    Instarank();
    virtual ~Instarank() {}

    void Clear();
    void Init(int, bool, ScoreType, int, bool, String, String);
    void UpdateRankLabel(UILabel *);
    bool HasHighscore() const;
    void UpdateString1Label(UILabel *);
    void UpdateString2Label(UILabel *);
    bool IsValid() const { return mIsValid; }
    ScoreType GetScoreType() const { return mScoreType; }
};

DECLARE_MESSAGE(InstarankDoneMsg, "instarank_done")
InstarankDoneMsg() : Message(Type()) {}
END_MESSAGE