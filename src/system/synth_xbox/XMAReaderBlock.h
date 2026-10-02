#pragma once

// A byte FIFO over one owned, 0x20-aligned buffer: XMAReader keeps one per
// decoded channel (sizeof 0x18, `li r3,0x18` in XMAReader::Init; Init passes
// 0x10000) and fills it from XMAPlaybackQueryAvailableData, then hands the
// readable span to StandardStream::ConsumeData. Retail .text 0x82BBB2E8-0x82BBB510,
// a TU of its own. No virtual functions, so retail carries no RTTI name for it.
class XMAReaderBlock {
public:
    ~XMAReaderBlock(); // 0x82BBB2E8
    int FreeSpace() const; // 0x82BBB330
    int ReadableBytes() const; // 0x82BBB378, the contiguous span at the read position
    void *ReadPtr(int) const; // 0x82BBB3B0; XMAReader::Poll passes the byte count
    void Consume(int); // 0x82BBB3C0
    XMAReaderBlock(int); // 0x82BBB3F0
    bool Write(const void *, int, bool); // 0x82BBB450; Poll always passes true

    int mSize; // 0x0
    int mRead; // 0x4
    int mWrite; // 0x8
    bool mFull; // 0xc, read == write means full rather than empty
    char *mBuffer; // 0x10
    bool mOwnsBuffer; // 0x14
};
