#include "hamobj/ScoreUtl.h"
#include "os/Debug.h"

std::vector<Symbol> sRatingStates;
std::vector<float> sDefaultRatingThresholds;

MoveRating DetectFracToMoveRating(float detect_frac, const std::vector<float> *ratings) {
    if (!ratings)
        ratings = &sDefaultRatingThresholds;
    MILO_ASSERT(detect_frac >= 0 && detect_frac <= 1.0f, 0x22);
    for (int i = 0; i < ratings->size(); i++) {
        if (detect_frac >= (*ratings)[i])
            return (MoveRating)i;
    }
    return kNumMoveRatings;
}

void RatingStateThreshold(
    int index, Symbol &ratingState, float &thresh, const std::vector<float> *thresholds
) {
    if (!thresholds)
        thresholds = &sDefaultRatingThresholds;
    MILO_ASSERT((0) <= (index) && (index) < (sRatingStates.size()), 0x77);
    MILO_ASSERT((0) <= (index) && (index) < (thresholds->size()), 0x78);
    ratingState = sRatingStates[index];
    thresh = (*thresholds)[index];
}

int RatingStateToIndex(Symbol s) {
    for (int i = 0; i < sRatingStates.size(); i++) {
        if (s == sRatingStates[i]) {
            return i;
        }
    }
    MILO_NOTIFY("Could not find rating (%s)", s);
    return 0;
}

float RatingToRatingFrac(Symbol rating) {
    unsigned int numRatings = sRatingStates.size();
    for (unsigned int i = 0; i < numRatings; i++) {
        if (rating == sRatingStates[i]) {
            return (float)(int)(numRatings - 1 - i) / (float)(int)(numRatings - 1);
        }
    }
    MILO_NOTIFY("Could not find rating (%s)", rating);
    return 0.0f;
}

Symbol RatingState(int index) {
    MILO_ASSERT((0) <= (index) && (index) < (sRatingStates.size()), 0xA7);
    return sRatingStates[index];
}

float DetectFracToRatingFrac(float detect_frac, const std::vector<float> *ratings) {
    MILO_ASSERT(detect_frac >= 0 && detect_frac <= 1.0f, 0x2f);
    if (!ratings)
        ratings = &sDefaultRatingThresholds;
    unsigned int i = 0;
    unsigned int size = ratings->size();
    float prevThresh = 1.0f;
    float result = 0.0f;
    if (size != 0) {
        do {
            float thresh = (*ratings)[i];
            if (detect_frac >= thresh) {
                result = 1.0f;
                if (i != 0) {
                    result = ((detect_frac - thresh) / (prevThresh - thresh)
                              + (float)(int)(size - 1) - (float)(int)i)
                        * (1.0f / (float)(int)(size - 1));
                }
                break;
            }
            i++;
            prevThresh = thresh;
        } while (i < size);
    }
    return result;
}

float RatingToDetectFrac(Symbol rating, const std::vector<float> *thresholds) {
    if (!thresholds)
        thresholds = &sDefaultRatingThresholds;
    unsigned int i = 0;
    unsigned int size = sRatingStates.size();
    if (size != 0) {
        do {
            if (rating == sRatingStates[i]) {
                return (*thresholds)[i];
            }
            i++;
        } while (i < size);
    }
    MILO_NOTIFY("Could not find rating (%s)", rating);
    return 0.0f;
}

Symbol DetectFracToRating(float detect_frac, const std::vector<float> *ratings, int *outIdx) {
    MoveRating r = DetectFracToMoveRating(detect_frac, ratings);
    if (outIdx)
        *outIdx = r;
    if (r == kNumMoveRatings) {
        return gNullStr;
    }
    return sRatingStates[r];
}

float GetScoreBonus(float detect_frac, const std::vector<float> *ratings) {
    if (!ratings)
        ratings = &sDefaultRatingThresholds;
    MILO_ASSERT(detect_frac >= 0 && detect_frac <= 1.0f, 0x8d);
    unsigned int size = ratings->size();
    float upperThresh = 0.0f;
    float lowerThresh = 0.0f;
    unsigned int i = 0;
    if (size != 0) {
        do {
            if (detect_frac >= (*ratings)[i]) {
                lowerThresh = (*ratings)[i];
                upperThresh = 1.0f;
                if (i != 0) {
                    upperThresh = (*ratings)[i - 1];
                }
                break;
            }
            i++;
        } while (i < size);
    }
    return (detect_frac - lowerThresh) / (upperThresh - lowerThresh);
}

void ScoreUtlInit(const DataArray *cfg) {
    sRatingStates.clear();
    sDefaultRatingThresholds.clear();
    DataArray *states = cfg->FindArray("feedback_states", true);
    DataArray *thresholds = cfg->FindArray("feedback_thresholds", true);
    MILO_ASSERT(states->Size() == thresholds->Size(), 0xB3);
    for (int i = 1; i < states->Size(); i++) {
        sRatingStates.push_back(states->Sym(i));
        sDefaultRatingThresholds.push_back(thresholds->Node(i).Float(thresholds));
    }
    MILO_ASSERT(sRatingStates.size() == kNumMoveRatings, 0xBA);
}
