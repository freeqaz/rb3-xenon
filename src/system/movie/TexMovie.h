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
    OBJ_SET_TYPE(TexMovie);
    // laneAT-f4 opt-out: the retail bytes show TexMovie's operator new was kept
    // OUT OF LINE and ICF-folded (its `new` site is a single
    // `bl ??2<folded>@@SAPAXI@Z` with NO StaticClassName call), unlike the
    // OBJ_MEM_OVERLOAD majority which retail inlined. Classified from the
    // CTOR relocation, not the symbol name -- see
    // /home/free/tmp/laneAT/f4/newobj_classify.py.
    MEM_OVERLOAD(TexMovie, 0x18);
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
    ObjOwnerPtr<RndTex> mTex; // 0x48 ObjOwnerPtr | 0x54, RndTex
    bool mLoop;
    bool mEntered;
    bool mIsLocalized;
    bool mPaused;
    FilePath sRoot;
    Movie mMovie; // 0x68

    TexMovie();
    void DoBeginMovieFromFile(BinStream *, LoaderPos);
    DataNode OnPlayMovie(DataArray *);
    DataNode OnGetRenderTextures(DataArray *);
};
