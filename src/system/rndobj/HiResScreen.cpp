#include "rndobj/HiResScreen.h"
#include "os/Debug.h"
#include "os/File.h"
#include "rndobj/Rnd.h"
#include "rndobj/Bitmap.h"
#include "rndobj/Tex.h"
#include "utl/MemMgr.h"
#include "utl/MakeString.h"
#include "utl/FileStream.h"

HiResScreen gHiResScreen;
HiResScreen &TheHiResScreen = gHiResScreen;

void HiResScreen::BmpCache::DeleteCache() {
    for (unsigned int i = 0; i < mTotalNumCacheLines; i++) {
        FileDelete(mFileNames[i].c_str());
    }
}

int HiResScreen::GetPaddingX() const { return 480; }
int HiResScreen::GetPaddingY() const { return 270; }

HiResScreen::BmpCache::BmpCache(unsigned int ui1, unsigned int ui2) {
    mPixelsPerRow = ui1;
    mTotalRows = ui2;
    mDirtyStart = 0;
    mDirtyEnd = 0;

    unsigned int rows_per = ui2 + 1;
    unsigned int byte_size;
    do {
        rows_per--;
        byte_size = rows_per * ui1 * 4;
    } while (byte_size > 0x6DDD00);

    mRowsPerCacheLine = rows_per;
    auto& _ref0 = mByteSize;
    _ref0 = byte_size;
    MILO_ASSERT(ui2 % rows_per == 0, 0x3B);
    mTotalNumCacheLines = ui2 / rows_per;
    mFileNames = new String[mTotalNumCacheLines];
    for (uint i = 0; i < mTotalNumCacheLines; i++) {
        mFileNames[i] = MakeString("_hires_cache_%.2d.dat", i);
    }
    mBuffer = (unsigned char *)MemAlloc(_ref0, __FILE__, 0x44, "HiResScreenCache");
    mCurrLoadedIndex = ui2;
    DeleteCache();
}

HiResScreen::BmpCache::~BmpCache() {
    DeleteCache();
    delete[] mFileNames;
    mFileNames = 0;
    delete mBuffer;
    mBuffer = 0;
}

void HiResScreen::BmpCache::GetLoadedRange(unsigned int &ui1, unsigned int &ui2) const {
    ui1 = mCurrLoadedIndex * mRowsPerCacheLine;
    ui2 = ui1 + mRowsPerCacheLine - 1;
}

void HiResScreen::BmpCache::FlushCache() {
    MILO_ASSERT(mCurrLoadedIndex < mTotalNumCacheLines, 0x9C);
    if (mDirtyEnd > mDirtyStart) {
        File *cacheFile = NewFile(mFileNames[mCurrLoadedIndex].c_str(), 1);
        MILO_ASSERT(cacheFile, 0xA2);
        cacheFile->Seek(mDirtyStart, 0);
        unsigned int nStart = mDirtyStart;
        unsigned int nEnd = mDirtyEnd;
        unsigned int nBuffRange = nEnd - nStart;
        MILO_ASSERT(nBuffRange <= mByteSize, 0xAA);
        unsigned int numWritten = cacheFile->Write(mBuffer + nStart, nBuffRange);
        MILO_ASSERT(numWritten == nBuffRange, 0xAE);
        cacheFile->Flush();
        delete cacheFile;
        mDirtyStart = 0;
        mDirtyEnd = 0;
    }
}

void HiResScreen::BmpCache::LoadCache(unsigned int y) {
    unsigned int nLoadedStart = mCurrLoadedIndex * mRowsPerCacheLine;
    unsigned int nLoadedEnd = nLoadedStart + mRowsPerCacheLine - 1;
    if (y >= nLoadedStart && y <= nLoadedEnd) {
        return;
    }
    if (mCurrLoadedIndex < mTotalNumCacheLines) {
        FlushCache();
    }
    unsigned int newIndex = y / mRowsPerCacheLine;
    File *cacheFile = NewFile(mFileNames[newIndex].c_str(), 2);
    if (cacheFile == 0) {
        memset(mBuffer, 0, mByteSize);
        cacheFile = NewFile(mFileNames[newIndex].c_str(), 0x101);
        MILO_ASSERT(cacheFile, 0x80);
        mDirtyStart = 0;
        mDirtyEnd = mByteSize;
    } else {
        unsigned int numRead = cacheFile->Read(mBuffer, mByteSize);
        MILO_ASSERT(numRead == mByteSize, 0x8A);
        mDirtyStart = 0;
        mDirtyEnd = 0;
    }
    if (cacheFile != 0) {
        delete cacheFile;
    }
    mCurrLoadedIndex = newIndex;
}

