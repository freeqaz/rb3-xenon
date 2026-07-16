#pragma once
#include "oggvorbis/codec.h"
#include "oggvorbis/ogg.h"
#include "os/CritSec.h"
#include "os/File.h"
#include "synth/OggMap.h"
#include "synth/StandardStream.h"
#include "synth/StreamReader.h"
#include "synth/tomcrypt/mycrypt.h"

class VorbisReader : public StreamReader, public CriticalSection {
public:
    VorbisReader(File *, bool, StandardStream *, bool);
    virtual ~VorbisReader();
    virtual void Poll(float);
    virtual void Seek(int);
    virtual void EnableReads(bool enable) { mEnableReads = enable; }
    virtual bool Done() { return mDone; }
    virtual bool Fail() { return mFail; }

    static void SignalDecodeThread();

private:
    bool TryReadHeader();
    bool TryReadPacket(ogg_packet &);
    void InitDecoder();
    bool DoSeek();
    bool DoFileRead();
    void Decrypt(unsigned char *, int);
    int QueuedOutputSamples();
    bool TryDecode();
    bool CheckHmxHeader();

protected:
    virtual void Init();
    virtual int ConsumeData(void **, int, int);
    virtual void EndData() {}

    void setupCypher(int);
    void DoRawSeek(int);

    bool mTerminating;  // set true in destructor to signal decode thread to drop this reader
    int mNumChannels;   // number of audio channels (from vorbis_info)
    int mSampleRate;    // sample rate in Hz (from vorbis_info)
    File *mFile; // 0x30
    int mHeadersRead; // 0x34
    char *mReadBuffer; // 0x38
    bool mEnableReads; // 0x3c
    int unk40; // 0x40
    bool unk44; // 0x44
    bool mDone; // 0x45
    int unk48; // 0x48
    StandardStream *mStream; // 0x4c
    ogg_sync_state *mOggSync; // 0x50
    ogg_stream_state *mOggStream; // 0x54
    vorbis_info *mVorbisInfo; // 0x58
    vorbis_comment *mVorbisComment; // 0x5c
    vorbis_dsp_state *mVorbisDsp; // 0x60
    vorbis_block *mVorbisBlock; // 0x64
    int mMagicA; // 0x68 - byte grinder seed A
    int mMagicB; // 0x6c - byte grinder seed B
    int mKeyIndex; // 0x70
    int mMagicHashA; // 0x74
    int mMagicHashB; // 0x78
    int unk7c; // 0x7c
    ogg_packet mPendingPacket; // 0x80
    bool mHasPendingPacket;
    int mSeekTarget; // 0xa4
    int mSamplesToSkip; // 0xa8
    OggMap mOggMap; // 0xac
    int mHdrSize; // 0xc0
    // TU5 grew VorbisReader's tail: everything from here on shifts +44/+45 bytes
    // vs the TU0-era layout (retail mHdrBuf@0xf0, mCtrState@0xf4, unked@0x11a,
    // mFail@0x11c, mPcmBuffers@0x120 confirmed from the TU5 binary). mVersion moved
    // up out of the mFail region; the exact identity of the inserted members is
    // unknown (none are read by the compiled functions in this TU), so the block is
    // reproduced as layout-correct placeholders.
    int mVersion; // 0xc4 - mogg version? (moved earlier in TU5)
    char mUnkTU5_0xc8[40]; // 0xc8 -> 0xf0 : TU5-inserted members (identity TBD)
    char *mHdrBuf; // 0xf0
    symmetric_CTR *mCtrState; // 0xf4
    unsigned char mNonce[16]; // 0xf8
    unsigned char mKeyMask[16]; // 0x108
    bool unkec; // 0x118
    char mUnkTU5_0x119; // 0x119 : TU5-inserted byte (pushes unked/mEof/mFail +1)
    bool unked; // 0x11a
    bool mEof; // 0x11b
    bool mFail; // 0x11c
    std::vector<std::vector<short> > mPcmBuffers; // 0x120 - per-channel PCM sample buffers
    s64 mLastGranulePos; // 0x130
    int mPcmReadPos; // 0x138
};
