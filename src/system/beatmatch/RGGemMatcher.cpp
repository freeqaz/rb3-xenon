#include "beatmatch/RGGemMatcher.h"
#include "beatmatch/RGState.h"
#include "beatmatch/Output.h"
#include <math.h>

RGGemMatcher::RGGemMatcher() { Reset(); }

void RGGemMatcher::Reset() {
    mState = RGState();
    ClearStringSwings();
    ClearNonStrums();
}

void RGGemMatcher::Swing(int iii, float f) {
    for (int i = 0; i < 6; i++) {
        if (iii & 1 << i) {
            mStringSwings[i] = f;
        }
    }
}

void RGGemMatcher::FretDown(int iii, float f) {
    int x, y;
    UnpackRGData(iii, x, y);
    mState.FretDown(x, y);
    mStringNonStrum[x] = f;
    AddFretHistory(x, y, f);
}

void RGGemMatcher::FretUp(int iii, float f) {
    int x, y;
    UnpackRGData(iii, x, y);
    mState.FretUp(x, y);
    mStringNonStrum[x] = f;
}

bool RGGemMatcher::FretMatch(
    const GameGem &gem,
    float f1,
    float f2,
    float f3,
    float f4,
    bool b1,
    bool b2,
    RGMatchType ty
) const {
    bool matchimpl = FretMatchImpl(gem, f1, f2, f3, f4, b1, b2, ty);
    if (TheBeatMatchOutput.IsActive()) {
        const char *str;
        if (matchimpl)
            str = MakeString("(%2d%10.1f MATCH_SUCCESS)\n", 0, f4 + f3);
        else
            str = MakeString("(%2d%10.1f MATCH_FAIL)\n", 0, f4 + f3);
        TheBeatMatchOutput.Print(str);
    }
    return matchimpl;
}

bool RGGemMatcher::FretMatchImpl(
    const GameGem &gem,
    float f1,
    float f2,
    float f3,
    float f4,
    bool b1,
    bool b2,
    RGMatchType ty
) const {
    bool loose = false;
    if (gem.Loose()) {
        loose = true;
        f1 += 25.0f;
    }
    bool isStrum = loose | (gem.GetRGStrumType() != 0);

    int bit;
    int numSwung = 0;
    int numChecked = 0;
    int numMatched = 0;
    int numFretted = 0;
    int numFullMatch = 0;
    int numCloseSwings = 0;
    int numMissed = 0;

    for (int i = 0; i < 6; i++) {
        float swing = mStringSwings[i];
        bool swung = b2 | ((float)fabs((swing + f3) - f2) <= f1);
        if (b1) {
            float nonStrum = mStringNonStrum[i];
            swung = swung | ((float)fabs((nonStrum + f3) - f2) <= f1);
        }
        if ((float)fabs(swing - f4) < 68.0f) {
            numCloseSwings++;
        }
        if (gem.GetFret(i) >= 0) {
            bit = 1 << i;
            if ((bit & 0x3F) && gem.GetRGNoteType(i) != kRGGhost) {
                if (gem.GetFret(i) > 0) {
                    numFretted++;
                }
                numChecked++;
                if (gem.GetRGNoteType(i) == kRGMuted ||
                    FretHistoryMatch(i, gem.GetFret(i), f4, f1, ty)) {
                    numMatched++;
                    if (swung) {
                        numSwung++;
                        if (gem.GetFret(i) > 0 || gem.GetRGNoteType(i) == kRGMuted) {
                            numFullMatch++;
                        }
                    }
                }
                if ((bit & gem.GetImportantStrings()) &&
                    !FretHistoryMatch(i, gem.GetFret(i), f4, f1, ty)) {
                    numMissed++;
                }
            }
        }
    }

    if (numMissed > 0)
        return false;
    if (numChecked == 1) {
        if (numSwung < 1)
            return false;
        if (numCloseSwings > 2 && !isStrum)
            return false;
        return true;
    }
    if (numChecked == 2) {
        if (numSwung < 1)
            return false;
        if (numMatched != 2)
            return false;
        if (numCloseSwings > 3 && !isStrum)
            return false;
        return true;
    }
    if (numFretted == 0 && numChecked > 1) {
        if (numSwung < (int)(0.75f * numChecked))
            return false;
        if (numCloseSwings > numChecked * 2 && !isStrum)
            return false;
        return true;
    }
    if (numChecked == 3 && numFretted > 1) {
        if (numSwung < 2)
            return false;
        return numFullMatch >= (int)(0.5f * numFretted);
    }
    if (numChecked > 2 && numFretted > 0) {
        if (numSwung < 3)
            return false;
        return numFullMatch >= (int)(0.5f * numFretted);
    }
    return false;
}

RGState *RGGemMatcher::GetState() { return &mState; }
const RGState *RGGemMatcher::GetState() const { return &mState; }

void RGGemMatcher::ClearStringSwings() {
    for (int i = 0; i < 6; i++) {
        mStringSwings[i] = 0.0f;
    }
}

void RGGemMatcher::ClearNonStrums() {
    for (int i = 0; i < 6; i++) {
        mStringNonStrum[i] = 0.0f;
    }
}

bool RGGemMatcher::FretHistoryMatch(int i1, int i2, float f3, float f4, RGMatchType ty)
    const {
    if (ty == 0)
        return mState.GetFret(i1) == i2;
    else {
        float f1 = f3;
        for (int i = 0; i < 4; i++) {
            if (i2 == pairs[i1][i].i && f3 - f1 < f4)
                return true;
            f1 = pairs[i1][i].f;
        }
        return false;
    }
}

void RGGemMatcher::AddFretHistory(int i1, int i2, float f3) {
    for (int i = 3; i >= 1; i--) {
        pairs[i1][i] = pairs[i1][i - 1];
    }
    pairs[i1][0].i = i2;
    pairs[i1][0].f = f3;
}
