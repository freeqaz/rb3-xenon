#pragma once
#include "utl/MemMgr.h"

class Rand {
public:
    Rand(int);
    void Seed(int);

    int Int();
    int Int(int, int);
    int FastInt(int, int);
    float Float();
    __declspec(noinline) float Float(float, float);
    float Gaussian();

    MEM_OVERLOAD(Rand, 0x16);

    static Rand sRand;

private:
    unsigned int mRandIndex1;
    unsigned int mRandIndex2;
    unsigned int mRandTable[256];
    float mSpareGaussianValue;
    bool mSpareGaussianAvailable;
};

void SeedRand(int);
int RandomInt();
int RandomInt(int, int);
float RandomFloat();
float RandomFloat(float, float);

// std::random_shuffle was removed in C++17 (Emscripten/Clang).
// This wrapper uses std::shuffle on native, std::random_shuffle on PPC.
#ifdef HX_NATIVE
#include <random>
#include <algorithm>
template <typename Iter>
inline void RandomShuffle(Iter first, Iter last) {
    std::shuffle(first, last, std::default_random_engine(RandomInt()));
}
#else
#include <algorithm>
template <typename Iter>
inline void RandomShuffle(Iter first, Iter last) {
    std::random_shuffle(first, last);
}
#endif
