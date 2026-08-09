#include "ui/UIComponent.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "obj/Utl.h"
#include "os/File.h"
#include "os/System.h"
#include "rndobj/Draw.h"
#include "rndobj/Mesh.h"
#include "rndobj/Poll.h"
#include "rndobj/Trans.h"
#include "ui/UI.h"
#include "ui/UIResource.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/MakeString.h"

int UIComponent::sSelectFrames = 0;
bool gResettingType;

Symbol UIComponentStateToSym(UIComponent::State s) {
    static Symbol syms[5] = { "normal", "focused", "disabled", "selecting", "selected" };
    return syms[s];
}

Symbol UIComponent::StateSym() const {
    return UIComponentStateToSym((UIComponent::State)mState);
}

void UIComponent::Enter() {
    RndPollable::Enter();
    mSelected = 0;
    if (mState == kSelecting) {
        SetState(kFocused);
    }
}

void UIComponent::Exit() { RndPollable::Exit(); }

UIComponent::UIComponent()
    : mState(kNormal), mNavRight(this), mNavDown(this), mSelectingUser(nullptr),
      mSelectScreen(nullptr), mSelected(0), mResource(nullptr),
      mResourceName(), mResourceDir(), mResourcePath(), mLoading(0),
      mSelectCancelled(0) {}

UIComponent::~UIComponent() {
    if (mResource)
        mResource->Release();
}

BEGIN_PROPSYNCS(UIComponent)
    SYNC_PROP(nav_right, mNavRight)
    SYNC_PROP(nav_down, mNavDown)
    SYNC_PROP_MODIFY(resource_name, mResourceName, ResourceFileUpdated(false))
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndTransformable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain does not include this superclass;
    // DC3's newer engine added it. Native-only.
    SYNC_SUPERCLASS(RndPollable)
#endif
END_PROPSYNCS

BEGIN_COPYS(UIComponent)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndTransformable)
    COPY_SUPERCLASS(RndDrawable)
    CREATE_COPY_AS(UIComponent, c)
    BEGIN_COPYING_MEMBERS_FROM(c)
        COPY_MEMBER(mNavRight)
        COPY_MEMBER(mNavDown)
    END_COPYING_MEMBERS
END_COPYS

void UIComponent::CopyMembers(const UIComponent *c, Hmx::Object::CopyType ty) {
    RndTransformable::Copy(c, ty);
    RndDrawable::Copy(c, ty);
    mNavRight = c->mNavRight;
    mNavDown = c->mNavDown;
    mResourceName = c->mResourceName;
    mResourceDir = c->mResourceDir;
    mResourcePath = c->mResourcePath;
}

// Ported from rb3-Wii (../rb3/src/system/ui/UIComponent.cpp) per Phase B of
// docs/decomp/research/2026-06-11-uicomponent-virtuals.md — retail-360 has
// this as a real UIComponent override (Object-vbase vtable slot 15,
// fn_827DAB68), not a fallthrough to Hmx::Object::SetTypeDef.
void UIComponent::SetTypeDef(DataArray *da) {
    if (!da && mResourcePath.length() == 0) {
        DataArray *cfg = SystemConfig("objects", ClassName());
        DataArray *found = cfg->FindArray("init", false);
        if (found) {
            DataArray *typesArr = cfg->FindArray("types");
            DataArray *defaultArr = typesArr->FindArray("default", false);
            if (defaultArr) {
                MILO_WARN(
                    "Resetting %s (%s) to default type (%s)",
                    ClassName(),
                    Name(),
                    PathName(Dir())
                );
                SetTypeDef(defaultArr);
                return;
            } else {
                MILO_FAIL(
                    "No default type for %s, please add to %s (%s)",
                    ClassName(),
                    typesArr->File(),
                    PathName(Dir())
                );
                return;
            }
        }
    }
    Hmx::Object::SetTypeDef(da);
    UpdateResource();
}

void UIComponent::ResourceCopy(const UIComponent *c) {
    MILO_ASSERT(c, 0x94);
    Hmx::Object::SetTypeDef((DataArray *)c->TypeDef());
    CopyMembers(c, kCopyShallow);
    if (mResourcePath.length() != 0) {
        mResourceDir = c->mResourceDir;
        MILO_ASSERT(mResourceDir.Ptr(), 0x9B);
    } else {
        mResource = c->mResource;
        mResource->PostLoad();
        MILO_ASSERT(mResource->Dir(), 0xA1);
    }
    Update();
}

