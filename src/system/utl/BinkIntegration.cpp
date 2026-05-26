#include "os/Endian.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/Timer.h"
#include "obj/DataFile.h"
#include "synth/Synth.h"
#include "utl/EncryptXTEA.h"
#include "utl/MemMgr.h"
#include "KeyChain.h"
#include <cstring>
#include <cstdio>

CriticalSection gCrit;

struct BINKENCRYPTIONHEADER {
    unsigned int mSignature;       // 0x00
    unsigned int mVersion;         // 0x04
    unsigned int mKeyIndex;        // 0x08
    unsigned int mMagicA;          // 0x0c
    unsigned int mMagicB;          // 0x10
    unsigned int mPad;             // 0x14 (align mNonce to 8-byte)
    unsigned long long mNonce[2];  // 0x18
    unsigned char mKeyMask[0x10];  // 0x28
};

// DC3 BINKIO struct — all IO state (file ptr, buffer, encryption) is in this flat struct.
// Offsets match what BinkFileOpen/BinkFileSetInfo/BinkFileClose/BinkFileBGControl use.
struct BINKIO {
    unsigned int (*ReadHeader)(struct BINKIO *, int, void *, unsigned int); // 0x00
    unsigned int (*ReadFrame)(struct BINKIO *, unsigned int, int, void *, unsigned int); // 0x04
    unsigned int (*GetBufferSize)(struct BINKIO *, unsigned int); // 0x08
    void (*SetInfo)(struct BINKIO *, void *, unsigned int, unsigned int, unsigned int); // 0x0c
    unsigned int (*Idle)(struct BINKIO *); // 0x10
    void (*Close)(struct BINKIO *); // 0x14
    int (*BGControl)(struct BINKIO *, unsigned int); // 0x18
    struct BINK *bink;             // 0x1c
    unsigned int unk20;            // 0x20
    unsigned int unk24;            // 0x24
    unsigned int unk28;            // 0x28
    unsigned int unk2c;            // 0x2c
    unsigned int unk30;            // 0x30
    unsigned int unk34;            // 0x34
    unsigned int unk38;            // 0x38
    unsigned int unk3c;            // 0x3c
    unsigned int ReadError;        // 0x40
    unsigned int DoingARead;       // 0x44
    unsigned int BytesRead;        // 0x48
    unsigned int unk4c;            // 0x4c
    unsigned int ForegroundTime;   // 0x50
    unsigned int TotalTime;        // 0x54
    unsigned int unk58;            // 0x58
    unsigned int unk5c;            // 0x5c
    unsigned int BufSize;          // 0x60
    unsigned int BufHighUsed;      // 0x64
    unsigned int CurBufSize;       // 0x68
    unsigned int bytesAvail;       // 0x6c
    unsigned int Suspended;        // 0x70
    unsigned int unk74;            // 0x74
    unsigned int unk78;            // 0x78
    unsigned int unk7c;            // 0x7c
    File *pFile;                   // 0x80
    unsigned int iCloseFile;       // 0x84
    unsigned char *pBuffer;        // 0x88
    unsigned char *pBufEnd;        // 0x8c
    unsigned char *pBufPos;        // 0x90
    unsigned char *pBufBack;       // 0x94
    unsigned int iBufEmpty;        // 0x98
    unsigned int iFileBufPos;      // 0x9c
    unsigned int fileFlags;        // 0xa0
    unsigned int lastTimerRead;    // 0xa4
    unsigned int iHeaderSize;      // 0xa8
    unsigned int unkac;            // 0xac
    BINKENCRYPTIONHEADER mEncHeader; // 0xb0 (size 0x38: mSignature=0xb0, mVersion=0xb4, mKeyIndex=0xb8, mMagicA=0xbc, mMagicB=0xc0, mPad=0xc4, mNonce[0]=0xc8, mNonce[1]=0xd0, mKeyMask=0xd8)
    XTEABlockEncrypter *pXTEADecrypter; // 0xe8
};

struct BINK {
    unsigned int Width;   // 0x00
    unsigned int Height;  // 0x04
    unsigned int Frames;  // 0x08
    unsigned int FrameNum; // 0x0c
    char padding[0x28];
    int NumTracks;        // 0x38
};