void HiResScreen::BmpCache::GetPixelColor(
    int x, int y, unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a
) const {
    MILO_ASSERT(x >= 0 && x < mPixelsPerRow, 0xBC);
    unsigned int nLoadedStart = mCurrLoadedIndex * mRowsPerCacheLine;
    unsigned int nLoadedEnd = nLoadedStart + mRowsPerCacheLine - 1;
    MILO_ASSERT(y >= nLoadedStart && y <= nLoadedEnd, 0xC1);
    unsigned int yOffset = nLoadedEnd - y;
    unsigned int offset = (yOffset * mPixelsPerRow + x) * 4;
    unsigned char *ptr = mBuffer + offset;
    a = ptr[3];
    r = ptr[2];
    g = ptr[1];
    b = ptr[0];
}

void HiResScreen::BmpCache::SetPixelColor(
    int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a
) {
    MILO_ASSERT(x >= 0 && x < mPixelsPerRow, 0xD0);
    unsigned int nLoadedStart = mCurrLoadedIndex * mRowsPerCacheLine;
    unsigned int nLoadedEnd = nLoadedStart + mRowsPerCacheLine - 1;
    MILO_ASSERT(y >= nLoadedStart && y <= nLoadedEnd, 0xD5);
    unsigned int yOffset = nLoadedEnd - y;
    unsigned int offset = (yOffset * mPixelsPerRow + x) * 4;
    unsigned int newPixel = (a << 24) | (r << 16) | (g << 8) | b;
    unsigned char *bufPtr = mBuffer + offset;
    unsigned int oldPixel = *(unsigned int *)bufPtr;
    if (newPixel != oldPixel) {
        *(unsigned int *)bufPtr = newPixel;
        unsigned int minDirty = mDirtyStart;
        if (minDirty > offset) {
            minDirty = offset;
        }
        mDirtyStart = minDirty;
        unsigned int maxDirty = offset + 4;
        unsigned int curEnd = mDirtyEnd;
        if (maxDirty >= curEnd) {
            curEnd = maxDirty;
        }
        mDirtyEnd = curEnd;
    }
}

void HiResScreen::TakeShot(const char *c, int i) {
    mFileBase = c;
    mTiling = i;
    mActive = 1;
    mCurrTile = 0;
    if (TheRnd.Width() <= 480 || TheRnd.Height() <= 270) {
        MILO_NOTIFY("Padding exceeds screen size");
        mActive = false;
    } else {
        mAccumWidth = i * (TheRnd.Width() - 480);
        mAccumHeight = i * TheRnd.Height() - i * 270;
        if ((int)mAccumWidth < TheRnd.Width() || (int)mAccumHeight < TheRnd.Height()) {
            MILO_NOTIFY(MakeString("HiResScreenshot requires more tiles (%d specified)", i));
            mActive = false;
        } else {
            mCache = new BmpCache(mAccumWidth, mAccumHeight);
            mEvenOddDisabled = TheRnd.GetEvenOddDisabled();
            mShrinkToSafe = TheRnd.ShrinkToSafeArea();
            mConsoleShowing = TheRnd.ConsoleShowing();
            TheRnd.SetEvenOddDisabled(true);
            TheRnd.SetShrinkToSafeArea(false);
            TheRnd.ShowConsole(false);
        }
    }
}

void HiResScreen::GetBorderForTile(int x, int y, int &left, int &right, int &top, int &bottom)
    const {
    left = 0;
    top = 0;
    right = 0;
    bottom = 0;
    int xStep = TheRnd.Width() - 480;
    int xPos = xStep * x + TheRnd.Width();
    if ((unsigned int)(unsigned int)xPos < (int)mAccumWidth) {
        right = 480;
    } else if (((x + 1) * xStep) - TheRnd.Width() > 0) {
        left = 480;
    }
    int yStep = TheRnd.Height() - 270;
    int yPos = yStep * y + TheRnd.Height();
    if (yPos < (unsigned int)(int)mAccumHeight) {
        bottom = 270;
    } else if (((y + 1) * yStep) - TheRnd.Height() > 0) {
        top = 270;
    }
}

void HiResScreen::Accumulate() {
    if (mCurrTile == 0) {
        mCurrTile = 1;
        return;
    }
    int prevTile = mCurrTile - 1;
    if (prevTile >= mTiling * mTiling) {
        return;
    }
    RndTex *tex = Hmx::Object::New<RndTex>();
    RndBitmap bm;
    tex->SetBitmap(0, 0, 0, RndTex::kFrontBuffer, false, 0);
    tex->LockBitmap(bm, true);
    delete tex;
    int tileX = prevTile % mTiling;
    int tileY = prevTile / mTiling;
    int left, right, top, bottom;
    GetBorderForTile(tileX, tileY, left, right, top, bottom);
    int xOff = (TheRnd.Width() - 480) * tileX;
    int yOff = (TheRnd.Height() - 270) * tileY;
    Merge(bm, xOff, yOff, left, right, bm.Width(), bm.Height(), top, bottom);
    TheRnd.ResetProcCounter();
    mCurrTile++;
}

