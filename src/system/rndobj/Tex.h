#pragma once
#include "obj/Object.h"
#include "rndobj/Bitmap.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"

/**
 * @brief A texture.
 * Original _objects description:
 * "Tex objects represent bitmaps used by materials. These
 * can be created automatically with 'import tex' on the file menu."
 */
class RndTex : public Hmx::Object {
public:
    enum Type {
        kRegular = 1,
        kRendered = 2,
        kMovie = 4,
        kBackBuffer = 8,
        kFrontBuffer = 0x18,
        kRenderedNoZ = 0x22,
        kShadowMap = 0x42,
        kDepthVolumeMap = 0xA2,
        kDensityMap = 0x122,
        kScratch = 0x200,
        kDeviceTexture = 0x1000,
        kRegularLinear = 0x2000
    };
    enum AlphaCompress {
    };

    virtual ~RndTex();
    OBJ_CLASSNAME(Tex)
    OBJ_SET_TYPE(Tex)
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, CopyType);
    virtual void Load(BinStream &);
    virtual void Print();
    virtual void PreLoad(BinStream &);
    virtual void PostLoad(BinStream &);
    virtual void SetMipMapK(float f) { mMipMapK = f; }
    virtual void LockBitmap(RndBitmap &, int);
    virtual void UnlockBitmap() {}
#ifdef HX_NATIVE
    virtual void MakeDrawTarget();
    virtual void FinishDrawTarget();
#else
    virtual void MakeDrawTarget() {}
    virtual void FinishDrawTarget() {}
#endif
    virtual void Compress(AlphaCompress) {}
    virtual bool TexelsLock(void *&) { return false; }
    virtual void TexelsUnlock() {}
    virtual unsigned int TexelsPitch() const { return 0; }
    virtual void Select(int) {}

    OBJ_MEM_OVERLOAD(0x1C)
    NEW_OBJ(RndTex)
    static void Init() { REGISTER_OBJ_FACTORY(RndTex) }

    /** Set this texture's bitmap using the supplied parameters.
     * @param [in] w The texture's width.
     * @param [in] h The texture's height.
     * @param [in] bpp The texture's bpp.
     * @param [in] ty The texture's type.
     * @param [in] useMips If true, generate a mipmap with this texture's bitmap.
     * @param [in] path The path to the texture.
     */
    void SetBitmap(int w, int h, int bpp, Type ty, bool useMips, const char *path);
    /** Set this texture's bitmap using the supplied parameters.
     * @param [in] bmap The bitmap to assign to this texture.
     * @param [in] path The path to the texture.
     * @param [in] b TODO: currently unknown.
     */
    void SetBitmap(const RndBitmap &bmap, const char *path, bool b);
    void SetBitmap(FileLoader *);
    void SetBitmap(const FilePath &);
    void SetBitmap(const RndBitmap &, const char *, bool, Type);
    /** Unused. Presumably saves the bitmap of this texture to a supplied filename. */
    void SaveBitmap(const char *);
    /** Determine whether this texture's dimensions are both powers of 2. */
    void SetPowerOf2();

    /** Validate the texture based on the supplied properties.
     * @param [in] width The texture's width.
     * @param [in] height The texture's height.
     * @param [in] bpp The texture's bpp.
     * @param [in] numMips The number of mips this texture has.
     * @param [in] ty The texture's type.
     * @param [in] file Param name is from RB2 DWARF, unknown what this is for.
     * @returns An error message if there were issues found.
     */
    static const char *
    CheckSize(int width, int height, int bpp, int numMips, Type ty, bool file);
    /** Get the appropriate texture bpp and order for this platform.
     * @param [in] path The path to the texture.
     * @param [out] bpp The bpp a texture on this platform should have.
     * @param [out] order The order a texture on this platform should have.
     * @param [in] hasAlpha If true, factor alpha into the resulting order.
     */
    static void PlatformBppOrder(const char *path, int &bpp, int &order, bool hasAlpha);

    int SizeKb() const { return ((mWidth * mHeight * mBpp) / 8 / 1024); }
    bool IsBackBuffer() const { return mType & kBackBuffer; }
    bool IsRenderTarget() const { return mType & kRendered; }
    int Width() const { return mWidth; }
    int Height() const { return mHeight; }
    Type GetType() const { return mType; }
    const FilePath &File() const { return mFilepath; }
    int NumMips() const { return mNumMips; }
    int Bpp() const { return mBpp; }
