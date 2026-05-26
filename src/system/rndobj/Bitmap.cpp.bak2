#include "rndobj/Bitmap.h"
#include "utl/CRC.h"
#include "utl/BufStream.h"
#include "utl/ChunkStream.h"
#include "utl/FileStream.h"
#include "utl/MemMgr.h"
#include "os/Endian.h"

unsigned char BITMAP_REV = 2;

int RndBitmap::NumMips() const {
    const RndBitmap *x = this;
    int i;
    for (i = 0; x->mMip; i++)
        x = x->mMip;
    return i;
}

int RndBitmap::PixelBytes() const { return mRowBytes * mHeight; }
int RndBitmap::DxtRowBytes() const { return mOrder & 0x38 ? mRowBytes * 4 : mRowBytes; }

unsigned char RndBitmap::PixelIndex(int i1, int i2) const {
    bool bb;
    int offset = PixelOffset(i1, i2, bb);
    u8 *p = mPixels + offset;
    unsigned char ret;
    if (mBpp == 8) {
        return *p;
    }
    ret = *p >> 4;
    if (!bb) {
        ret = *p & 0xF;
    }
    return ret;
}

void RndBitmap::SetName(const Hmx::CRC &crc) { mName = crc; }

BinStream &RndBitmap::LoadHeader(BinStream &bs, u8 &numMips) {
    u8 rev, h;
    u8 pad[32];
    bs.Tell();
    bs >> rev;
    if (rev > 1)
        bs >> mName;
    bs >> mBpp;
    if (rev > 0)
        bs >> mOrder;
    else {
        bs >> h;
        mOrder = h;
    }
    bs >> numMips;
    bs >> mWidth;
    bs >> mHeight;
    bs >> mRowBytes;

    int count;
    if (rev == 0) {
        count = 6;
    } else
        count = rev == 1 ? 0x13 : 0xF;
    bs.Read(pad, count);
    return bs;
}

BinStream &RndBitmap::SaveHeader(BinStream &bs) const {
    static u8 pad[0xf];
    bs << BITMAP_REV << mName << mBpp << (unsigned int)mOrder << (unsigned char)NumMips()
       << mWidth << mHeight;
    bs << mRowBytes;
    bs.Write(pad, 0xf);
    return bs;
}

BinStream &operator>>(BinStream &bs, tagBITMAPFILEHEADER &bmfh) {
    bs >> bmfh.bfSize;
    bs >> bmfh.bfReserved1;
    bs >> bmfh.bfReserved2;
    bs >> bmfh.bfOffBits;
    return bs;
}

BinStream &operator<<(BinStream &bs, const tagBITMAPFILEHEADER &bmfh) {
    bs << bmfh.bfSize << bmfh.bfReserved1 << bmfh.bfReserved2 << bmfh.bfOffBits;
    return bs;
}

BinStream &operator>>(BinStream &bs, tagBITMAPINFOHEADER &bmih) {
    bs >> bmih.biSize;
    bs >> bmih.biWidth;
    bs >> bmih.biHeight;
    bs >> bmih.biPlanes;
    bs >> bmih.biBitCount;
    bs >> bmih.biCompression;
    bs >> bmih.biSizeImage;
    bs >> bmih.biXPelsPerMeter;
    bs >> bmih.biYPelsPerMeter;
    bs >> bmih.biClrUsed;
    bs >> bmih.biClrImportant;
    return bs;
}

BinStream &operator<<(BinStream &bs, const tagBITMAPINFOHEADER &bmih) {
    bs << bmih.biSize << bmih.biWidth << bmih.biHeight << bmih.biPlanes << bmih.biBitCount
       << bmih.biCompression << bmih.biSizeImage << bmih.biXPelsPerMeter
       << bmih.biYPelsPerMeter << bmih.biClrUsed << bmih.biClrImportant;
    return bs;
}

void RndBitmap::Reset() {
    mName.Reset();
    mRowBytes = 0;
    mHeight = 0;
    mWidth = 0;
    mBpp = 32;
    mOrder = 1;
    mPalette = nullptr;
    mPixels = nullptr;
    if (mBuffer) {
        MemFree(mBuffer, __FILE__, 0x16C);
        mBuffer = nullptr;
    }
    RELEASE(mMip);
}

bool RndBitmap::LoadBmp(BinStream *bs) {
    unsigned short us;
    *bs >> us;
    if (us != 0x4D42) {
        MILO_NOTIFY("%s not BMP format", bs->Name());
        return false;
    } else {
        tagBITMAPFILEHEADER header;
        *bs >> header;
        return LoadDIB(bs, header.bfOffBits);
    }
}

void RndBitmap::SaveBmp(BinStream *file) const {
    MILO_ASSERT(file, 0x515);
    if (mOrder & 1) {
        MILO_NOTIFY("Order isn't kARGB");
    } else {
        SaveBmpHeader(file);
        SaveBmpPixels(file);
    }
}

void RndBitmap::SaveBmp(const char *cc) const {
    FileStream *file = new FileStream(cc, FileStream::kWrite, true);
    SaveBmp(file);
    delete file;
}

int RndBitmap::PaletteBytes() const {
    if (mBpp <= 8) {
        if ((mOrder & 0x38) == 0) {
            return (1 << mBpp) * 4;
        }
    }
    return 0;
}

void RndBitmap::AllocateBuffer() {
    int paletteBytes;
    if (mPalette)
        paletteBytes = 0;
    else
        paletteBytes = PaletteBytes();
    int sum = paletteBytes + PixelBytes();
    if (sum)
        mBuffer = (u8 *)MemAlloc(sum, __FILE__, 0x1A9, "Bitmap_buf");
    if (paletteBytes)
        mPalette = mBuffer;
    mPixels = mBuffer + paletteBytes;
}