void HiResScreen::Finish() {
    int fileNum = 0;
    String filename;
    File *existFile = 0;
    FileStream *fs = 0;
    do {
        fileNum++;
        filename = MakeString("%s_%d.bmp", mFileBase, fileNum);
        if (existFile) {
            delete existFile;
        }
        existFile = NewFile(filename.c_str(), 1);
    } while (existFile);
    mCache->FlushCache();
    fs = new FileStream(filename.c_str(), FileStream::kWrite, true);
    void *tmpBuf = MemAlloc(0x40, "HiResScreen.cpp", 0x250, "TmpRndBitmap", 0);
    RndBitmap bm;
    bm.Create(mAccumWidth, mAccumHeight, 0, 0x20, 0, 0, tmpBuf, 0);
    bm.SaveBmpHeader(fs);
    ::operator delete(tmpBuf);
    for (int i = mCache->mTotalNumCacheLines - 1; i >= 0; i--) {
        mCache->LoadCache(i * mCache->mRowsPerCacheLine);
        fs->Write(mCache->mBuffer, mCache->mByteSize);
    }
    if (fs) {
        delete fs;
    }
    FileMkDir("lo_res");
    filename = MakeString("lo_res/%s_%d.bmp", mFileBase, fileNum);
    File *loResFile = NewFile(filename.c_str(), 0x101);
    if (loResFile != 0) {
        delete loResFile;
        RndBitmap loResBm;
        DownSample(loResBm);
        loResBm.SaveBmp(filename.c_str());
    }
    mActive = false;
    TheRnd.SetEvenOddDisabled(mEvenOddDisabled);
    TheRnd.SetShrinkToSafeArea(mShrinkToSafe);
    TheRnd.ShowConsole(mConsoleShowing);
    delete mCache;
}

void HiResScreen::Merge(
    const RndBitmap &bm, int srcX, int srcY, int srcW, int srcH, int dstX, int dstY, int padX, int padY
) {
    int blendThreshX = dstX - padX;
    int blendThreshY = dstY - padY;
    if (srcH >= dstY) {
        return;
    }
    int xConst = srcH - srcY;
    int xBlend = srcH - blendThreshY;
    int yIter = srcY;
    do {
        if ((unsigned int)yIter >= mAccumHeight) break;
        if (yIter >= 0) {
            mCache->LoadCache(yIter);
            if (srcW < dstX) {
                int xIter = srcX;
                int yBlend = srcW - blendThreshX;
                int yConst = srcW - srcX;
                do {
                    if ((unsigned int)xIter >= mAccumWidth) break;
                    if (xIter >= 0) {
                        int bmY = xConst + yIter;
                        int bmX = yConst + xIter;
                        unsigned char r, g, b, a;
                        bm.PixelColor(bmX, bmY, r, g, b, a);
                        unsigned char cr, cg, cb, ca;
                        mCache->GetPixelColor(xIter, yIter, cr, cg, cb, ca);
                        float blendX = 0.0f;
                        float blendY = 0.0f;
                        if (bmX > blendThreshX) {
                            blendX = (float)yBlend / (float)padX;
                        }
                        if (bmY > blendThreshY) {
                            blendY = (float)xBlend / (float)padY;
                        }
                        float blend;
                        if (blendX > 0.0f || blendY > 0.0f) {
                            blend = sqrtf(blendX * blendX + blendY * blendY);
                            blend = blend - 0.5f;
                            blend = blend + blend;
                            blend = Max(blend, 0.0f);
                            blend = Min(blend, 1.0f);
                        } else {
                            blend = 0.0f;
                        }
                        float invBlend = (1.0f - blend) * 255.0f;
                        a = (unsigned char)invBlend;
                        if (ca != 0) {
                            float t = ca / 255.0f;
                            int dr = cr - r;
                            int dg = cg - g;
                            int db = cb - b;
                            r += (unsigned char)(dr * t + 0.5f);
                            g += (unsigned char)(dg * t + 0.5f);
                            b += (unsigned char)(db * t + 0.5f);
                            if (a < ca) {
                                a = ca;
                            }
                        }
                        mCache->SetPixelColor(xIter, yIter, r, g, b, a);
                    }
                    xIter++;
                    yBlend++;
                } while (yConst + xIter < dstX);
            }
        }
        yIter++;
        xBlend++;
    } while (xConst + yIter < dstY);
}