#ifdef HX_NATIVE
// Bink SDK not available on native — stub all proprietary functions
void ReadFunc(BINKIO *, bool) {}
void BinkFree(void *) {}
unsigned int BinkFileReadHeader(BINKIO *, int, void *, unsigned int) { return 0; }
unsigned int BinkFileReadFrame(BINKIO *, unsigned int, int, void *, unsigned int) { return 0; }
void BinkSetMemory(void *(*)(unsigned int), void (*)(void *)) {}
void BinkSetIO(int (*)(BINKIO *, const char *, unsigned int)) {}
#else
extern void BinkFree(void *);
extern void BinkSetMemory(void *(*)(unsigned int), void (*)(void *));
extern void BinkSetIO(int (*)(BINKIO *, const char *, unsigned int));
extern unsigned int RADTimerRead();
#endif

// Forward declarations for all IO callbacks (referenced by BinkFileBGControl and BinkFileIdle below)
unsigned int BinkFileReadHeader(BINKIO *bink, int, void *header, unsigned int length);
void ReadFunc(BINKIO *bink, bool startRead);
unsigned int BinkFileReadFrame(BINKIO *bink, unsigned int frameOffset, int hasHeader, void *dest, unsigned int length);
unsigned int BinkFileIdle(BINKIO *bink);

template<typename T>
void EndianSwapBlock(T *block, int count) {
    MILO_ASSERT(block != NULL, 0x53);
    MILO_ASSERT(count >= 0, 0x54);

    T *cur = block;
    T *end = block + count;
    if (cur != end) {
        do {
            *cur = EndianSwap(*cur);
            cur++;
        } while (cur != end);
    }
}

// Explicit instantiation for unsigned int
template void EndianSwapBlock<unsigned int>(unsigned int *, int);

int BinkFileBGControl(BINKIO *file, unsigned int flags) {
    char *pByte = (char *)file;
    volatile unsigned int *pControl = (unsigned int *)(pByte + 0x70);
    volatile unsigned int *pStatus = (unsigned int *)(pByte + 0x44);

    if (flags & 1) {
        if (*pControl == 0) {
            *pControl = 1;
        }
        if (flags & 0x80000000) {
            while (*pStatus != 0) {
                // spin
            }
        }
    } else if (flags & 2) {
        if (*pControl == 1) {
            *pControl = 0;
        }
        if (flags & 0x80000000) {
            BinkFileIdle(file);
        }
    }
    return *pControl;
}

void *BinkAlloc(unsigned int size) {
    return MemAlloc(size, "BinkIntegration.cpp", 0x44, "Bink Internal", 0);
}

unsigned int BinkFileGetBufferSize(BINKIO *, unsigned int size) {
    unsigned int aligned = (size + 0x7FFF) & 0xFFFF8000;
    if (aligned < 0x10000) {
        aligned = 0x10000;
    }
    return aligned;
}

void BinkFileSetInfo(BINKIO *file, void *buf, unsigned int size, unsigned int, unsigned int fileFlags) {
    unsigned int aligned = size & 0xFFFF8000;
#ifdef HX_NATIVE
    // Use struct members directly — raw PPC offsets are wrong on LP64
    // (pointers are 8 bytes, so field offsets differ from the 32-bit layout)
    file->pBuffer = (unsigned char *)buf;
    file->pBufEnd = (unsigned char *)buf + aligned;
    file->pBufPos = (unsigned char *)buf;
    file->pBufBack = (unsigned char *)buf;
    file->iBufEmpty = aligned;
    file->BufSize = aligned;
    file->bytesAvail = 0;
    file->fileFlags = fileFlags;
#else
    char *p = (char *)file;
    *(void **)(p + 0x88) = buf;
    *(unsigned int *)(p + 0x8c) = (int)buf + aligned;
    *(void **)(p + 0x90) = buf;
    *(void **)(p + 0x94) = buf;
    *(unsigned int *)(p + 0x98) = aligned;
    *(unsigned int *)(p + 0x60) = aligned;
    *(unsigned int *)(p + 0x6c) = 0;
    *(unsigned int *)(p + 0xa0) = fileFlags;
#endif
}

void BinkFileClose(BINKIO *bink) {
    char *p = (char *)bink;
    if (*(unsigned int *)(p + 0x84) != 0) {
        File *file = *(File **)(p + 0x80);
        if (file != nullptr) {
            delete file;
        }
        *(File **)(p + 0x80) = nullptr;
    }
    if (*(unsigned int *)(p + 0xb4) == 2) {
        operator delete(*(void **)(p + 0xe8));
    }
}