void RndBitmap::Create(
    int width,
    int height,
    int rowlen,
    int bpp,
    int order,
    void *palette,
    void *pixels,
    void *buf
) {
    MILO_ASSERT(width >= 0 && height >= 0, 0x1BA);
    MILO_ASSERT(bpp == 4 || bpp == 8 || bpp == 16 || bpp == 24 || bpp == 32, 0x1BB);
    mWidth = width;
    mHeight = height;
    mRowBytes = rowlen;
    mBpp = bpp;
    mOrder = order;
    mPixels = (u8 *)pixels;
    mPalette = (u8 *)palette;
    RELEASE(mMip);
    if (mRowBytes == 0) {
        if (mBpp == 4 && mWidth & 1) {
            mRowBytes = (mWidth + 1) * mBpp >> 3;
        } else
            mRowBytes = mWidth * mBpp >> 3;
    }
    if (mOrder & 4) {
        unsigned char theBpp = mBpp;
        if ((theBpp == 8 && (mWidth < 16 || mHeight < 16))
            || (theBpp == 4 && (mWidth < 32 || mHeight < 16)) || theBpp > 8) {
            mOrder &= ~0x4;
        }
    }
    if (mBuffer) {
        MemFree(mBuffer, __FILE__, 0x1DF);
        mBuffer = 0;
    }
    if (buf)
        mBuffer = (u8 *)buf;
    else if (!pixels)
        AllocateBuffer();
}

void RndBitmap::Create(const RndBitmap &bm, int bpp, int order, void *palette) {
    Create(bm.Width(), bm.Height(), 0, bpp, order, palette, NULL, NULL);
    if (mPalette && !palette) {
        MILO_ASSERT(bm.Palette(), 0x1EE);
        for (int i = 0; i < bm.NumPaletteColors(); i++) {
            unsigned char r, g, b, a;
            bm.PaletteColor(i, r, g, b, a);
            SetPaletteColor(i, r, g, b, a);
        }
    }
    Blt(bm, 0, 0, 0, 0, mWidth, mHeight);
    if (bm.nextMip()) {
        mMip = new RndBitmap();
        mMip->Create(*bm.nextMip(), bpp, order, mPalette);
    }
}

void RndBitmap::Create(void *buffer) {
    if (!buffer)
        MILO_NOTIFY("Load buffer is empty");
    else {
        BufStream bs(buffer, 32, true);
        unsigned char buf;
        LoadHeader(bs, buf);
        if (mBuffer) {
            MemFree(mBuffer, __FILE__, 0x203);
            mBuffer = 0;
        }
        mBuffer = (u8 *)buffer;
        u8 *i5 = mBuffer + bs.Tell();

        int pbytes = PaletteBytes();
        mPalette = pbytes ? i5 : 0;

        int pixbytes = PixelBytes();
        mPixels = i5 + pbytes;
        u8 *pixels = i5 + pbytes + pixbytes;
        RELEASE(mMip);
        int width = mWidth;
        int height = mHeight;
        RndBitmap *cur = this;
        while (buf-- != 0) {
            cur->mMip = new RndBitmap();
            cur = cur->mMip;
            width >>= 1;
            height >>= 1;
            pixbytes >>= 2;
            cur->Create(width, height, 0, mBpp, mOrder, mPalette, pixels, 0);
            pixels += pixbytes;
        }
    }
}

void RndBitmap::SetPixelIndex(int i1, int i2, unsigned char uc) {
    bool bb;
    int offset = PixelOffset(i1, i2, bb);
    u8 *pixels = mPixels;
    if (mBpp == 8) {
        *(pixels + offset) = uc;
    } else if (bb) {
        *(pixels + offset) = uc << 4 | *(pixels + offset) & 0xF;
    } else {
        *(pixels + offset) = *(pixels + offset) & 0xF0 | uc;
    }
}

