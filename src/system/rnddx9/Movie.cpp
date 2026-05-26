#include "rnddx9/Movie.h"
#include "Tex.h"
#include "os/Debug.h"
#include "os/File.h"
#include "rndobj/Movie.h"
#include "rndobj/Tex.h"
#include "utl/BufStream.h"
#include "utl/FilePath.h"
#include "utl/FileStream.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "rndobj/Utl.h"
#include "xdk/d3d9i/d3d9.h"
#include "xdk/d3d9i/d3d9types.h"

DxMovie::DxMovie() : mFrameBuf(0), mStream(0) {}

DxMovie::~DxMovie() {
    RELEASE(mStream);
    if (mFrameBuf) {
        MemFree(mFrameBuf, __FILE__, 0x1F);
        mFrameBuf = nullptr;
    }
}

void DxMovie::StreamReadFinish() {
    while (!mStream->ReadDone())
        ;
}

void DxMovie::SetFile(const FilePath &file, bool stream) {
    RELEASE(mStream);
    if (mFrameBuf) {
        MemFree(mFrameBuf, __FILE__, 0x2B);
        mFrameBuf = nullptr;
    }
    mVideo.Reset();
    RndMovie::SetFile(file, stream);
    if (!mFile.empty()) {
        if (stream) {
            mStream = NewFile(CacheResource(mFile.c_str(), this), 2);
            if (!mStream) {
                MILO_NOTIFY("%s: %s not found", PathName(this), (const String &)mFile);
                return;
            }
            FileStream fStream(mStream, true);
            mVideo.Load(fStream, true);
            void *frames =
                MemAlloc(mVideo.FrameSize() * 2, __FILE__, 0x44, "RndMovie frames");
            mBufOffset = 0;
            mFrameBuf = frames;
            mReadPtr = frames;
            mStreamDataOffset = fStream.Tell();
        } else {
            FileLoader *fl = dynamic_cast<FileLoader *>(TheLoadMgr.ForceGetLoader(file));
            char *buffer;
            int size;
            if (fl) {
                buffer = fl->GetBuffer(&size);
                delete fl;
            } else {
                buffer = nullptr;
            }
            if (!buffer)
                return;
            BufStream bStream(buffer, size, true);
            mVideo.Load(bStream, false);
            MemFree(buffer, __FILE__, 0x58);
        }
        mNumFrames = mVideo.NumFrames();
        SetTex(mTex);
        SetFrame(0, 1);
    }
}

void DxMovie::SetTex(RndTex *tex) {
    RndMovie::SetTex(tex);
    if (mTex) {
        if (mVideo.Width() && mVideo.Height() && mVideo.NumFrames()) {
            mTex->SetBitmap(
                mVideo.Width(),
                mVideo.Height(),
                mVideo.Bpp(),
                RndTex::kMovie,
                false,
                nullptr
            );
        } else {
            mTex->SetBitmap(0x10, 0x10, 0x20, RndTex::kRegular, false, nullptr);
        }
    }
}

void DxMovie::Update() {
    DxTex *tex = static_cast<DxTex *>(mTex.Ptr());
    D3DSurface *surface = tex->GetMovieSurface();
    tex->SwapMovieSurface();
    if (surface) {
        D3DLOCKED_RECT lock;
        int bpp = mVideo.Bpp();
        int srcPitch = bpp * mVideo.Width() * 4;
        D3DSurface_LockRect(surface, &lock, nullptr, 0);
        if (srcPitch == lock.Pitch) {
            memcpy(lock.pBits, mReadPtr, mVideo.FrameSize());
        } else {
            MILO_ASSERT(srcPitch < lock.Pitch, 0xB5);
            char *curBits = (char *)lock.pBits;
            char *c58 = (char *)mReadPtr;
            for (int i = mVideo.FrameSize(); i > 0; i -= srcPitch) {
                memcpy(curBits, c58, srcPitch);
                c58 += srcPitch;
                curBits += lock.Pitch;
            }
        }
        D3DSurface_UnlockRect(surface);
        D3DResource_Release(surface);
    }
}

int DxMovie::StreamChunkSize() {
    return Min(mVideo.FrameSize(), mStream->Size() - mStream->Tell());
}

void DxMovie::StreamNextBuffer() {
    StreamReadFinish();
    mBufOffset = mBufOffset ? 0 : mVideo.FrameSize();
    mStream->ReadAsync((char *)mFrameBuf + mBufOffset, StreamChunkSize());
}

void DxMovie::StreamRestart(int frame) {
    StreamReadFinish();
    mStream->Seek(mVideo.FrameSize() * frame + mStreamDataOffset, FILE_SEEK_SET);
    mStream->Read(mFrameBuf, StreamChunkSize());
    mBufOffset = 0;
    mReadPtr = mFrameBuf;
    StreamNextBuffer();
}
