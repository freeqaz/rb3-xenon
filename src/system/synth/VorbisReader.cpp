#include "synth/VorbisReader.h"
#include "KeyChain.h"
#include "VorbisReader.h"
#include "oggvorbis/codec.h"
#include "obj/DataFile.h"
#include "oggvorbis/ogg.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/Endian.h"
#include "os/Timer.h"
#include "synth/Synth.h"
#include "utl/BufStream.h"
#include "xdk/win_types.h"
#include "xdk/xapilibi/processthreadsapi.h"
#include "xdk/xapilibi/synchapi.h"
#include "xdk/xapilibi/xbox.h"

namespace {
    unsigned char gKey[256];
    std::list<VorbisReader *> gReaders;
    std::list<VorbisReader *> gNewReaders;
    CriticalSection gLock;

    int gCipher = -1;
    int gKeySize = 16;
    unsigned char gRB1Key[16] = { 0x37, 0xB2, 0xE2, 0xB9, 0x1C, 0x74, 0xFA, 0x9E,
                                  0x38, 0x81, 0x08, 0xEA, 0x36, 0x23, 0xDB, 0xE4 };
    HANDLE gEvent = INVALID_HANDLE_VALUE;

    DWORD DecodeThreadEntry(HANDLE);
}

#define VORBIS_FAIL(name, err)                                                           \
    MILO_NOTIFY("Ogg Vorbis failure: %s, error code %i", name, err);

VorbisReader::VorbisReader(File *file, bool expectMap, StandardStream *stream, bool b2)
    : mNumChannels(-1), mSampleRate(-1), mFile(file), mHeadersRead(0), mReadBuffer(0),
      mEnableReads(true), unk40(0), unk44(0), mDone(0), mStream(stream), mOggSync(0),
      mOggStream(0), mVorbisInfo(0), mVorbisComment(0), mVorbisDsp(0), mVorbisBlock(0),
      mHasPendingPacket(0), mSeekTarget(-1), mSamplesToSkip(0), mHdrSize(0), mHdrBuf(0), mCtrState(0),
      unkec(b2), unked(0), mEof(0), mFail(0), mLastGranulePos(-1), mPcmReadPos(0) {
    MILO_ASSERT(mFile, 0xEC);
    if (expectMap) {
        mHdrBuf = new char[60000];
        mFile->ReadAsync(mHdrBuf, 60000);
        mFail = mFile->Fail();
    }
    mOggSync = new ogg_sync_state;
    ogg_sync_init(mOggSync);
    mTerminating = false;
#ifndef HX_NATIVE
    if (gEvent == INVALID_HANDLE_VALUE) {
        gEvent = CreateEventA(nullptr, false, false, nullptr);
        MILO_ASSERT(gEvent, 0xFE);
        HANDLE t = CreateThread(nullptr, 0x10000, DecodeThreadEntry, nullptr, 4, nullptr);
        MILO_ASSERT(t, 0x103);
        DWORD ret = XSetThreadProcessor(t, 2);
        MILO_ASSERT(ret != -1, 0x107);
        ret = ResumeThread(t);
        MILO_ASSERT(ret != -1, 0x109);
    }
    CritSecTracker tracker(&gLock);
    gNewReaders.push_front(this);
#endif
}

VorbisReader::~VorbisReader() {

#ifndef HX_NATIVE
    auto& terminating = mTerminating;
    terminating = true;
    unked = false;
    while (terminating) {
        SetEvent(gEvent);
    }
#endif
    delete[] mHdrBuf;
    mHdrBuf = nullptr;
    if (mOggStream) {
        ogg_stream_clear(mOggStream);
    }
    if (mVorbisBlock) {
        vorbis_block_clear(mVorbisBlock);
    }
    if (mVorbisDsp) {
        vorbis_dsp_clear(mVorbisDsp);
    }
    if (mVorbisComment) {
        vorbis_comment_clear(mVorbisComment);
    }
    if (mVorbisInfo) {
        vorbis_info_clear(mVorbisInfo);
    }
    ogg_sync_clear(mOggSync);
    RELEASE(mVorbisBlock);
    RELEASE(mVorbisDsp);
    RELEASE(mVorbisComment);
    RELEASE(mVorbisInfo);
    RELEASE(mOggStream);
    RELEASE(mOggSync);
    RELEASE(mCtrState);
}