int RndBitmap::PixelOffset(int x, int y, bool &nibble) const {
    static char bytes02[64] = {
        0x0,  0x4,  0x8,  0xC,  0x10, 0x14, 0x18, 0x1c, 0x2,  0x6,  0xa,  0xe,  0x12,
        0x16, 0x1a, 0x1e, 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38, 0x3c, 0x22, 0x26,
        0x2a, 0x2e, 0x32, 0x36, 0x3a, 0x3e, 0x11, 0x15, 0x19, 0x1d, 0x1,  0x5,  0x9,
        0xd,  0x13, 0x17, 0x1b, 0x1f, 0x3,  0x7,  0xb,  0xf,  0x31, 0x35, 0x39, 0x3d,
        0x21, 0x25, 0x29, 0x2d, 0x33, 0x37, 0x3b, 0x3f, 0x23, 0x27, 0x2b, 0x2f
    };
    static char bytes13[64] = {
        0x10, 0x14, 0x18, 0x1c, 0x0,  0x4,  0x8,  0xc,  0x12, 0x16, 0x1a, 0x1e, 0x2,
        0x6,  0xa,  0xe,  0x30, 0x34, 0x38, 0x3c, 0x20, 0x24, 0x28, 0x2c, 0x32, 0x36,
        0x3a, 0x3e, 0x22, 0x26, 0x2a, 0x2e, 0x1,  0x5,  0x9,  0xd,  0x11, 0x15, 0x19,
        0x1d, 0x3,  0x7,  0xb,  0xf,  0x13, 0x17, 0x1b, 0x1f, 0x21, 0x25, 0x29, 0x2d,
        0x31, 0x35, 0x39, 0x3d, 0x23, 0x27, 0x2b, 0x2f, 0x33, 0x37, 0x3b, 0x3f
    };
    static char hbytes02[128] = {
        0x0,  0x8,  0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x2,  0xa,  0x12, 0x1a, 0x22,
        0x2a, 0x32, 0x3a, 0x4,  0xc,  0x14, 0x1c, 0x24, 0x2c, 0x34, 0x3c, 0x6,  0xe,
        0x16, 0x1e, 0x26, 0x2e, 0x36, 0x3e, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70,
        0x78, 0x42, 0x4a, 0x52, 0x5a, 0x62, 0x6a, 0x72, 0x7a, 0x44, 0x4c, 0x54, 0x5c,
        0x64, 0x6c, 0x74, 0x7c, 0x46, 0x4e, 0x56, 0x5e, 0x66, 0x6e, 0x76, 0x7e, 0x21,
        0x29, 0x31, 0x39, 0x1,  0x9,  0x11, 0x19, 0x23, 0x2b, 0x33, 0x3b, 0x3,  0xb,
        0x13, 0x1b, 0x25, 0x2d, 0x35, 0x3d, 0x5,  0xd,  0x15, 0x1d, 0x27, 0x2f, 0x37,
        0x3f, 0x7,  0xf,  0x17, 0x1f, 0x61, 0x69, 0x71, 0x79, 0x41, 0x49, 0x51, 0x59,
        0x63, 0x6b, 0x73, 0x7b, 0x43, 0x4b, 0x53, 0x5b, 0x65, 0x6d, 0x75, 0x7d, 0x45,
        0x4d, 0x55, 0x5d, 0x67, 0x6f, 0x77, 0x7f, 0x47, 0x4f, 0x57, 0x5f
    };
    static char hbytes13[128] = {
        0x20, 0x28, 0x30, 0x38, 0x0,  0x8,  0x10, 0x18, 0x22, 0x2a, 0x32, 0x3a, 0x2,
        0xa,  0x12, 0x1a, 0x24, 0x2c, 0x34, 0x3c, 0x4,  0xc,  0x14, 0x1c, 0x26, 0x2e,
        0x36, 0x3e, 0x6,  0xe,  0x16, 0x1e, 0x60, 0x68, 0x70, 0x78, 0x40, 0x48, 0x50,
        0x58, 0x62, 0x6a, 0x72, 0x7a, 0x42, 0x4a, 0x52, 0x5a, 0x64, 0x6c, 0x74, 0x7c,
        0x44, 0x4c, 0x54, 0x5c, 0x66, 0x6e, 0x76, 0x7e, 0x46, 0x4e, 0x56, 0x5e, 0x1,
        0x9,  0x11, 0x19, 0x21, 0x29, 0x31, 0x39, 0x3,  0xb,  0x13, 0x1b, 0x23, 0x2b,
        0x33, 0x3b, 0x5,  0xd,  0x15, 0x1d, 0x25, 0x2d, 0x35, 0x3d, 0x7,  0xf,  0x17,
        0x1f, 0x27, 0x2f, 0x37, 0x3f, 0x41, 0x49, 0x51, 0x59, 0x61, 0x69, 0x71, 0x79,
        0x43, 0x4b, 0x53, 0x5b, 0x63, 0x6b, 0x73, 0x7b, 0x45, 0x4d, 0x55, 0x5d, 0x65,
        0x6d, 0x75, 0x7d, 0x47, 0x4f, 0x57, 0x5f, 0x67, 0x6f, 0x77, 0x7f
    };

    if (mOrder & 4) {
        if (mBpp == 8) {
            int temp_r9 = (int)mRowBytes * 2;
            char *lookup = (((y >> 2) % 4) & 1) ? hbytes13 : hbytes02;
            unsigned char var_r11_3 = lookup[(y % 4) * 0x10 + (x % 16)];
            if ((int)var_r11_3 > 0x1F) {
                var_r11_3 = (var_r11_3 + temp_r9) - 0x20;
            }
            return var_r11_3 + ((((y >> 1) & 0xFFFFFFFE) * temp_r9) + (((x >> 1) * 4) & 0xFFFFFFE0));
        }
        int temp_r10_2 = (y >> 2) % 4;
        int var_r3, var_r8, var_r11_2;
        if ((mWidth > 0x80U) && (mHeight > 0x80U)) {
            var_r3 = (((int)(y - ((y / 128) << 7)) >> 1) & 0xFFFFFFF8) + ((x >> 1) & 0xFFFFFFC0);
            var_r8 = (((int)(x - ((x / 128) << 7)) >> 2) & 0xFFFFFFF8) + ((y >> 2) & 0xFFFFFFE0) + (temp_r10_2 * 2);
            var_r11_2 = (((mHeight - (((int)mHeight / 128) << 7)) & 0xFFFFFFF0) + (mWidth & 0xFFFFFF80)) * 2;
        } else {
            var_r3 = (y >> 1) & 0xFFFFFFF8;
            var_r8 = ((x >> 2) & 0xFFFFFFF8) + (temp_r10_2 * 2);
            var_r11_2 = (int)mHeight * 2;
        }
        char *lookup2 = (temp_r10_2 & 1) ? hbytes13 : hbytes02;
        unsigned char temp_r10_3 = lookup2[(y % 4) << 5 + (x - ((x / 32) << 5))];
        int var_r10_2 = (int)temp_r10_3 >> 1;
        nibble = temp_r10_3 & 1;
        if (var_r10_2 > 0x1F) {
            var_r10_2 = (var_r10_2 + var_r11_2) - 0x20;
        }
        return var_r10_2 + ((var_r11_2 * var_r8) + (var_r3 * 4));
    }
    if (mOrder & 0x40) {
        unsigned char temp_r11_3 = mBpp;
        int var_r10_3 = 8;
        if (temp_r11_3 != 4) {
            var_r10_3 = 4;
        }
        unsigned short temp_r31 = mWidth;
        int temp_r11_4 = temp_r11_3 - 0x10;
        nibble = x & 1;
        int temp_r11_5 = (((temp_r11_4 - temp_r11_4) - !(temp_r11_4 >> 31)) & 4) + 4;
        int temp_r9_2 = (((temp_r11_3 - 0x20) == 0) & 1) + 1;
        int temp_r7_2 = x % temp_r11_5;
        int temp_r8_2 = ((((((int)temp_r31 / temp_r11_5) * (y / var_r10_3)) + (x / temp_r11_5)) * temp_r9_2 * var_r10_3) + (y % var_r10_3)) * temp_r11_5;
        unsigned int temp_r27 = temp_r31 * temp_r9_2;
        int var_r11 = (int)(mBpp * ((temp_r8_2 + temp_r7_2) % temp_r27)) >> (temp_r9_2 + 2);
        int var_r10 = mRowBytes * ((unsigned int)(temp_r8_2 + temp_r7_2) / temp_r27);
        return var_r11 + var_r10;
    }
    nibble = x & 1;
    int var_r11 = (int)(mBpp * x) >> 3;
    int var_r10 = mRowBytes * y;
    return var_r11 + var_r10;
}

