#pragma once
#include "utl/BinStream.h"
#include "utl/ChunkIDs.h"
#include "utl/MemMgr.h"

#define kDataHeaderSize 8

class ChunkHeader {
public:
    ChunkHeader() : mID(), mLength(0), mIsList(0) {}
    ChunkHeader(BinStream &bs) : mID(), mLength(0), mIsList(0) { Read(bs); }
    ChunkHeader(ChunkID id, int len, bool list) : mID(id), mLength(len), mIsList(list) {}
    void Read(BinStream &);
    const ChunkID &ID() const { return mID; }
    int Length() { return mLength; }
    bool IsList() { return mIsList; }

    // prolly not the function name, it's inlined in debug
    // RBVR has a symbol int TotalLength() const, this could be it
    unsigned int GetNewLength() {
        unsigned int sublen = mIsList ? 12 : 8;
        return mLength + sublen;
    }

    MEM_OVERLOAD(ChunkHeader, 0x6B);

private:
    ChunkID mID; // 0x0
    int mLength; // 0x4
    bool mIsList; // 0x8
};

class IListChunk {
public:
    IListChunk(BinStream &, bool);
    IListChunk(IListChunk &);
    ~IListChunk();
    void Reset();
    void Lock();
    void UnLock();
    const ChunkHeader *CurSubChunkHeader() const;
    const ChunkHeader *Next();
    const ChunkHeader *Next(ChunkID);
    BinStream &BaseStream() { return mBaseBinStream; }

private:
    void Init();

    IListChunk *mParent; // 0x0
    BinStream &mBaseBinStream; // 0x4
    ChunkHeader *mHeader; // 0x8
    int mStartMarker; // 0xc
    int mEndMarker; // 0x10
    bool mLocked; // 0x14
    ChunkHeader mSubHeader; // 0x18
    bool mSubChunkValid; // 0x24
    bool mRecentlyReset; // 0x25
    int mSubChunkMarker; // 0x28
};

class IDataChunk : public BinStream {
public:
    IDataChunk(IListChunk &);
    virtual ~IDataChunk();
    virtual void Flush() {}
    virtual int Tell();
    virtual EofType Eof() { return (EofType)(mEof != 0); }
    virtual bool Fail() { return mFailed; }
    virtual void ReadImpl(void *, int);
    virtual void WriteImpl(const void *, int) {}
    virtual void SeekImpl(int, SeekType);
    // NOT virtual: BinStream declares no Header(), so `virtual` here APPENDS a
    // 12th vtable slot; retail's IDataChunk vtable has 11.  The sole subclass
    // (WaveFileData) does not override it -- and it inherits this table, which
    // is why WaveFileData measured +1 for the same single cause.
    ChunkHeader *Header() { return mHeader; }

private:
    IListChunk *mParent; // 0xc
    BinStream &mBaseBinStream; // 0x10
    ChunkHeader *mHeader; // 0x14
    int mStartMarker; // 0x18
    int mEndMarker; // 0x1c
    bool mFailed; // 0x20
    bool mEof; // 0x21
};