void VorbisReader::Seek(int sample) {
    CritSecTracker tracker(this);
    MILO_ASSERT(mHeadersRead == 3, 0x1BD);
    MILO_ASSERT(mEnableReads, 0x1BE);
    MILO_ASSERT(sample >= 0, 0x1BF);
    mSeekTarget = sample;
    mDone = false;
    unked = false;
}

void VorbisReader::Init() {
    MILO_ASSERT(mStream, 0x41F);
    mStream->InitInfo(mNumChannels, mSampleRate, false, mOggMap.GetSongLengthSamples());
}

int VorbisReader::ConsumeData(void **v, int i1, int i2) {
    MILO_ASSERT(mSeekTarget == -1, 0x436);
    if (mSamplesToSkip > 0) {
        int ret = Min(i1, mSamplesToSkip);
        mSamplesToSkip -= ret;
        return ret;
    } else {
        MILO_ASSERT(mStream, 0x43F);
        return mStream->ConsumeData(v, i1, i2);
    }
}

void VorbisReader::setupCypher(int moggVersion) {
    char script[256];
    unsigned char masterKey[256];
#ifdef HX_NATIVE
    // On native, bypass the DTA obfuscation for masterKey initialization.
    // Xbox uses DTA scripting with (int)masterKey pointer math (32-bit only)
    // to copy masher data into masterKey as an anti-tamper measure.
    // On 64-bit this truncates the pointer. Just call getMasher directly.
    KeyChain::getMasher(masterKey);
    KeyChain::getKey(mKeyIndex, gKey, masterKey);
#else
    DataArray *arr = DataReadString("{Na 42 'O32'}");
    unsigned int iEval = arr->Evaluate(0).Int();
    arr->Release();

    char i6 = (iEval % 13);
    i6 = i6 + 'A';
    sprintf(script, "{%c %d %c}", i6, (int)masterKey ^ iEval, i6);
    DataArray *buf118Arr = DataReadString(script);
    buf118Arr->Evaluate(0);
    buf118Arr->Release();
#endif
    KeyChain::getKey(mKeyIndex, gKey, masterKey);
    TheSynth->Grinder().GrindArray(mMagicA, mMagicB, gKey, 0x10, moggVersion);
    for (int i = 0; i < 16; i++) {
        gKey[i] ^= mKeyMask[i];
    }
    int ret = ctr_start(gCipher, mNonce, gKey, gKeySize, 0, mCtrState);
    memset(gKey, 0, gKeySize);
    MILO_ASSERT(ret == 0, 0xB0);

#ifdef HX_NATIVE
    extern int magicNumberGeneratorNative(int idx, int mode);
    mMagicHashA = magicNumberGeneratorNative(mMagicA, 1);
    mMagicHashB = magicNumberGeneratorNative(mMagicB, 2);
#else
    sprintf(script, "{ha %d 1}", mMagicA);
    DataArray *magicGenA = DataReadString(script);
    mMagicHashA = magicGenA->Evaluate(0).Int();
    magicGenA->Release();

    sprintf(script, "{ha %d 2}", mMagicB);
    DataArray *magicGenB = DataReadString(script);
    mMagicHashB = magicGenB->Evaluate(0).Int();
    magicGenB->Release();
#endif
}

bool VorbisReader::TryReadHeader() {
    if (!mOggStream) {
        ogg_page page;
        int pageOut = ogg_sync_pageout(mOggSync, &page);
        if (pageOut < 0) {
            VORBIS_FAIL("StreamInit", pageOut);
#ifdef HX_NATIVE
            // Persistent page sync failure means data is corrupt (likely bad
            // mogg decryption). Mark the reader as failed so HandleWait can
            // break out instead of blocking forever.
            mFail = true;
            return false;
#endif
        }
        if (pageOut > 0) {
            mOggStream = new ogg_stream_state;
            ogg_stream_init(mOggStream, ogg_page_serialno(&page));
            ogg_stream_pagein(mOggStream, &page);
            mVorbisInfo = new vorbis_info;
            vorbis_info_init(mVorbisInfo);
            mVorbisComment = new vorbis_comment;
            vorbis_comment_init(mVorbisComment);
        } else
            return false;
    }
    if (mHeadersRead == 3)
        return false;
    else {
        ogg_packet packet;
        if (TryReadPacket(packet)) {
            int vorbisErr =
                vorbis_synthesis_headerin(mVorbisInfo, mVorbisComment, &packet);
            if (vorbisErr < 0)
                VORBIS_FAIL("HeaderIn", vorbisErr);
            mHeadersRead++;
            return true;
        } else {
            return false;
        }
    }
}