unsigned char RndBitmap::NearestColor(
    unsigned char r, unsigned char g, unsigned char b, unsigned char a
) const {
    unsigned char pa, pb, pg, pr;
    int ir = r, ig = g, ib = b, ia = a;
    int paletteColorIdx = -1;
    int minDiff = 0x400;
    for (int i = (1 << mBpp) - 1; i >= 0; i--) {
        int offset = i;
        if ((mOrder & 2) && mBpp == 8) {
            if ((i & 0x18) == 8) {
                offset = i + 8;
            } else if ((i & 0x18) == 0x10) {
                offset = i - 8;
            }
        }
        ConvertColor(mPalette + offset * 4, pr, pg, pb, pa);
        int dr = pr - ir;
        int dg = pg - ig;
        int db = pb - ib;
        int da = pa - ia;
        int diff = abs(dr) + abs(dg) + abs(db) + abs(da);
        if (diff < minDiff) {
            minDiff = diff;
            paletteColorIdx = i;
        }
    }
    return paletteColorIdx;
}

void RndBitmap::ConvertColor(
    const unsigned char *uc,
    unsigned char &r,
    unsigned char &g,
    unsigned char &b,
    unsigned char &a
) const {
    if (mBpp == 0x20 || mPalette) {
        if (mOrder & 1) {
            a = uc[3];
            b = uc[2];
            g = uc[1];
            r = uc[0];
        } else if (mOrder & 0x40) {
            a = uc[0];
            r = uc[1];
            g = uc[0x20];
            b = uc[0x21];
        } else {
            a = uc[3];
            r = uc[2];
            g = uc[1];
            b = uc[0];
        }

        if (mOrder & 2) {
            a = ((a * 256) - a) >> 7;
        }
    } else if (mBpp == 0x10) {
        unsigned short swapped = SwapBytes(*(unsigned short *)uc);
        if (mOrder & 1) {
            a = -(swapped >> 0xF & 1);
            b = swapped >> 7 & 0xF8;
            g = swapped >> 2 & 0xF8;
            r = swapped << 3;
        } else {
            a = -(swapped >> 0xF & 1);
            r = swapped >> 7 & 0xF8;
            g = swapped >> 2 & 0xF8;
            b = swapped << 3;
        }
    } else if ((mOrder & 0x80) && (mOrder & 0x40)) {
        r = 255;
        g = 255;
        b = 255;
        a = *uc;
    } else {
        a = 255;
        if (mOrder & 1) {
            b = uc[2];
            g = uc[1];
            r = uc[0];
        } else {
            r = uc[2];
            g = uc[1];
            b = uc[0];
        }
    }
}

void RndBitmap::ConvertColor(
    unsigned char r, unsigned char g, unsigned char b, unsigned char a, unsigned char *uc
) const {
    if (mBpp == 0x20 || mPalette) {
        if (mOrder & 2) {
            a = ((unsigned int)a + 1) >> 1;
        }
        if (mOrder & 1) {
            uc[3] = a;
            uc[2] = b;
            uc[1] = g;
            uc[0] = r;
        } else if (mOrder & 0x40) {
            uc[0] = a;
            uc[1] = r;
            uc[0x20] = g;
            uc[0x21] = b;
        } else {
            uc[3] = a;
            uc[2] = r;
            uc[1] = g;
            uc[0] = b;
        }
        return;
    }
    if (mBpp == 0x10) {
        unsigned short *twobytes = (unsigned short *)uc;
        if (mOrder & 1) {
            *twobytes = (a & 0x80) << 8 | (b & 0xF8) << 7 | (g & 0xF8) << 2 | r >> 3;
        } else {
            *twobytes = (a & 0x80) << 8 | (r & 0xF8) << 7 | (g & 0xF8) << 2 | b >> 3;
        }
        *twobytes = EndianSwap(*twobytes);
        return;
    }
    if (mOrder & 1) {
        uc[2] = b;
        uc[1] = g;
        uc[0] = r;
    } else {
        uc[2] = r;
        uc[1] = g;
        uc[0] = b;
    }
}

void RndBitmap::PaletteColor(
    int i, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a
) const {
    ConvertColor(mPalette + PaletteOffset(i) * 4, r, g, b, a);
}

int RndBitmap::PaletteOffset(int i) const {
    if ((mOrder & 2) && mBpp == 8) {
        if ((i & 0x18) == 8) {
            i += 8;
        } else if ((i & 0x18) == 0x10) {
            i -= 8;
        }
    }
    return i;
}