// matches on retail: https://decomp.me/scratch/3ya1L  (fn_827DB8C8)
void UIComponent::Update() {
    if (mResourcePath.length() != 0) {
        if (!mResourceDir) {
            FileStat stat;
            const char *default_str = "default";
            const char *milo_str =
                MakeString("%s/%s.milo", mResourcePath.c_str(), default_str);
            if (!default_str) {
                MILO_FAIL(
                    "No default_resource for %s, please add 'default_resource' block ",
                    ClassName()
                );
                return;
            }
            int filestat = FileGetStat(milo_str, &stat);
            if (filestat == -1) {
                MILO_FAIL(
                    "%s %s (%s) is missing default resource file %s, please fix",
                    ClassName(),
                    Name(),
                    PathName(this),
                    milo_str
                );
            } else {
                MILO_ASSERT(!mLoading, 0x161);
                MILO_WARN(
                    "Resetting %s (%s) resource to default because resource %s couldn't be found (%s)",
                    ClassName(),
                    Name(),
                    mResourceName.c_str(),
                    PathName(Dir())
                );
                mResourceName = default_str;
                ResourceFileUpdated(false);
                UIComponent::Update();
            }
        }
    } else {
        if (mResource) {
            RndDir *rdir = mResource->Dir();
            if (rdir) {
                mMeshes.clear();
                static Symbol meshes("meshes");
                DataArray *mesharr = TypeDef()->FindArray(meshes, false);
                if (mesharr) {
                    for (int i = 1; i < mesharr->Size(); i++) {
                        DataArray *innerarr = mesharr->Array(i);
                        RndMesh *newmesh = rdir->Find<RndMesh>(innerarr->Str(0), true);
                        UIMesh uimesh;
                        uimesh.mMesh = newmesh;
                        for (int i = 0; i < kNumStates; i++)
                            uimesh.mMats[i] = 0;
                        for (int j = 1; j < innerarr->Size(); j++) {
                            DataArray *anotherarr = innerarr->Array(j);
                            State state = SymToUIComponentState(anotherarr->Sym(0));
                            uimesh.mMats[state] =
                                rdir->Find<RndMat>(anotherarr->Str(1), true);
                        }
                        mMeshes.push_back(uimesh);
                    }
                }
            } else {
                ObjectDir *curdir = Dir();
                const DataArray *def = TypeDef();
                MILO_WARN(
                    "Can't find %s (%s) resource file %s for type %s! (%s)",
                    ClassName(),
                    Name(),
                    def->FindStr("resource_file"),
                    Type(),
                    PathName(curdir)
                );
                DataArray *cfg = SystemConfig("objects", ClassName(), "types");
                DataArray *defaultarr = cfg->FindArray("default", false);
                if (!defaultarr) {
                    MILO_FAIL(
                        "No default type for %s, please add to %s",
                        ClassName(),
                        cfg->File()
                    );
                } else if (defaultarr == def) {
                    MILO_FAIL(
                        "%s default type has invalid resource file, please fix %s",
                        ClassName(),
                        cfg->File()
                    );
                } else {
                    MILO_ASSERT(!mLoading, 0x1A7);
                    MILO_WARN(
                        "Resetting %s (%s) type to default (%s)",
                        ClassName(),
                        Name(),
                        PathName(Dir())
                    );
                    gResettingType = true;
                    SetTypeDef(defaultarr);
                    gResettingType = false;
                    UIComponent::Update();
                }
            }
        }
    }
}

void UIComponent::UpdateMeshes(State s) {
    for (std::vector<UIMesh>::iterator it = mMeshes.begin(); it != mMeshes.end(); ++it) {
        if (it->mMesh->Mat() != it->mMats[s]) {
            it->mMesh->SetMat(it->mMats[s]);
        }
    }
}

void UIComponent::UpdateResource() {
    if (mResource)
        mResource->Release();
    mResource = TheUI->Resource(this);
    if (mResource) {
        mResource->Load(mLoading);
    }
    if (!mLoading && !gResettingType)
        Update();
}

void UIComponent::ResourceFileUpdated(bool b) {
    if (!mResourceName.empty()) {
        mResourcePath = GetResourcesPath();
        const char *pathstr =
            MakeString("%s/%s.milo", mResourcePath.c_str(), mResourceName);
        mResourceDir.LoadFile(FilePath(FileRoot(), pathstr), b, true, kLoadFront, false);
        if (!b)
            mResourceDir.PostLoad(0);
    } else
        mResourceDir = 0;
    if (!b)
        Update();
}

const char *UIComponent::GetResourcesPath() {
    std::vector<Symbol> syms;
    syms.push_back(ClassName());
    ListSuperClasses(ClassName(), syms);
    static Symbol objects("objects");
    static Symbol resources_path("resources_path");
    DataArray *arr = 0;
    for (int i = 0; i < syms.size(); i++) {
        arr = SystemConfig(objects, syms[i])->FindArray(resources_path, false);
        if (arr)
            break;
    }
    if (!arr)
        return 0;
    else {
        const char *str = arr->Str(1);
        if (*str == '\0')
            return 0;
        else
            return FileMakePath(FileGetPath(arr->File()), str);
    }
}

ObjectDir *UIComponent::ResourceDir() {
    if (mResourceDir)
        return mResourceDir;
    else if (mResource)
        return mResource->Dir();
    else
        return 0;
}

DataNode UIComponent::OnGetResourcesPath(DataArray *da) {
    if (mResourcePath.length() != 0) {
        return DataNode(FileRelativePath(FileRoot(), mResourcePath.c_str()));
    } else
        return DataNode("");
}

