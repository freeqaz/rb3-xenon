#pragma once
#include "math/Vec.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Anim.h"
#include "rndobj/Cam.h"
#include "rndobj/Draw.h"
#include "rndobj/Env.h"
#include "rndobj/Poll.h"
#include "rndobj/Tex.h"
#include "utl/MemMgr.h"

/** "TexRender renders a draw and cam into a texture." */
class RndTexRenderer : public RndDrawable, public RndAnimatable, public RndPollable {
public:
    // Hmx::Object
    virtual ~RndTexRenderer() {}
    OBJ_CLASSNAME(TexRenderer);
    OBJ_SET_TYPE_ENGINE(TexRenderer);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    // RndDrawable: `this` at 0x0
    virtual void DrawShowing();
    virtual void ListDrawChildren(std::list<RndDrawable *> &);
    virtual void DrawPreClear() { DrawToTexture(); }
    virtual void UpdatePreClearState();
    // RndAnimatable: `this` at 0x34
    virtual void SetFrame(float frame, float blend);
    virtual float StartFrame();
    virtual float EndFrame();
    virtual void ListAnimChildren(std::list<RndAnimatable *> &) const;
    // RndPollable: `this` at 0x50
    virtual void ListPollChildren(std::list<RndPollable *> &) const;

    OBJ_MEM_OVERLOAD(0x1A)
    NEW_OBJ(RndTexRenderer)
    static void Init() { REGISTER_OBJ_FACTORY(RndTexRenderer) }

    void DrawToTexture();
    virtual void DrawBefore() {}
    virtual void DrawAfter() {}
    void SetOutputTexture(RndTex *tex) { mOutputTexture = tex; }
    RndTex* GetOutputTexture() const { return mOutputTexture; }
    void SetDraw(RndDrawable *draw) {
        mDrawable = draw;
        mDirty = true;
    }
    void SetForce(bool force) {
        mForce = force;
        mDirty = true;
    }

protected:
    RndTexRenderer();
    void InitTexture();

    DataNode OnGetRenderTextures(DataArray *);

    // Retail RB3 is RndTexRenderer revision 11: the layout below is reconstructed
    // from the retail ctor/Save (fn_82430F68 inits 4 ObjPtrs @0x48/0x54/0x60/0x70
    // with bools @0x6c/0x6d/0x6e between the 3rd and 4th; Save fn_82431948 writes
    // `li 0xb`). DC3 is newer (rev 13: it adds mEnviron @rev12 and
    // mClearBuffer/mClearColor @rev13, and reorders the bools after mEnviron) —
    // those three members do NOT exist in retail RB3, so they are removed here and
    // the bools restored to their rev-11 position right after mCamera.
    bool mDirty; // 0x3c
    /** "Force rendering every frame" */
    bool mForce; // 0x3d
    /** "Renders the texture before the rest of the scene is rendered.
        Useful for rendering large textures" */
    bool mDrawPreClear; // 0x3e
    /** "Renders the texture only on 'world' frames,
       while skipping rendering on post processing frames" */
    bool mDrawWorldOnly; // 0x3f
    /** "If true, exclusively draws the draw,
        if false the scene will draw it too, use with caution!" */
    bool mDrawResponsible; // 0x40
    /** "If [draw] will not get enter, exit, or poll automatically,
        it will be up to script hooks to do any of that" */
    bool mNoPoll; // 0x41
    /** "Height for imposter rendering with current camera" */
    float mImpostorHeight; // 0x44
    /** "Texture to write to" */
    ObjPtr<RndTex> mOutputTexture; // 0x48
    /** "Draw Object to render to texture" */
    ObjPtr<RndDrawable> mDrawable; // 0x54
    /** "Camera to use, if you want specific one,
        defaults to proxy cam, if none and draw is proxy" */
    ObjPtr<RndCam> mCamera; // 0x60
    /** "Check this if rendering multiple characters to a texture.
        Will draw 2x if checked." */
    bool mPrimeDraw; // 0x6c
    bool mFirstDraw; // 0x6d
    /** "Generate mip maps for the texture." */
    bool mForceMips; // 0x6e
    /** "We will mirror this cam about whatever mesh is associated
        with our output texture to automatically position
        the render-2-tex cam for mirroring" */
    ObjPtr<RndCam> mMirrorCam; // 0x70
};