unsigned char RndBitmap::RowNonTransparent(int x, int y, int z, int *iptr) {
    for (int i = x; i < y; i++) {
        unsigned char r, g, b, a;
        PixelColor(i, z, r, g, b, a);
        if (a != 0) {
            *iptr = i;
            return a;
        }
    }
    return 0;
}

unsigned char RndBitmap::ColumnNonTransparent(int x, int y, int z, int *iptr) {
    for (int i = y; i < z; i++) {
        unsigned char r, g, b, a;
        PixelColor(x, i, r, g, b, a);
        if (a != 0) {
            *iptr = i;
            return a;
        }
    }
    return 0;
}

bool RndBitmap::IsTranslucent() const {
    if (mBpp == 24)
        return false;
    for (int i = 0; i < mHeight; i++) {
        for (int j = 0; j < mWidth; j++) {
            unsigned char r, g, b, a;
            PixelColor(j, i, r, g, b, a);
            if (a < 253)
                return true;
        }
    }
    return false;
}

void RndBitmap::GenerateMips() {
    int dim = Min(mWidth, mHeight);
    if (dim > 16U) {
        RELEASE(mMip);
        mMip = new RndBitmap();
        mMip->Create(mWidth >> 1, mHeight >> 1, 0, mBpp, mOrder, mPalette, 0, 0);
        for (int i = 0; i < mMip->mHeight; i++) {
            for (int j = 0; j < mMip->mWidth; j++) {
                unsigned char r, g, b, a;
                PixelColor(j * 2, i * 2, r, g, b, a);
                int rsum = r;
                int gsum = g;
                int bsum = b;
                int asum = a;
                PixelColor(j * 2 + 1, i * 2, r, g, b, a);
                rsum += r;
                gsum += g;
                bsum += b;
                asum += a;
                PixelColor(j * 2, i * 2 + 1, r, g, b, a);
                rsum += r;
                gsum += g;
                bsum += b;
                asum += a;
                PixelColor(j * 2 + 1, i * 2 + 1, r, g, b, a);
                rsum += r;
                gsum += g;
                bsum += b;
                asum += a;
                int rval = rsum >> 2;
                int gval = gsum >> 2;
                int bval = bsum >> 2;
                int aval = asum >> 2;
                mMip->SetPixelColor(j, i, (unsigned char)rval, (unsigned char)gval, (unsigned char)bval, (unsigned char)aval);
            }
        }
        mMip->GenerateMips();
    }
}

void RndBitmap::SelfMip() {
    int rowOffset = mRowBytes / 2;
    int pixelBytes = PixelBytes();
    mWidth /= 2;
    unsigned short w = Width();
    unsigned short h = Height();
    int dim = Min(w, h);

    int i4 = 0;
    int i3 = 0;
    while (dim > 1) {
        if (dim & 1) {
            i3 = 1;
        }
        dim >>= 1;
        i4++;
    }
    int count = i4 + i3 - 3;
    RndBitmap *cur = this;
    RELEASE(mMip);
    for (int i = 0; i < count; i++) {
        cur->mMip = new RndBitmap();
        cur->mMip->Create(
            cur->mWidth >> 1,
            cur->mHeight >> 1,
            mRowBytes,
            mBpp,
            mOrder,
            mPalette,
            mPixels + rowOffset,
            0
        );
        pixelBytes >>= 1;
        cur = cur->mMip;
        rowOffset += pixelBytes;
    }
}

bool ModifierFound(const char *filename, const char *key) {
    return strstr(filename, MakeString("%s.", key))
        || strstr(filename, MakeString("%s_", key));
}

bool RndBitmap::ProcessFlags(const char *filename, bool wantMips) {
    if (ModifierFound(filename, "_tb")) {
        SetAlpha(kTransparentBlack);
    } else if (ModifierFound(filename, "_gw")) {
        SetAlpha(kGrayscaleWhite);
    } else if (ModifierFound(filename, "_ga")) {
        SetAlpha(kGrayscaleAlpha);
    }

    if (ModifierFound(filename, "_pma")) {
        SetPreMultipliedAlpha();
    }
    if (ModifierFound(filename, "_selfmip")) {
        SelfMip();
    } else if (wantMips) {
        if (!ModifierFound(filename, "_nomip")) {
            GenerateMips();
        }
    } else if (ModifierFound(filename, "_mip")) {
        GenerateMips();
    }
    return true;
}

bool RndBitmap::SamePixelFormat(const RndBitmap &bm) const {
    if (mBpp != bm.Bpp() || mOrder != bm.Order())
        return false;
    if (mPalette && bm.Palette()) {
        return SamePaletteColors(bm);
    } else
        return true;
}