void UIComponent::MockSelect() {
    MILO_ASSERT(sSelectFrames < 255, 0x13F);
    MILO_ASSERT(sSelectFrames >= 0, 0x140);
    mSelected = sSelectFrames;
    SetState(UIComponent::kSelecting);
    mSelectCancelled = true;
}

BEGIN_SAVES(UIComponent)
    // RB3 retail is rev 2 and saves mResourceName as a third field. DC3 (newer)
    // is rev 3 and dropped it. Adjudicated on retail bytes: 'li r11,0x2' for the
    // rev word, and a trailing 'subi r4,r31,0x2c; bl operator<<(BinStream&,
    // const String&)'. r31 == this+0x144 here, so -0x60/-0x54/-0x2c are
    // mNavRight (0xe4) / mNavDown (0xf0) / mResourceName (0x118).
    // rb3-Wii is NOT an oracle for this body -- its dev build uses
    // SAVE_OBJ(UIComponent, 182), the "can't save" stub.
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndTransformable)
    SAVE_SUPERCLASS(RndDrawable)
    // Chained: retail threads operator<<'s returned BinStream& from the first
    // call into the second (r3 is not reloaded between them, and &mNavDown is
    // precomputed into r29), then reloads r3 from the bs copy for the String.
    bs << mNavRight << mNavDown;
    bs << mResourceName;
END_SAVES

BEGIN_LOADS(UIComponent)
    PreLoad(bs);
    PostLoad(bs);
END_LOADS

UIComponent::State SymToUIComponentState(Symbol s) {
    for (int i = 0; i < 5; i++) {
        if (s.Str() == UIComponentStateToSym((UIComponent::State)i).Str())
            return (UIComponent::State)i;
    }
    MILO_ASSERT(false, 0x22);
    return UIComponent::kNumStates;
}

void UIComponent::SetState(UIComponent::State s) {
    if (!CanHaveFocus() && s == kFocused) {
        MILO_NOTIFY(
            "Component: %s cannot have focus.  Why are we setting it to the focused state?",
            Name()
        );
        s = kNormal;
    }
    mState = s;
}

void UIComponent::OldResourcePreload(BinStream &bs) {
    char c[264];
    bs.ReadString(c, 0x100);
}

void UIComponent::Init() {
    REGISTER_OBJ_FACTORY(UIComponent);
    sSelectFrames = SystemConfig("objects", "UIComponent")->FindInt("select_frames");
}

void UIComponent::Poll() {
    if (mSelected == 0)
        return;
    if (--mSelected != 0)
        return;
    FinishSelecting();
}

INIT_REVS(3, 0)

void UIComponent::PreLoad(BinStream &bs) {
    LOAD_REVS(bs);
    ASSERT_REVS(3, 0);
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndTransformable)
    LOAD_SUPERCLASS(RndDrawable)
    if (d.rev > 0) {
        d >> mNavRight;
        d >> mNavDown;
    }
    if (d.rev > 1 && d.rev < 3) {
        OldResourcePreload(bs);
    }
}

void UIComponent::PostLoad(BinStream &) {}

void UIComponent::SendSelect(LocalUser *user) {
    if (mState == kFocused) {
        SetState(kSelecting);
        static UIComponentSelectMsg select_msg(0, 0);
        select_msg[0] = this;
        select_msg[1] = user;
        TheUI->Handle(select_msg, false);
        if (mState != kSelecting)
            mSelectScreen = 0;
        else {
            mSelectScreen = TheUI->CurrentScreen();
            mSelectingUser = user;
            mSelected = sSelectFrames;
        }
    }
}

void UIComponent::FinishSelecting() {
    if (mState != kDisabled && mState != kNormal)
        SetState(kFocused);
    if (!mSelectCancelled && mSelectScreen == TheUI->CurrentScreen()) {
        static UIComponentSelectDoneMsg select_msg(this, 0);
        select_msg[0] = this;
        select_msg[1] = mSelectingUser;
        TheUI->Handle(select_msg, false);
    } else
        mSelectCancelled = false;
}

// Retail's END_HANDLERS still emits the PathName(this) side-effect call even
// though the notify print is stripped. Our global Debug.h release MILO_NOTIFY is
// ((void)sizeof(...)), and sizeof does NOT evaluate its operand, so PathName is
// dropped and the unhandled-msg tail goes missing. Locally redefine MILO_NOTIFY
// to comma-evaluate its args (matching rb3-Wii release Debug.h:151 MILO_WARN form)
// so PathName(this) is emitted (bl fn_82732F68) while the print stays stripped.
// NEVER edit global Debug.h — this is TU-local only.
#pragma push_macro("MILO_NOTIFY")
#undef MILO_NOTIFY
#define MILO_NOTIFY(...) (void)(__VA_ARGS__)
BEGIN_HANDLERS(UIComponent)
    HANDLE_EXPR(get_state, GetState())
    HANDLE_ACTION(set_state, SetState((UIComponent::State)_msg->Int(2)))
    HANDLE_EXPR(can_have_focus, CanHaveFocus())
    HANDLE_EXPR(get_resource_dir, ResourceDir())
    HANDLE(get_resources_path, OnGetResourcesPath)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
#pragma pop_macro("MILO_NOTIFY")