void VorbisReader::InitDecoder() {
    MILO_ASSERT(!mVorbisDsp && !mVorbisBlock, 0x2C0);
    MILO_ASSERT(mHeadersRead == 3, 0x2C1);
    mVorbisDsp = new vorbis_dsp_state;
    vorbis_synthesis_init(mVorbisDsp, mVorbisInfo);
    mVorbisBlock = new vorbis_block;
    vorbis_block_init(mVorbisDsp, mVorbisBlock);
}

bool VorbisReader::DoSeek() {
    mEnableReads = false;
    DoFileRead();
    if (!mFail && !mReadBuffer) {
        int i1, i2;
        mOggMap.GetSeekPos(mSeekTarget, i1, i2);
        DoRawSeek(i1);
        mSamplesToSkip = mSeekTarget - i2;
        MILO_ASSERT(mSamplesToSkip >= 0, 0x3C2);
        mSeekTarget = -1;
        mEnableReads = true;
        return true;
    }
    return false;
}

int VorbisReader::QueuedOutputSamples() {
    if (mVorbisDsp) {
        START_AUTO_TIMER("vorbis_synthesis_pcmout_cpu");
        return vorbis_synthesis_pcmout(mVorbisDsp, nullptr);
    } else
        return 0;
}

bool VorbisReader::TryReadPacket(ogg_packet &pk) {
    MILO_ASSERT(mOggStream, 0x39C);
    while (true) {
        int streamErr = ogg_stream_packetout(mOggStream, &pk);
        if (streamErr < 0) {
            VORBIS_FAIL("PacketOut", streamErr);
        }
        if (streamErr > 0)
            return true;
        ogg_page page;
        int syncErr = ogg_sync_pageout(mOggSync, &page);
        if (syncErr > 0) {
            ogg_stream_pagein(mOggStream, &page);
        } else
            return false;
    }
}

bool VorbisReader::TryDecode() {
    if (mFail)
        return false;
    if (QueuedOutputSamples() > 0)
        return false;
    if (!mHasPendingPacket && TryReadPacket(mPendingPacket)) {
        mHasPendingPacket = true;
    }
    if (mHasPendingPacket) {
        START_AUTO_TIMER("vorbis_synthesis_poll_cpu");
        if (mVorbisBlock->synthesis_state == vorbis_block::vss_init) {
            START_AUTO_TIMER("vorbis_synthesis_vssinit_cpu");
        } else if (mVorbisBlock->synthesis_state == vorbis_block::vss_decode) {
            START_AUTO_TIMER("vorbis_synthesis_vssdecode_cpu");
        } else {
            START_AUTO_TIMER("vorbis_synthesis_vssmdct_cpu");
        }
        int pollErr = vorbis_synthesis_poll(mVorbisBlock, &mPendingPacket);
        if (pollErr == OV_ENOTAUDIO) {
            mHasPendingPacket = false;
        } else {
            if (pollErr == -0x32)
                return true;
            if (pollErr < 0) {
                VORBIS_FAIL("Synthesis", pollErr);
            }
            mHasPendingPacket = false;
            if (pollErr == 0) {
                START_AUTO_TIMER("vorbis_synthesis_blockin_cpu");
                int blockErr = vorbis_synthesis_blockin(mVorbisDsp, mVorbisBlock);
                if (blockErr < 0) {
                    VORBIS_FAIL("BlockIn", blockErr);
                }
                return true;
            }
        }
    } else if (mEof && !mReadBuffer && QueuedOutputSamples() == 0 && !mDone) {
        EndData();
        mDone = true;
    }
    return false;
}

void VorbisReader::SignalDecodeThread() {
    if (gEvent != INVALID_HANDLE_VALUE) {
        SetEvent(gEvent);
    }
}