void RndBitmap::Blt(
    const RndBitmap &bm, int dX, int dY, int sX, int sY, int width, int height
) {
    MILO_ASSERT((int)(int)mWidth >= dX + width, 1728);
    MILO_ASSERT(dY + height <= mHeight, 1729);
    MILO_ASSERT(sX + width <= bm.Width(), 1730);
    MILO_ASSERT(sY + height <= bm.Height(), 1731);
    if (SamePixelFormat(bm)) {
        if (mOrder & 0x38) {
            MILO_ASSERT(!((dX | dY | sX | sY | width | height) & 0x3), 0x6CC);
        }
        int count = width * mBpp >> 3;
        for (; height > 0; height--, dY++, sY++) {
            void *dst = mPixels + (dY * mRowBytes) + (dX * mBpp >> 3);
            void *src = bm.mPixels + (sY * bm.RowBytes()) + (sX * bm.Bpp() >> 3);
            memcpy(dst, src, count);
        }
    } else {
        if (mOrder & 0x38) {
            MILO_NOTIFY(
                "RndBitmap::Blt: Can't blt to DXT formatted textures, changing to rgba."
            );
            Create(mWidth, mHeight, 0, 0x20, 0, 0, 0, 0);
        }
        if (mPalette && bm.Palette()) {
            unsigned char colorBuffer[256];
            int i = bm.NumPaletteColors() - 1;
            unsigned char *idx = colorBuffer + i;
            for (; i >= 0; i--, idx--) {
                unsigned char r, g, b, a;
                bm.PaletteColor(i, r, g, b, a);
                *idx = NearestColor(r, g, b, a);
            }
            for (int h = height, dy = dY, sy = sY; h >= 1; h--, dy++, sy++) {
                for (int w = width, sx = sX, dx = dX; w > 0; w--, sx++, dx++) {
                    SetPixelIndex(dx, dy, colorBuffer[bm.PixelIndex(sx, sy)]);
                }
            }
        } else {
            for (int h = height, dy = dY, sy = sY; h >= 1; h--, dy++, sy++) {
                for (int w = width, sx = sX, dx = dX; w > 0; w--, sx++, dx++) {
                    unsigned char r, g, b, a;
                    bm.PixelColor(sx, sy, r, g, b, a);
                    SetPixelColor(dx, dy, r, g, b, a);
                }
            }
        }
    }
}

void RndBitmap::SaveBmpPixels(BinStream *file) const {
    for (int i = mHeight - 1; i >= 0; i--) {
        u8 *pixels = mPixels + mRowBytes * i;
        if (mBpp == 4) {
            u8 *pixelIt = pixels;
            for (; pixelIt != pixels + mRowBytes; pixelIt++) {
                unsigned char pix = ((*pixelIt & 0xF0) >> 4) | ((*pixelIt & 0x0F) << 4);
                *file << pix;
            }
        } else {
            file->Write(pixels, mRowBytes);
        }
    }
}

void RndBitmap::SetMip(RndBitmap *bm) {
    RndBitmap *mip = mMip;
    delete mip;
    mMip = 0;
    if (bm) {
        MILO_ASSERT(mWidth / 2 == bm->Width(), 0x435);
        MILO_ASSERT(mHeight / 2 == bm->Height(), 0x436);
        MILO_ASSERT(mOrder == bm->Order(), 0x437);
        MILO_ASSERT(mBpp == bm->Bpp(), 0x438);
        mMip = bm;
    }
}

void RndBitmap::SaveBmpHeader(BinStream *file) const {
    tagBITMAPFILEHEADER fileheader;
    tagBITMAPINFOHEADER infoheader;

    MILO_ASSERT(file, 0x524);
    unsigned short us = 0x4D42; // "BM" in ASCII, used to identify that this is a bmp file
    *file << us;
    fileheader.bfOffBits = PaletteBytes() + 54;
    fileheader.bfSize = fileheader.bfOffBits + PixelBytes();
    fileheader.bfReserved1 = 0;
    fileheader.bfReserved2 = 0;
    *file << fileheader;

    infoheader.biSize = 40;
    infoheader.biWidth = mWidth;
    infoheader.biHeight = mHeight;
    infoheader.biPlanes = 1;
    infoheader.biBitCount = mBpp;
    infoheader.biCompression = 0;
    infoheader.biSizeImage = 0;
    infoheader.biXPelsPerMeter = 0xb11;
    infoheader.biYPelsPerMeter = 0xb11;
    infoheader.biClrUsed = 0;
    infoheader.biClrImportant = 0;
    *file << infoheader;
    if (mPalette) {
        file->Write(mPalette, (1 << mBpp) << 2);
    }
}

void RndBitmap::SetPaletteColor(
    int idx, unsigned char r, unsigned char g, unsigned char b, unsigned char a
) {
    ConvertColor(r, g, b, a, mPalette + PaletteOffset(idx) * 4);
}

void RndBitmap::Save(BinStream &bs) const {
    SaveHeader(bs);
    if (mPalette) {
        bs.Write(mPalette, PaletteBytes());
    }
    const RndBitmap *m = this;
    while (m) {
        WriteChunks(bs, m->Pixels(), m->PixelBytes(), 0x8000);
        m = m->mMip;
    }
}

// DXT color decompression: extracts RGBA from DXT-compressed 4x4 block
//
// DXT1 compression stores 4x4 pixel blocks (16 pixels) using:
//   - 2 reference colors in RGB565 format (16-bit each)
//   - 16 2-bit indices (4 bytes) selecting interpolated colors
//   Total: 8 bytes per 16 pixels (4 bits/pixel compression)
//
// Color interpolation modes:
//   - If color0 > color1 (4-color opaque mode):
//       idx 0 = color0, idx 1 = color1, idx 2 = 2/3 blend, idx 3 = 1/3 blend
//   - If color0 <= color1 (3-color + transparent mode, DXT1 only):
//       idx 0 = color0, idx 1 = color1, idx 2 = average, idx 3 = transparent black
//
// Parameters:
//   blockData: pointer to 8-byte DXT block (4 bytes colors + 4 bytes indices)
//   pixelX, pixelY: coordinates within 4x4 block (0-3)
//   hasDxt1Alpha: enable DXT1 1-bit alpha mode (color0 <= color1 → idx 3 = transparent)
//   r, g, b, a: output color components (0-255)
void DecodeDxtColor(unsigned char *blockData, int pixelX, int pixelY, bool hasDxt1Alpha, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a) {
    unsigned char *rowPtr;
    unsigned short color0 = *(unsigned short *)blockData;
    unsigned short color1 = *((unsigned short *)blockData + 1);

    if (pixelY & 1) {
        rowPtr = blockData + pixelY - 1;
    } else {
        rowPtr = blockData + pixelY + 1;
    }

    unsigned char r0 = (color0 >> 8) & 0xF8;
    unsigned char r1 = (color1 >> 8) & 0xF8;
    unsigned char g0 = (color0 >> 3) & 0xFC;
    unsigned char g1 = (color1 >> 3) & 0xFC;
    int colorIdx = (rowPtr[4] >> ((pixelX << 1) & 0xFE)) & 3;
    unsigned char b0 = (color0 << 3) & 0xF8;
    unsigned char b1 = (color1 << 3) & 0xF8;

    a = 0xFF;

    if (colorIdx == 0) {
        r = r0;
        g = g0;
        b = b0;
        return;
    }

    if (colorIdx == 1) {
        r = r1;
        g = g1;
        b = b1;
        return;
    }

    if ((color0 > color1) || !hasDxt1Alpha) {
        int w0 = 4 - colorIdx;
        int w1 = colorIdx - 1;
        r = (unsigned int) ((r1 * w1) + (r0 * w0)) / 3U;
        g = (unsigned int) ((g1 * w1) + (g0 * w0)) / 3U;
        b = (unsigned int) ((b1 * w1) + (b0 * w0)) / 3U;
    } else {
        r = ((int)r0 + (int)r1) / 2;
        g = ((int)g0 + (int)g1) / 2;
        b = ((int)b0 + (int)b1) / 2;
        if (colorIdx == 3) {
            a = 0;
        }
    }
}

