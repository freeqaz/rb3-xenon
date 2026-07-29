#include "ui/UIProxy.h"
#include "math/Mtx.h"
#include "obj/Data.h"
#include "obj/DataFunc.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/File.h"
#include "rndobj/Dir.h"
#include "rndobj/Env.h"
#include "rndobj/Trans.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"
#include "utl/Messages.h"
#include "utl/Symbols.h"

UIProxy::UIProxy()
    : mDir(), mEnv(this, 0), mMainTrans(0), mSyncOnMove(0), mPolled(0) {
    mOldXfm.Reset();
    mOldXfm.v.x = -1.0e+30f;
}

void UIProxy::Init() { REGISTER_OBJ_FACTORY(UIProxy); }

void UIProxy::SetTypeDef(DataArray *da) {
    if (TypeDef() != da) {
        UIComponent::SetTypeDef(da);
        mDir = 0;
        mSyncOnMove = false;
        if (da) {
            da->FindData("sync_on_move", mSyncOnMove, false);
            DataArray *fileArr = da->FindArray("file", false);
            if (fileArr->Size() != 3 || fileArr->Int(2) != 0) {
                bool shared = true;
                da->FindData("share", shared, false);
                FilePath fp(FileGetPath(da->File()), fileArr->Str(1));
                mDir.LoadFile(fp, Loading(), shared, kLoadFront, false);
                mPolled = false;
                if (!Loading())
                    UpdateDir();
            }
        }
    }
}

BEGIN_COPYS(UIProxy)
    COPY_SUPERCLASS(UIComponent)
    CREATE_COPY(UIProxy)
    BEGIN_COPYING_MEMBERS
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mEnv)
        }
    END_COPYING_MEMBERS
END_COPYS

BEGIN_SAVES(UIProxy)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(UIComponent)
    bs << mEnv;
END_SAVES

BEGIN_LOADS(UIProxy)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

INIT_REVS(3, 0)

void UIProxy::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(3, 0)
    d.PushRev(this);
    UIComponent::PreLoad(bs);
}

void UIProxy::PostLoad(BinStream &bs) {
    mDir.PostLoad(nullptr);
    UIComponent::PostLoad(bs);
    BinStreamRev d(bs, bs.PopRev(this));
    if (d.rev == 1) {
        bool b;
        bs >> b;
    }
    if (d.rev > 2)
        bs >> mEnv;
    UpdateDir();
}

void UIProxy::Poll() {
    UIComponent::Poll();
    if (!Loading() && !mDir.Ptr() && mDir.IsLoaded()) {
        mDir.PostLoad(nullptr);
        UpdateDir();
    }
    if (mDir.Ptr() && Showing()) {
        SyncDir();
        mDir->Poll();
        mPolled = true;
    }
}

void UIProxy::DrawShowing() {
    if (mDir.Ptr() && mPolled) {
        RndEnviron *oldEnv = mDir->GetEnv();
        mDir->SetEnv(mEnv);
        mDir->DrawShowing();
        mDir->SetEnv(oldEnv);
    }
}

RndDrawable *UIProxy::CollideShowing(const Segment &seg, float &f, Plane &pl) {
    if (!mDir.Ptr())
        return nullptr;
    else {
        SyncDir();
        return mDir->CollideShowing(seg, f, pl) ? this : nullptr;
    }
}

int UIProxy::CollidePlane(const Plane &pl) {
    if (!mDir.Ptr())
        return -1;
    else {
        SyncDir();
        return mDir->CollidePlane(pl);
    }
}

void UIProxy::SetProxyDir(const FilePath &fp, bool b) {
    mMainTrans = 0;
    mDir.LoadFile(fp, true, b, kLoadFront, false);
    mPolled = 0;
}

void UIProxy::SetProxyDir(RndDir *dir) {
    mDir = dir;
    mPolled = 0;
    UpdateDir();
}

void UIProxy::SyncDir() {
    const Transform &world = WorldXfm();
    if (mSyncOnMove) {
        if (world == mOldXfm)
            return;
        mOldXfm = world;
    }
    if (mMainTrans)
        mMainTrans->SetWorldXfm(world);
    else {
        if (mDir->TransParent())
            mDir->SetWorldXfm(world);
        else
            mDir->SetLocalXfm(world);
    }
    static Message sync_dir_msg("sync_dir");
    HandleType(sync_dir_msg);
}

void UIProxy::UpdateDir() {
    DataArray *transArr = TypeDef()->FindArray("main_trans", false);
    if (transArr) {
        if (mDir.Ptr()) {
            mMainTrans = mDir->Find<RndTransformable>(transArr->Str(1), true);
        } else {
            MILO_WARN("%s Couldn't load main_trans", PathName(this));
            mMainTrans = 0;
        }
    } else
        mMainTrans = 0;
    UpdateSphere();
    if (mDir.Ptr()) {
        mDir->Enter();
        mPolled = false;
        mOldXfm.v.x = -1.0e+30f;
    }
}

DataNode UIProxy::OnSetProxyDir(DataArray *da) {
    if (da->Size() == 2) {
        SetProxyDir(FilePath(da->Str(2)), da->Int(3));
    } else
        SetProxyDir(da->Obj<RndDir>(2));
    return DataNode(1);
}

BEGIN_HANDLERS(UIProxy)
    HANDLE_EXPR(proxy_dir, ProxyDir())
    HANDLE(set_proxy_dir, OnSetProxyDir)
    HANDLE_SUPERCLASS(UIComponent)
END_HANDLERS

BEGIN_PROPSYNCS(UIProxy)
    SYNC_PROP(environ, mEnv)
    SYNC_SUPERCLASS(UIComponent)
END_PROPSYNCS
