#pragma once

// size 0x44
struct MemPointDelta {
public:
    MemPointDelta();
    MemPointDelta &operator+=(const MemPointDelta &);
    bool AnyGreaterThan(int) const;
    const char *ToString(int) const;

    static const char *HeaderString(const char *);

    int mHeapFreeBlocks[16]; // 0x0
    int mPhysicalFree; // 0x40
};

struct MemPoint {
public:
    enum eInitType {
        kInitType0,
        kInitType1
    };

    MemPoint(eInitType = kInitType1);
    MemPointDelta operator-(const MemPoint &);

    int mHeapFreeBlocks[16]; // 0x0
    int mPhysicalFree; // 0x40
};