void VorbisReader::DoRawSeek(int byte) {
    if (mReadBuffer) {
        mEnableReads = false;
        while (!mFail && mReadBuffer)
            DoFileRead();
        mEnableReads = true;
    }
    for (int i = 0; i < mNumChannels; i++) {
        mPcmBuffers[i].clear();
    }
    mPcmReadPos = 0;
    mLastGranulePos = -1;
    int streamErr = ogg_stream_reset(mOggStream);
    if (streamErr < 0)
        VORBIS_FAIL("StreamReset", streamErr);
    int syncErr = ogg_sync_reset(mOggSync);
    if (syncErr < 0)
        VORBIS_FAIL("SyncReset", syncErr);
    vorbis_block_clear(mVorbisBlock);
    int restartErr = vorbis_synthesis_restart(mVorbisDsp);
    if (restartErr < 0)
        VORBIS_FAIL("DspReset", restartErr);
    vorbis_block_init(mVorbisDsp, mVorbisBlock);
    mHasPendingPacket = false;
    mFile->Seek(byte + mHdrSize, 0);
    if (mCtrState) {
        MILO_ASSERT(byte%16 == 0, 0x3F4);
        *(int *)mNonce = bool(EndianSwap((unsigned int)byte));
        int ret = ctr_reinit(gCipher, mNonce, mCtrState);
        MILO_ASSERT(ret == 0, 0x3F7);
    }
    mDone = false;
    mEof = false;
}

#define kMaxHeader 60000

bool VorbisReader::CheckHmxHeader() {
    if (!mHdrBuf)
        return true;
    else {
        int bytes;
        if (mFile->ReadDone(bytes)) {
            BufStream bs(mHdrBuf, 60000, true);
            bs >> mVersion;
            bs >> mHdrSize;
            MILO_ASSERT(mVersion >= 10, 0x239);
            MILO_ASSERT(mVersion <= 16, 0x23A);
            MILO_ASSERT(mHdrSize <= kMaxHeader, 0x23B);
            MILO_ASSERT(mHdrSize >= 0, 0x23C);
            mOggMap.Read(bs);
            mKeyIndex = 0;
            memset(mKeyMask, 0, sizeof(mKeyMask));
            mMagicA = mMagicB = 0;
            mMagicHashA = mMagicHashB = 0;
            if (mVersion >= 0xC && mVersion <= 0x10) {
                bs.Read(mNonce, sizeof(mNonce));
                s64 idx;
                bs >> idx;
                mMagicA = idx;
                bs >> idx;
                mMagicB = idx;
                unsigned char stuff[16];
                bs.Read(stuff, sizeof(stuff));
                bs.Read(stuff, sizeof(stuff));
                bs >> idx;
                mKeyIndex = (int)idx % 6 + 6;
                TheSynth->Grinder().HvDecrypt(stuff, mKeyMask, mVersion);
                gCipher = register_cipher(&rijndael_desc);
                MILO_ASSERT(gCipher >= 0, 0x268);
                mCtrState = new symmetric_CTR;
                int keyIndex = mKeyIndex;
                MILO_ASSERT_RANGE(keyIndex, 0, KeyChain::getNumKeys(), 0x26D);
                setupCypher(mVersion);
            } else if (mVersion == 0xB) {
                bs.Read(mNonce, sizeof(mNonce));
                gCipher = register_cipher(&rijndael_desc);
                MILO_ASSERT(gCipher >= 0, 0x276);
                mCtrState = new symmetric_CTR;
                int ret = ctr_start(gCipher, mNonce, gRB1Key, gKeySize, 0, mCtrState);
                MILO_ASSERT(ret == 0, 0x27E);
            } else {
                MILO_NOTIFY_ONCE("old mogg version!");
            }
            delete[] mHdrBuf;
            mHdrBuf = nullptr;
            mFile->Seek(mHdrSize, BinStream::kSeekBegin);
        }
        mFail = mFile->Fail();
        return !mHdrBuf;
    }
}

#ifdef HX_NATIVE