#ifdef HX_NATIVE
    const RndBitmap& Bitmap() const { return mBitmap; }
#endif
    bool PowerOf2();

protected:
    RndTex();

public:
#ifdef HX_NATIVE
    virtual void PresyncBitmap();
    virtual void SyncBitmap();
#else
    virtual void PresyncBitmap() {}
    virtual void SyncBitmap() {}
#endif
protected:

    /** Handler to set this texture's bitmap.
     * @param [in] arr The supplied DataArray.
     * Expected DataArray contents:
     *     Node 2: A string containing the path to the texture.
     * Example usage: {$this set_bitmap texture.tex}
     * OR
     * Expected DataArray contents:
     *     Node 2: The texture width.
     *     Node 3: The texture height.
     *     Node 4: The texture bpp.
     *     Node 5: The texture type.
     *     Node 6: Whether or not to set a mipmap.
     * Example usage: {$this set_bitmap 512 512 24 kRendered TRUE}
     */
    DataNode OnSetBitmap(const DataArray *arr);

    /** Handler to set this texture's type to rendered.
     * Example usage: {$this set_rendered}
     */
    DataNode OnSetRendered(const DataArray *);
    DataNode OnSetSize(int, int);

    // NOTE: DC3 (newer) has `Hmx::CRC unk2c; // 0x2c` here that retail RB3-360
    // lacks. rb3-Wii agrees (mBitmap follows the prior block directly, no CRC),
    // and RndTex::Print/Save/~RndTex read mBitmap+every following member at +4 vs
    // retail. Gated out (default) to match the retail layout; the only use is the
    // COPY_MEMBER in Tex.cpp, gated to match. Native keeps the DC3 member.
#ifdef RB3_RNDTEX_DC3_CRC
    Hmx::CRC unk2c; // 0x2c
#endif
    /** The bitmap associated with this texture. */
    RndBitmap mBitmap; // 0x2c
    float mMipMapK; // 0x50
    /** The texture's type. */
    Type mType; // 0x54
    /** The texture's width, in pixels. */
    int mWidth; // 0x58
    /** The texture's height, in pixels. */
    int mHeight; // 0x5c
    /** The texture's bits per pixel. */
    int mBpp; // 0x60
    /** The texture's file. */
    FilePath mFilepath; // 0x64
    /** The number of mips in this texture's mipmap. */
    int mNumMips; // 0x6c
    /** Unused. Presumably, whether to use specialized computations for the PS3. */
    bool mOptimizeForPS3; // 0x68
    /** Whether or not this texture's width and height are powers of 2. */
    // RB3-360 retail HAS this member; DC3 (a newer engine revision) dropped it and
    // this header inherited DC3's commented-out form, which is why RndTex::RndTex
    // was one instruction short: retail zeroes TWO bools, `stb r29,0x68` then
    // `stb r29,0x69`.
    // ⚠ ORDER IS THE REVERSE OF rb3-Wii, which has mIsPowerOf2 (0x5C) BEFORE
    // mOptimizeForPS3 (0x5D). Declaring it the Wii way moved mOptimizeForPS3 to
    // 0x69 and measured -1 matched / -404 matched_code over 625 recompiled TUs.
    // Retail adjudicates directly: ?Save@RndTex@@ (100%) reads mOptimizeForPS3
    // with `lbz r11, 0x68(r30)`, so mOptimizeForPS3 is at 0x68 and mIsPowerOf2
    // takes 0x69. Declared last => no existing member offset moves.
    bool mIsPowerOf2; // 0x69
    FileLoader *mLoader; // 0x74
};

TextStream &operator<<(TextStream &, RndTex::Type);
