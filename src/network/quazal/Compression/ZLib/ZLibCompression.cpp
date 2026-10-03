// Quazal NetZ - .\Compression\ZLib\ZLibCompression.cpp
//
// The retail TU is 0x82B1B660..0x82B1BE58: the two zlib allocator hooks, the
// constructor, the destructor, CompressImpl, DecompressImpl and the scalar
// deleting destructor. 0x82B1B600 before it is the previous TU's deleting
// destructor (its vtable 0x8218A0E8 precedes this TU's file string), and
// 0x82B1BE58 is the EH prefix of the next TU's first function. The .rdata is
// 0x8218A0F8..0x8218A1F0: the file string, "1.2.1" twice (no string pooling
// under /Od), the constructor's and destructor's FuncInfo, and the vtable
// 0x8218A1E0 (deleting destructor, CompressImpl, DecompressImpl).
//
// Built /Od /Oi- /Ob1 /GR-: EH stays on (the constructor and destructor
// unwind the CompressionAlgorithm base), and no RTTI locator precedes the
// vtable.
//
// The classes are declared here only as far as this TU uses them.

#include "zlib/zlib.h"
#include <string.h>

extern "C" {
void *QuazalCRTCalloc(unsigned int, unsigned int);
void QuazalCRTFree(void *);
}

namespace Quazal {

    class RootObject {
    public:
        ~RootObject() {}
        static void *operator new(unsigned int, const char *, unsigned int);
        static void operator delete(void *);
        static void operator delete(void *, const char *, unsigned int);
    };

    class Buffer : public RootObject {
    public:
        unsigned int GetContentSize() const;
        unsigned char *GetContentPtr() const;
        void SetContentSize(unsigned int);
        unsigned int GetSize() const;
        void Resize(unsigned int);
    };

    class CompressionAlgorithm : public RootObject {
    public:
        CompressionAlgorithm();
        virtual ~CompressionAlgorithm();
        virtual bool CompressImpl(const Buffer &, Buffer *) = 0;
        virtual int DecompressImpl(const Buffer &, Buffer *) = 0;

        int unk4; // 0x4
        unsigned int mUncompressedDataSize; // 0x8
        unsigned int mCompressedDataSize; // 0xc
        char mCSCompression[0x18]; // 0x10
        char m_puCompression[0x48]; // 0x28
        char m_puDecompression[0x48]; // 0x70
    };

    // The inline constructor is what gives retail's `new` its third temp.
    struct ZLibStreams : public RootObject {
        ZLibStreams() {}

        z_stream inflate_stream; // 0x0
        z_stream deflate_stream; // 0x38
    };

    class ZLibCompression : public CompressionAlgorithm {
    public:
        ZLibCompression();
        virtual ~ZLibCompression();
        virtual bool CompressImpl(const Buffer &, Buffer *);
        virtual int DecompressImpl(const Buffer &, Buffer *);

        ZLibStreams *mStreams; // 0xb8
    };

}

extern "C" {

void *QuazalCZlibAlloc(void *opaque, unsigned int items, unsigned int size) {
    return QuazalCRTCalloc(items, size);
}

void QuazalCZlibFree(void *opaque, void *address) { QuazalCRTFree(address); }

}

namespace Quazal {

    ZLibCompression::ZLibCompression() {
        mStreams = new (__FILE__, 0x78) ZLibStreams;
        mStreams->inflate_stream.zalloc = (alloc_func)QuazalCZlibAlloc;
        mStreams->inflate_stream.zfree = (free_func)QuazalCZlibFree;
        mStreams->inflate_stream.opaque = 0;
        inflateInit(&mStreams->inflate_stream);
        mStreams->deflate_stream.zalloc = (alloc_func)QuazalCZlibAlloc;
        mStreams->deflate_stream.zfree = (free_func)QuazalCZlibFree;
        mStreams->deflate_stream.opaque = 0;
        deflateInit(&mStreams->deflate_stream, Z_DEFAULT_COMPRESSION);
    }

    ZLibCompression::~ZLibCompression() {
        inflateEnd(&mStreams->inflate_stream);
        deflateEnd(&mStreams->deflate_stream);
        delete mStreams;
        mStreams = 0;
    }

    // Byte 0 of the output is the compression ratio (0 = stored).
    bool ZLibCompression::CompressImpl(const Buffer &in, Buffer *out) {
        deflateReset(&mStreams->deflate_stream);
        if (out->GetSize() < in.GetContentSize() * 1.001f + 12.0f) {
            out->Resize((int)(in.GetContentSize() * 1.001f + 12.0f) + 1);
        }
        mStreams->deflate_stream.avail_out = out->GetSize();
        mStreams->deflate_stream.next_out = out->GetContentPtr();
        mStreams->deflate_stream.avail_in = in.GetContentSize();
        mStreams->deflate_stream.next_in = in.GetContentPtr();
        mStreams->deflate_stream.next_out++;
        mStreams->deflate_stream.avail_out--;
        deflate(&mStreams->deflate_stream, Z_FINISH);
        if (mStreams->deflate_stream.total_out < in.GetContentSize()) {
            float fRatio =
                in.GetContentSize() / (float)mStreams->deflate_stream.total_out + 1.0f;
            unsigned char ucRatio = 0;
            if (fRatio > 255.0f) {
                ucRatio = 255;
            } else {
                ucRatio = fRatio;
            }
            out->SetContentSize(mStreams->deflate_stream.total_out + 1);
            *out->GetContentPtr() = ucRatio;
        } else {
            unsigned char *pDest = out->GetContentPtr();
            out->SetContentSize(in.GetContentSize() + 1);
            *pDest = 0;
            pDest++;
            memcpy(pDest, in.GetContentPtr(), in.GetContentSize());
        }
        return false;
    }

    // Returns nonzero when inflate does not reach Z_STREAM_END. The local
    // names are load-bearing: /Od lays locals out by a walk over their names,
    // and these four reproduce retail's 0x50/0x54/0x58/0x5c slots.
    int ZLibCompression::DecompressImpl(const Buffer &in, Buffer *out) {
        int res = 0;
        unsigned char ucRatio = *in.GetContentPtr();
        if (ucRatio != 0) {
            int err = 0;
            bool bDone = false;
            if (out->GetSize() < in.GetContentSize() * ucRatio) {
                out->Resize(in.GetContentSize() * ucRatio);
            }
            do {
                inflateReset(&mStreams->inflate_stream);
                mStreams->inflate_stream.avail_out = out->GetSize();
                mStreams->inflate_stream.next_out = out->GetContentPtr();
                mStreams->inflate_stream.avail_in = in.GetContentSize();
                mStreams->inflate_stream.next_in = in.GetContentPtr();
                mStreams->inflate_stream.next_in++;
                mStreams->inflate_stream.avail_in--;
                err = inflate(&mStreams->inflate_stream, Z_FINISH);
                if ((mStreams->inflate_stream.avail_out == 0 && err == Z_OK)
                    || err == Z_BUF_ERROR) {
                    out->Resize(out->GetSize() * 2);
                } else {
                    bDone = true;
                }
            } while (!bDone);
            if (err == Z_STREAM_END) {
                out->SetContentSize(mStreams->inflate_stream.total_out);
            } else {
                res = 1;
            }
        } else {
            unsigned char *pSrc = in.GetContentPtr();
            out->SetContentSize(in.GetContentSize() - 1);
            pSrc++;
            memcpy(out->GetContentPtr(), pSrc, in.GetContentSize() - 1);
        }
        return res;
    }

}