void DecodeDxt3Alpha(unsigned char *uc, int i, int j, unsigned char &alpha) {
    unsigned short *bytepair = (unsigned short *)uc;
    int i1 = bytepair[j] >> (i << 2);
    alpha = ((i1 << 4) & 0xF0) | (i1 & 0xF);
}

void DecodeDxt5Alpha(unsigned char *uc, int i, int j, unsigned char &alpha) {
    // Stack-allocated lookup tables matching target's exact initialization order
    unsigned char local_60[32];

    // Initialize in the exact order shown in Ghidra
    local_60[0] = 0;
    local_60[1] = 0;
    local_60[2] = 0;
    local_60[3] = 1;
    local_60[4] = 1;
    local_60[5] = 1;
    local_60[6] = 2;
    local_60[7] = 2;
    local_60[8] = 3;
    local_60[9] = 3;
    local_60[10] = 3;
    local_60[11] = 4;
    local_60[12] = 4;
    local_60[13] = 4;
    local_60[14] = 5;
    local_60[15] = 5;

    local_60[16] = 0;
    local_60[17] = 3;
    local_60[18] = 6;
    local_60[19] = 1;
    local_60[20] = 4;
    local_60[21] = 7;
    local_60[22] = 2;
    local_60[23] = 5;
    local_60[24] = 0;
    local_60[25] = 3;
    local_60[26] = 6;
    local_60[27] = 1;
    local_60[28] = 4;
    local_60[29] = 7;
    local_60[30] = 2;
    local_60[31] = 5;

    // Read alpha values - order matches target: alpha1 first, alpha0 second
    unsigned char alpha1 = uc[1];
    unsigned char alpha0 = uc[0];

    int iVar3 = j << 2;
    unsigned char byteOff = local_60[iVar3 + i];
    unsigned char bitPos = local_60[i + iVar3 + 16];

    // Calculate adjusted offset
    unsigned int adjustedOff;
    if ((byteOff & 1) == 0) {
        adjustedOff = byteOff + 1;
    } else {
        adjustedOff = byteOff + 0xFF;
    }

    unsigned int index;
    if (bitPos < 6) {
        index = (uc[adjustedOff + 2] >> bitPos) & 7;
    } else {
        unsigned int off2 = byteOff + 1;
        unsigned int adjustedOff2;
        if ((off2 & 1) == 0) {
            adjustedOff2 = off2 + 2;
        } else {
            adjustedOff2 = off2 + 1;
        }
        if (bitPos == 6) {
            index = ((uc[adjustedOff2 + 2] & 1) << 2) | (uc[adjustedOff + 2] >> 6);
        } else {
            index = ((uc[adjustedOff2 + 2] & 3) << 1) | (uc[adjustedOff + 2] >> 7);
        }
    }

    if (index == 0) {
        alpha = alpha1;
    } else if (index == 1) {
        alpha = alpha0;
    } else {
        if (alpha0 < alpha1) {
            if (index == 6) {
                alpha = 0;
            } else if (index == 7) {
                alpha = 0xFF;
            } else {
                alpha = ((6 - index) * (unsigned int)alpha1 + (index - 1) * (unsigned int)alpha0 + 2) / 5;
            }
        } else {
            alpha = ((8 - index) * (unsigned int)alpha1 + (index - 1) * (unsigned int)alpha0 + 3) / 7;
        }
    }
}

void RndBitmap::DxtColor(
    int x, int y, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a
) const {
    int dxt = mOrder & 0x38;
    MILO_ASSERT(dxt != 0, 0x6CC);

    int yQuotient = y / 4;
    int yRemainder = y - yQuotient * 4;
    int xQuotient = x / 4;
    int blockIdx = (mWidth >> 2) * yQuotient + xQuotient;
    int xRemainder = x - xQuotient * 4;

    if (dxt == 8) {
        DecodeDxtColor(mPixels + blockIdx * 8, xRemainder, yRemainder, true, r, g, b, a);
    } else {
        u8 *blockData = mPixels + blockIdx * 0x10;
        unsigned char unused;
        DecodeDxtColor(blockData + 8, xRemainder, yRemainder, false, r, g, b, unused);
        if (dxt == 0x10) {
            unsigned short *alphaData = (unsigned short *)blockData;
            unsigned char alphaBits = (unsigned char)(alphaData[yRemainder] >> (xRemainder << 2));
            a = alphaBits | (alphaBits << 4);
        } else {
            DecodeDxt5Alpha(blockData, xRemainder, yRemainder, a);
        }
    }
}

