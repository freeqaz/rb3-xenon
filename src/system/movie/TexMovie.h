#pragma once
#include "movie/Movie.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/PropSync.h"
#include "rndobj/Draw.h"
#include "rndobj/Poll.h"
#include "rndobj/Tex.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"

class TexMovie : public RndDrawable, public RndPollable {
public:
    // Hmx::Object
    virtual ~TexMovie();
    virtual void Copy(Hmx::Object const *, Hmx::Object::CopyType);
    virtual void Replace(ObjRef *, Hmx::Object *);
    OBJ_CLASSNAME(TexMovie);
    OBJ_SET_TYPE_ENGINE(TexMovie);
    // TexMovie::NewObject (0x82742E20, emitted in the Movie TU) evaluates
    // StaticClassName() and then calls MemAlloc(0x84, 0) inline: the
    // OBJ_MEM_OVERLOAD shape. ??_GTexMovie (0x82747708) calls
    // ?MemFree@@YAXPAX@Z directly, so the delete side is the inline one too.
    OBJ_MEM_OVERLOAD_INLINE_DEL(0x18);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Load(BinStream &);

    // RndDrawable
    virtual void DrawPreClear();
    virtual void UpdatePreClearState();

    // RndPollable
    virtual void Poll();
    virtual void Enter();
    virtual void Exit();

    void SetPaused(bool);
    void Reset();
    bool IsEmpty() const;
    void DrawToTexture();
    void SetFile(FilePath const &);
    NEW_OBJ(TexMovie);
    static void Init() { REGISTER_OBJ_FACTORY(TexMovie); }

    void SetVolume(float vol) { mMovie.SetVolume(vol); }
    // RB3 retail Movie has no embedded FaderGroup (dc3-engine-only addition);
    // see movie/Movie.h. Fader management is not routed through Movie here.
    void AddFader(Fader *f) {}
    bool IsOpen() const { return mMovie.IsOpen(); }
    Movie &GetMovie() { return mMovie; }

protected:
    ObjOwnerPtr<RndTex> mTex; // 0x2c ObjOwnerPtr | 0x54, RndTex
    bool mLoop;
    bool mEntered;
    bool mIsLocalized;
    bool mPaused;
    FilePath sRoot;
    Movie mMovie; // 0x48

    TexMovie();
    // RB3 retail's TexMovie::DoBeginMovieFromFile takes no LoaderPos (the
    // LoaderPos parameter is a newer dc3-engine addition, matching
    // Movie::BeginFromFile above -- both call sites (OnPlayMovie, SetFile)
    // show target loading only the BinStream* arg into r4, never a second
    // register for LoaderPos; confirmed against RB3 retail asm 2026-07-31).
    void DoBeginMovieFromFile(BinStream *);
    DataNode OnPlayMovie(DataArray *);
    DataNode OnGetRenderTextures(DataArray *);
};
