#pragma once
#include "obj/Object.h"
#include "rndobj/Draw.h"
#include "rndobj/PostProc.h"
#include "rndobj/ShaderOptions.h"
#include "rndobj/Tex.h"

enum BloomBlurStyle {
    kBloomBlurNormal = 0,
    kBloomBlurStreak = 1,
    kBloomBlurGlare = 2,
};

enum BloomBlurDirection {
    kBloomBlurHorizontal = 0,
    kBloomBlurVertical = 1,
};

void Bloom_Downsample(ShaderType shader, RndTex *texSrc, RndTex *texDst);
void Bloom_Blur(RndTex *texSrc, RndTex *texDst, BloomBlurStyle style, BloomBlurDirection direction, unsigned int pass, float attenuation, float angle);

class NgPostProc : public RndPostProc {
public:
    class BloomTextureSet {
    public:
        BloomTextureSet();
        virtual ~BloomTextureSet();

        void AllocateTextures(unsigned int, unsigned int);
        void FreeTextures();
        RndTex *Tex(int i) { return mBloomTexture[i]; }

    private:
        RndTex *mBloomTexture[2]; // 0x4

        friend class NgPostProc;
    };

    template <int N>
    class BloomTextures {
    public:
        BloomTextures() {}
        virtual ~BloomTextures() {
            for (int i = 0; i < N; i++) {
                mTextures[i].FreeTextures();
            }
        }

        // Each set is a quarter of the previous one's size, starting a
        // quarter of (w, h).
        void AllocateTextures(unsigned int w, unsigned int h) {
            BloomTextureSet *set = mTextures;
            for (int i = N; i != 0; i--) {
                w >>= 2;
                h >>= 2;
                set->AllocateTextures(w, h);
                set++;
            }
        }

    private:
        BloomTextureSet mTextures[N];

        friend class NgPostProc;
    };

    NgPostProc();
    virtual ~NgPostProc();
    OBJ_CLASSNAME(PostProc);
    OBJ_SET_TYPE(PostProc);
    virtual void Select();
    virtual void QueueMotionBlurObject(class RndDrawable *);
    virtual void SetBloomColor();
    virtual void EndWorld();
    virtual void DoPost();

protected:
    virtual void OnSelect();
    virtual void OnUnselect();

public:

    static void Init();
    NEW_OBJ(NgPostProc);
    static void RebuildTex();
    static void Terminate();

protected:
    void DoVelocity();
    void DoBloom();
    void CheckNoise();
    void CheckBlendPrevious();
    void CheckVignette();
    void CheckMotionBlur();
    void CheckChromaticAberration();
    void CheckHallOfTime();
    void CheckHueConverge();
    void CheckGradientMap();
    void CheckPosterizeAndKaleidoscope();
    void CheckRefract();
    void ModulateColorXfm();

    static Hmx::Color s_prevBloomColor;
    static float s_prevBloomIntensity;
    static NgPostProc *s_BloomSetter;
    static BloomTextures<3> sBloom;

    static void ReleaseTex();

    Vector2 mRandomSeed; // 0x20c
    Vector2 mRefractOffset; // 0x214
    ObjPtrList<RndDrawable> mMotionBlurDrawList; // 0x21c
#ifdef HX_NATIVE
    bool mMotionBlurEnabled;
#endif
    // RB3's NgPostProc is 0x230 bytes (NewObject allocates 0x230).
};