void HiResScreen::DownSample(RndBitmap &outBm) {
    int tiling = mTiling;
    int accum_h = mAccumHeight;
    int accum_w = mAccumWidth;
    int newWidth = (tiling * 480 + accum_w) / tiling;
    int newHeight = (tiling * 270 + accum_h) / tiling;
    float scaleX = (float)accum_w / (float)newWidth;
    float scaleY = (float)accum_h / (float)newHeight;
    outBm.Create(newWidth, newHeight, 0, 0x20, 0, 0, 0, 0);
    memset(outBm.Pixels(), 0, outBm.PixelBytes());
    for (int y = 0; y < newHeight; y++) {
        int srcY = (int)(y * scaleY);
        mCache->LoadCache(srcY);
        for (int x = 0; x < newWidth; x++) {
            int srcX = (int)(x * scaleX);
            unsigned char r, g, b, a;
            mCache->GetPixelColor(srcX, srcY, r, g, b, a);
            outBm.SetPixelColor(x, y, r, g, b, a);
        }
    }
}

void HiResScreen::CurrentTileRect(
    const Hmx::Rect &inRect, Hmx::Rect &outTileRect, Hmx::Rect &outAccumRect
) const {
    int tiling = mTiling;
    int tile = mCurrTile;
    int tileX = tile % tiling;
    int tileY = tile / tiling;
    float tileXf = (float)tileX;
    float invTiling = 1.0f / (float)tiling;
    float tileYf = (float)tileY;
    float tileXStart = tileXf * invTiling;
    float tileYStart = tileYf * invTiling;
    float tileXWidth = (tileXf + 1.0f) * invTiling - tileXStart;
    float tileYHeight = (tileYf + 1.0f) * invTiling - tileYStart;
    float x0 = (inRect.x - tileXStart) / tileXWidth;
    float x1 = ((inRect.w + inRect.x) - tileXStart) / tileXWidth;
    float y0 = (inRect.y - tileYStart) / tileYHeight;
    float y1 = ((inRect.h + inRect.y) - tileYStart) / tileYHeight;
    x0 = Clamp(0.0f, 1.0f, x0);
    x1 = Clamp(0.0f, 1.0f, x1);
    y0 = Clamp(0.0f, 1.0f, y0);
    y1 = Clamp(0.0f, 1.0f, y1);
    outTileRect.x = x0;
    outTileRect.w = x1;
    outTileRect.y = y0;
    outTileRect.h = y1;
    outAccumRect.x = x0 * invTiling + tileXStart;
    outAccumRect.y = y0 * invTiling + tileYStart;
    outAccumRect.w = (x1 * invTiling + tileXStart) - outAccumRect.x;
    outAccumRect.h = (y1 * invTiling + tileYStart) - outAccumRect.y;
}

Hmx::Rect HiResScreen::ScreenRect(const RndCam *cam, const Hmx::Rect &r) const {
    Hmx::Rect ret = r;
    if ((cam->TargetTex() != 0 && !mOverride) || !mActive || mCurrTile >= mTiling * mTiling) {
        return r;
    }
    int tiling = mTiling;
    int tileX = mCurrTile % tiling;
    int tileY = mCurrTile / tiling;
    float invTiling = 1.0 / (float)tiling;
    Hmx::Rect tileRect, accumRect;
    CurrentTileRect(r, tileRect, accumRect);
    int left, right, top, bottom;
    GetBorderForTile(tileX, tileY, left, right, top, bottom);
    float screenH = (float)TheRnd.Height();
    float screenW = (float)TheRnd.Width();
    float leftF = (float)left;
    float rightF = (float)right;
    float topF = (float)top;
    float bottomF = (float)bottom;
    float xScale = screenH / (screenH - leftF);
    float yScale = screenW / (screenW - topF);
    float xShift = screenH / (screenH - rightF);
    float yShift = screenW / (screenW - bottomF);
    float xOffset = (xScale - invTiling) - invTiling;
    xShift = (xShift - invTiling) - invTiling;
    float yOffset = yScale - invTiling;
    yShift = yShift - invTiling;
    ret.x = accumRect.x - xOffset;
    ret.w = (accumRect.w + (xOffset + xShift));
    ret.y = accumRect.y - yOffset;
    ret.h = accumRect.h + yOffset + yShift;
    return ret;
}

Hmx::Rect HiResScreen::ScreenRect() const {
    const RndCam *cam = RndCam::Current();
    Hmx::Rect r = cam->GetScreenRect();
    return ScreenRect(cam, r);
}

Hmx::Rect HiResScreen::InvScreenRect() const {
    Hmx::Rect r = ScreenRect();
    Hmx::Rect ret;
    float invH = 1.0f / r.h;
    float negInvH = -1.0f / r.h;
    ret.h = invH;
    float invW = 1.0f / r.w;
    ret.w = invW;
    ret.y = negInvH * r.y;
    ret.x = -(invW * r.x);
    return ret;
}