unsigned int BinkFileIdle(BINKIO *bink) {
    char *p = (char *)bink;
    if (*(unsigned int *)(p + 0x40) != 0)
        return 0;
    if (*(unsigned int *)(p + 0x70) != 0)
        return 0;
    if (*(unsigned int *)(p + 0x44) != 0) {
        gCrit.Enter();
        ReadFunc(bink, false);
        gCrit.Exit();
    }
    return *(unsigned int *)(p + 0x44);
}

int BinkFileOpen(BINKIO *bink, const char *name, unsigned int flags) {
    char *p = (char *)bink;
    memset(bink, 0, 0x120);
    if (flags & 0x800000) {
        *(const char **)(p + 0x80) = name;
    } else {
        File *file = NewFile(name, 2);
        *(File **)(p + 0x80) = file;
        *(int *)(p + 0x84) = 1;
        if (file == nullptr)
            return 0;
    }
    *(void **)(p + 0x00) = (void *)BinkFileReadHeader;
    *(void **)(p + 0x04) = (void *)BinkFileReadFrame;
    *(void **)(p + 0x08) = (void *)BinkFileGetBufferSize;
    *(void **)(p + 0x0c) = (void *)BinkFileSetInfo;
    *(void **)(p + 0x10) = (void *)BinkFileIdle;
    *(void **)(p + 0x14) = (void *)BinkFileClose;
    *(void **)(p + 0x18) = (void *)BinkFileBGControl;
    return 1;
}

void BinkInit() {
    BinkSetMemory(BinkAlloc, operator delete);
    BinkSetIO(BinkFileOpen);
}

#ifndef HX_NATIVE
unsigned int BinkFileReadHeader(BINKIO *bink, int, void *header, unsigned int length) {
    File **ppFile = &bink->pFile;
    File *file = *ppFile;
    BINKENCRYPTIONHEADER *encHeader = (BINKENCRYPTIONHEADER *)((char *)ppFile + 0x30);
    // If we haven't read the encryption header yet (mSignature == 0), read it now
    if (encHeader->mSignature == 0) {
        int encRead = file->Read(encHeader, sizeof(BINKENCRYPTIONHEADER));
        // Byteswap mSignature through mMagicB (5 uints = 0x14 bytes)
        EndianSwapBlock<unsigned int>(&encHeader->mSignature, 5);
        // Byteswap the nonce fields
        unsigned long long n0 = encHeader->mNonce[0];
        unsigned long long n1 = encHeader->mNonce[1];
        encHeader->mNonce[0] = EndianSwap(n0);
        encHeader->mNonce[1] = EndianSwap(n1);
        // Check if this is an encrypted BIK ("BIKE" = 0x4542494b)
        if (encHeader->mSignature == 0x4542494b) {
            XTEABlockEncrypter *decrypter = new XTEABlockEncrypter;
            bink->pXTEADecrypter = decrypter;

            // Key derivation — same DTA obfuscation pattern as VorbisReader::setupCypher
            DataArray *arr = DataReadString("{Na 42 'O32'}");
            unsigned int iEval = arr->Evaluate(0).Int();
            arr->Release();

            char i6 = (iEval % 13) + 'A';
            char script[256];
            unsigned char masterKey[256];
            sprintf(script, "{%c %d %c}", i6, (int)masterKey ^ iEval, i6);
            DataArray *buf118Arr = DataReadString(script);
            buf118Arr->Evaluate(0);
            buf118Arr->Release();

            unsigned char key[0x10];
            KeyChain::getKey(encHeader->mKeyIndex, key, masterKey);
            TheSynth->Grinder().GrindArray(
                encHeader->mMagicA, encHeader->mMagicB, key, 0x10, 0xc
            );
            for (int i = 0; i < 16; i++) {
                key[i] ^= encHeader->mKeyMask[i];
            }

            EndianSwapBlock<unsigned int>((unsigned int *)key, 4);
            bink->pXTEADecrypter->SetKey(key);
            bink->pXTEADecrypter->SetNonce(encHeader->mNonce, 0);
            bink->iFileBufPos += encRead;
        } else {
            // Not an encrypted BIK — seek back and pretend we never read the header
            memset(encHeader, 0, encRead);
            file->Seek(-encRead, FILE_SEEK_CUR);
            BINK *curBink = bink->bink;
            if (curBink != NULL && curBink->NumTracks > 2 && curBink->Width < 8) {
                MILO_LOG("Attempting read of unsecure Bink song file!\n");
            }
        }
    }
    // Read the actual Bink file header
    unsigned int bytesRead = (unsigned int)file->Read(header, length);
    if (bytesRead != length) {
        bink->ReadError = 1;
    }
    bink->iHeaderSize += bytesRead;
    bink->iFileBufPos += bytesRead;
    int remaining = file->Size() - (int)bink->iFileBufPos;
    if ((unsigned int)remaining >= bink->BufSize) {
        remaining = (int)bink->BufSize;
    }
    bink->CurBufSize = (unsigned int)remaining;
    EndianSwapBlock<unsigned int>((unsigned int *)header, bytesRead >> 2);
    return bytesRead;
}

