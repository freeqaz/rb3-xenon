enum PlayBlend { kPlayNow = 0, kPlayNoBlend = 1, kPlayLast = 2 };
struct CharGraphNode;
extern const CharGraphNode* FindFirst(void*, float);
extern const CharGraphNode* FindLast(void*, float);

const CharGraphNode* FindNodeA(PlayBlend blendMode, void* clip, float f) {
    const CharGraphNode* n = 0;
    if (blendMode >= kPlayNoBlend) {
        if (!(blendMode != kPlayNoBlend)) {
            n = 0;
        } else {
            if (blendMode >= kPlayLast) {
                n = FindLast(clip, f);
            } else {
                n = FindFirst(clip, f);
            }
        }
    }
    return n;
}

const CharGraphNode* FindNodeB(PlayBlend blendMode, void* clip, float f) {
    const CharGraphNode* n = 0;
    if (blendMode >= kPlayNoBlend) {
        if (blendMode == kPlayNoBlend) {
            n = 0;
        } else {
            if (blendMode >= kPlayLast) {
                n = FindLast(clip, f);
            } else {
                n = FindFirst(clip, f);
            }
        }
    }
    return n;
}