static void Decrypt(VorbisReader *reader, unsigned char *data, int bytes,
                    symmetric_CTR *ctrState, int magicHashA, int magicHashB) {
    if (!ctrState)
        return;

    // Step 1: Decrypt the entire buffer in-place using AES-CTR (stream cipher)
    unsigned char *tmp = new unsigned char[bytes];
    ctr_decrypt(data, tmp, bytes, ctrState);
    memcpy(data, tmp, bytes);
    delete[] tmp;

    // Step 2: Scan for all HMXA page headers and apply anti-tamper reversal.
    // v0xE encryption replaces OggS with HMXA and XORs bytes 12-15 and 20-23
    // with magicHash values. The XOR was designed for big-endian (Xbox 360),
    // so we must byte-swap the hash values on little-endian before XORing.
    if (magicHashA != 0 || magicHashB != 0) {
        unsigned int xorA = __builtin_bswap32((unsigned int)magicHashA);
        unsigned int xorB = __builtin_bswap32((unsigned int)magicHashB);
        for (int i = 0; i <= bytes - 4; i++) {
            if (data[i] == 'H' && data[i+1] == 'M'
                && data[i+2] == 'X' && data[i+3] == 'A') {
                data[i]   = 'O';
                data[i+1] = 'g';
                data[i+2] = 'g';
                data[i+3] = 'S';
                if (i + 16 <= bytes) {
                    unsigned int *ui = (unsigned int *)&data[i + 12];
                    *ui ^= xorA;
                }
                if (i + 24 <= bytes) {
                    unsigned int *ui = (unsigned int *)&data[i + 20];
                    *ui ^= xorB;
                }
            }
        }
    }
}

bool VorbisReader::DoFileRead() {
    bool ret = false;
    if (mFail)
        return false;

    int queuedBytes = mOggSync->fill - mOggSync->returned;
    if (mEnableReads && !mReadBuffer && !mFile->Eof() && queuedBytes < 0x10000) {
        mReadBuffer = ogg_sync_buffer(mOggSync, 0x4000);
        mFile->ReadAsync(mReadBuffer, 0x4000);
        mFail = mFile->Fail();
        ret = true;
    }

    int bytes = 0;
    if (!mFail && mReadBuffer && mFile->ReadDone(bytes) && !unk40) {
        mFail = mFile->Fail();
        if (mFail)
            return false;
        MILO_ASSERT(bytes > 0, 0x1F9);
        Decrypt(this, (unsigned char *)mReadBuffer, bytes, mCtrState, mMagicHashA, mMagicHashB);
        ogg_sync_wrote(mOggSync, bytes);
        mReadBuffer = 0;
        ret = true;
    }
    mFail = mFile->Fail();
    return ret;
}

void VorbisReader::Poll(float until) {
    if (!mFail && !unk44 && CheckHmxHeader() && !mDone && (mSeekTarget < 0 || DoSeek())) {
        DoFileRead();
        mEof = mFile->Eof();
        if (mHeadersRead < 3) {
            while (TryReadHeader())
                ;
            if (mHeadersRead >= 3) {
                mNumChannels = mVorbisInfo->channels;
                mSampleRate = mVorbisInfo->rate;
                mPcmBuffers.resize(mNumChannels);
                Init();
                InitDecoder();
            }
        } else {
            Timer timer;
            timer.Start();
            bool first = !unkec;
            while (timer.Ms() < until || first) {
                first = false;
                // Step 1: Push any decoded PCM to ring buffers
                {
                    float **pcm;
                    int pcmAvail = vorbis_synthesis_pcmout(mVorbisDsp, &pcm);
                    if (pcmAvail > 0) {
                        int consumed = ConsumeData((void **)pcm, pcmAvail,
                                                   mVorbisDsp->granulepos - pcmAvail);
                        vorbis_synthesis_read(mVorbisDsp, consumed);
                        if (consumed == 0)
                            break; // Ring buffer full — wait for audio callback to drain
                    }
                }
                // Step 2: Decode more Vorbis blocks
                if (!TryDecode()) {
                    // No packet available — on native there's no background decode
                    // thread, so read more file data and retry before giving up.
                    if (!DoFileRead())
                        break; // Can't read more (EOF, error, or ogg buffer full)
                    if (!TryDecode())
                        break; // Still no packet — give up this poll cycle
                }
                // Step 3: Feed raw data for next iteration
                DoFileRead();
                timer.Split();
            }
        }
    }
}

#endif // HX_NATIVE