void ReadFunc(BINKIO *bink, bool startRead) {
    File **ppFile = &bink->pFile;
    // If an async read was in progress, check if it's done
    if (bink->DoingARead != 0) {
        int bytesRead = 0;
        if (!(*ppFile)->ReadDone(bytesRead))
            return;
        bink->DoingARead = 0;
        if (bink->mEncHeader.mVersion == 2) {
            static Timer *_t = AutoTimer::GetTimer(Symbol("XTEA"));
            XTEABlock temp;
            AutoTimer _at(_t, 50.0f, nullptr, nullptr);
            // Decrypt the buffer data in-place using XTEA block cipher
            XTEABlock *block = (XTEABlock *)bink->pBufBack;
            while (block < (XTEABlock *)((unsigned char *)bink->pBufBack + bytesRead)) {
                block->mData[0] = EndianSwap(block->mData[0]);
                block->mData[1] = EndianSwap(block->mData[1]);
                bink->pXTEADecrypter->Encrypt(block, &temp);
                unsigned int *dst = (unsigned int *)block;
                const unsigned int *src = (const unsigned int *)&temp;
                dst[0] = src[1]; dst[1] = src[0];
                dst[2] = src[3]; dst[3] = src[2];
                block++;
            }
        } else {
            EndianSwapBlock<unsigned int>((unsigned int *)bink->pBufBack, (unsigned int)bytesRead >> 2);
        }
        // Advance pBufBack by bytesRead, wrapping at pBufEnd back to pBuffer
        unsigned int uBytesRead = (unsigned int)bytesRead;
        bink->pBufBack += uBytesRead;
        if (bink->pBufBack >= bink->pBufEnd) {
            bink->pBufBack = bink->pBuffer;
        }
        bink->iBufEmpty -= uBytesRead;
        bink->bytesAvail += uBytesRead;
        bink->BytesRead += uBytesRead;
        if (bink->bytesAvail > bink->BufHighUsed) {
            bink->BufHighUsed = bink->bytesAvail;
        }
        int now = RADTimerRead();
        int elapsed = now - (int)bink->lastTimerRead;
        bink->lastTimerRead = elapsed;
        bink->ForegroundTime += (unsigned int)elapsed;
        if (bink->Suspended != 0) {
            return;
        }
    }
    if (startRead) {
        int fileSize = (*ppFile)->Size();
        int fileTell = (*ppFile)->Tell();
        unsigned int remaining = (unsigned int)(fileSize - fileTell);
        if (bink->iBufEmpty < 0x8000 || (*ppFile)->Eof()) {
            bink->CurBufSize = bink->bytesAvail;
        } else {
            bink->DoingARead = 1;
            if (remaining > 0x8000)
                remaining = 0x8000;
            (*ppFile)->ReadAsync(bink->pBufBack, (int)remaining);
        }
    }
}