void RndBitmap::PixelColor(
    int x, int y, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a
) const {
    if (mPalette) {
        PaletteColor(PixelIndex(x, y), r, g, b, a);
    } else if (mOrder & 0x38) {
        DxtColor(x, y, r, g, b, a);
    } else {
        bool boolbool;
        const unsigned char *p = mPixels + PixelOffset(x, y, boolbool);
        ConvertColor(p, r, g, b, a);
    }
}

void RndBitmap::SetPixelColor(
    int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a
) {
    if (mPalette) {
        SetPixelIndex(x, y, NearestColor(r, g, b, a));
    } else {
        bool boolbool;
        unsigned char *p = mPixels + PixelOffset(x, y, boolbool);
        ConvertColor(r, g, b, a, p);
    }
}

void RndBitmap::ConvertToAlpha() {
    if (mBpp == 24) {
        RndBitmap bmap;
        bmap.Create(*this, 32, mOrder, 0);
        if (mBuffer) {
            MemFree(mBuffer, __FILE__, 0x328);
            mBuffer = nullptr;
        }
        mPalette = bmap.mPalette;
        mPixels = bmap.mPixels;
        mBuffer = bmap.mBuffer;
        mBpp = bmap.mBpp;
        mRowBytes = bmap.mRowBytes;
        bmap.mBuffer = nullptr;
    }
}

int RndBitmap::NumPaletteColors() const {
    if (mPalette)
        return 1 << mBpp;
    else
        return 0;
}

void RndBitmap::SetAlpha(AlphaFlag flag) {
    ConvertToAlpha();
    if (mBpp <= 8) {
        int max = 255;
        int min = 0;
        for (int i = NumPaletteColors() - 1; i >= 0; i--) {
            unsigned char r, g, b, a;
            PaletteColor(i, r, g, b, a);
            switch (flag) {
            case kTransparentBlack:
                if (!(r | g | b)) {
                    a = min;
                }
                break;
            case kGrayscaleWhite:
                a = r;
                b = max;
                g = max;
                r = max;
                break;
            case kGrayscaleAlpha:
                a = r;
                break;
            default:
                break;
            }
            SetPaletteColor(i, r, g, b, a);
        }
    } else {
        for (int i = 0; i < mHeight; i++) {
            for (int j = 0; j < mWidth; j++) {
                unsigned char r, g, b, a;
                PixelColor(j, i, r, g, b, a);
                switch (flag) {
                case kTransparentBlack:
                    if (!(r | g | b)) {
                        a = 0;
                    }
                    break;
                case kGrayscaleWhite:
                    a = r;
                    b = 255;
                    g = 255;
                    r = 255;
                    break;
                case kGrayscaleAlpha:
                    a = r;
                    break;
                default:
                    break;
                }
                SetPixelColor(j, i, r, g, b, a);
            }
        }
    }
}

bool RndBitmap::SamePaletteColors(const RndBitmap &bmap) const {
    u8 *bmapPalette = bmap.mPalette;
    u8 *thisPalette = mPalette;
    u32 bmapPaletteWord = *(u32 *)(bmapPalette + 0x14);
    u32 thisPaletteWord = *(u32 *)(thisPalette + 0x14);

    if (bmapPaletteWord == thisPaletteWord) {
        return true;
    }

    u8 bppVal = bmap.mBpp;
    u32 paletteIndexMask = (1 << bppVal) - 1;

    if ((s32)paletteIndexMask < 0) {
        return true;
    }

    u32 paletteIndexMaskMinus8 = paletteIndexMask - 8;
    u32 thisBitmapFlags = mOrder & 2;
    u32 bmapBitmapFlags = bmap.mOrder & 2;

    u32 paletteIdx = paletteIndexMask;

    while (true) {
        u32 thisAdjustedIdx = paletteIdx;
        if (thisBitmapFlags != 0 && bppVal == 8) {
            u32 bits = paletteIdx & 0x18;
            if (bits == 8) {
                thisAdjustedIdx = paletteIndexMaskMinus8 + 0x10;
            } else if (bits == 0x10) {
                thisAdjustedIdx = paletteIndexMaskMinus8;
            }
        }

        u8 thisR, thisG, thisB, thisA;
        ConvertColor(thisPalette + thisAdjustedIdx * 4, thisR, thisG, thisB, thisA);

        u32 bmapAdjustedIdx = paletteIdx;
        if (bmapBitmapFlags != 0 && bppVal == 8) {
            u32 bits = paletteIdx & 0x18;
            if (bits == 8) {
                bmapAdjustedIdx = paletteIndexMaskMinus8 + 0x10;
            } else if (bits == 0x10) {
                bmapAdjustedIdx = paletteIndexMaskMinus8;
            }
        }

        u8 bmapR, bmapG, bmapB, bmapA;
        bmap.ConvertColor(bmapPalette + bmapAdjustedIdx * 4, bmapR, bmapG, bmapB, bmapA);

        if (thisR != bmapR) {
            return false;
        }

        paletteIdx -= 1;
        paletteIndexMaskMinus8 -= 1;

        if ((s32)paletteIdx < 0) {
            break;
        }
    }

    return true;
}

bool RndBitmap::LoadBmp(const char *filename, bool wantMips, bool noAlpha) {
    FileStream *stream = new FileStream(filename, FileStream::kRead, true);
    if (stream->Fail()) {
        delete stream;
        return false;
    } else {
        if (!LoadBmp(stream)) {
            delete stream;
            return false;
        } else {
            delete stream;
            if (!noAlpha) {
                ProcessFlags(filename, wantMips);
            }
            return true;
        }
    }
}