unsigned int BinkFileReadFrame(BINKIO *bink, unsigned int frameOffset, int hasHeader, void *dest, unsigned int length) {
    unsigned int bytesReturned = 0;
    gCrit.Enter();
    if (bink->ReadError != 0) {
        gCrit.Exit();
        return 0;
    }
    // If encrypted, skip the 0x38-byte encryption header when computing offset
    unsigned int adjOffset = frameOffset;
    if (bink->mEncHeader.mSignature != 0) adjOffset += 0x38;
    // Check if the file has enough data
    unsigned int fileSize = (unsigned int)bink->pFile->Size();
    if (adjOffset + length > fileSize) {
        bink->ReadError = 1;
        bytesReturned = 0;
        gCrit.Exit();
        return bytesReturned;
    }
    {
        int startTimer = RADTimerRead();
        unsigned int seekPos = adjOffset;
        // If frame is not at the current file position, seek/skip
        unsigned int blockOff = 0;
        if ((int)seekPos != -1 && seekPos != bink->iFileBufPos) {
            bytesReturned = 0;
            if (seekPos > bink->iFileBufPos) {
                // Target is ahead — can we satisfy from buffered data?
                int fileTell = bink->pFile->Tell();
                if ((int)seekPos <= fileTell) {
                    // Advance buffer read position to skip data
                    unsigned int advance = seekPos - bink->iFileBufPos;
                    bink->pBufPos += advance;
                    if (bink->pBufPos > bink->pBufEnd) {
                        bink->pBufPos -= bink->BufSize;
                    }
                    bink->iBufEmpty += advance;
                    bink->bytesAvail -= advance;
                } else {
                    // Need full seek — flush buffer state
                    while (bink->DoingARead != 0) {
                        ReadFunc(bink, false);
                    }
                    unsigned char *pBuf = bink->pBuffer;
                    bink->bytesAvail = 0;
                    bink->iBufEmpty = bink->BufSize;
                    bink->pBufPos = pBuf;
                    bink->pBufBack = pBuf;
                    if (bink->mEncHeader.mVersion == 2) {
                        // Align to XTEA block boundary
                        unsigned int rawOff = seekPos - bink->iHeaderSize - 0x38;
                        blockOff = rawOff & 0xf;
                        bink->pBufPos = pBuf + blockOff;
                        seekPos = (rawOff & 0xfffffff0) + bink->iHeaderSize + 0x38;
                        bink->pXTEADecrypter->SetNonce(bink->mEncHeader.mNonce, rawOff >> 4);
                    }
                    bink->pFile->Seek((int)seekPos, FILE_SEEK_SET);
                    bink->DoingARead = 0;
                }
            }
            bink->iFileBufPos = blockOff + seekPos;
        }
        if (bink->pBuffer == nullptr) {
            // No buffer — direct synchronous read
            int readStart = RADTimerRead();
            unsigned int nr = (unsigned int)bink->pFile->Read(dest, (int)length);
            bytesReturned = nr;
            if (nr < length) {
                bink->ReadError = 1;
            }
            bink->BytesRead += bytesReturned;
            bink->iFileBufPos += bytesReturned;
            int readEnd = RADTimerRead();
            *(volatile unsigned int *)&bink->ForegroundTime += (unsigned int)(readEnd - readStart);
            *(volatile unsigned int *)&bink->ForegroundTime += (unsigned int)(readEnd - startTimer);
            EndianSwapBlock<unsigned int>((unsigned int *)dest, bytesReturned >> 2);
        } else {
            // Buffered async read loop
            unsigned char *destPtr = (unsigned char *)dest;
            unsigned int remaining = length;
            while (remaining > 0 && bink->ReadError == 0) {
                ReadFunc(bink, true);
                unsigned int avail = bink->bytesAvail;
                if (avail > remaining)
                    avail = remaining;
                if (avail > 0) {
                    bink->iFileBufPos += avail;
                    remaining -= avail;
                    bytesReturned += avail;
                    // Handle circular buffer wrap
                    unsigned int toEnd = (unsigned int)(bink->pBufEnd - bink->pBufPos);
                    if (toEnd <= avail) {
                        memcpy(destPtr, bink->pBufPos, toEnd);
                        destPtr += toEnd;
                        avail -= toEnd;
                        bink->iBufEmpty += toEnd;
                        bink->pBufPos = bink->pBuffer;
                        bink->bytesAvail -= toEnd;
                    }
                    if (avail > 0) {
                        memcpy(destPtr, bink->pBufPos, avail);
                        destPtr += avail;
                        bink->iBufEmpty += avail;
                        bink->pBufPos += avail;
                        bink->bytesAvail -= avail;
                    }
                }
            }
            int threadEnd = RADTimerRead();
            bink->TotalTime += (unsigned int)(threadEnd - startTimer);
        }
        // Update CurBufSize for Bink SDK flow control
        unsigned int newAvail = (unsigned int)(bink->pFile->Size() - (int)bink->iFileBufPos);
        if (newAvail >= bink->BufSize) {
            newAvail = bink->BufSize;
        }
        bink->CurBufSize = newAvail;
        if (bink->bytesAvail + 0x8000 > bink->CurBufSize) {
            bink->CurBufSize = bink->bytesAvail;
        }
    }
    gCrit.Exit();
    return bytesReturned;
}
#endif
