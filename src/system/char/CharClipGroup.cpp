#include "char/CharClipGroup.h"
#include "CharClipGroup.h"
#include "char/CharClip.h"
#include "obj/ObjPtrVec_impl.h"
#include "math/Rand.h"
#include "math/Utl.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Mat.h"
#include "utl/Str.h"
#include <algorithm>
#include <cstring>

CharClipGroup::CharClipGroup() : mClips(this), mWhich(0), mFlags(0) {}

BEGIN_HANDLERS(CharClipGroup)
    HANDLE_EXPR(get_clip, GetClip(0))
    HANDLE_ACTION(delete_remaining, DeleteRemaining(_msg->Int(2)))
    HANDLE_EXPR(get_size, (int)mClips.size())
    HANDLE_EXPR(has_clip, HasClip(_msg->Obj<CharClip>(2)))
    HANDLE_EXPR(find_clip, GetClip(_msg->Int(2)))
    HANDLE_ACTION(add_clip, AddClip(_msg->Obj<CharClip>(2)))
    HANDLE_ACTION(set_clip_flags, SetClipFlags(_msg->Int(2)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharClipGroup)
    SYNC_PROP(clips, mClips)
    SYNC_PROP(flags, mFlags)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharClipGroup)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mClips;
    bs << mWhich;
    bs << mFlags;
END_SAVES

BEGIN_COPYS(CharClipGroup)
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharClipGroup)
    BEGIN_COPYING_MEMBERS
        if (ty == kCopyFromMax) {
            for (int i = 0; i < c->mClips.size(); i++) {
                CharClip *curClip = (CharClip *)c->mClips[i];
                if (!FindClip(curClip->Name())) {
                    mClips.push_back(ObjOwnerPtr<CharClip>(this, curClip));
                }
            }
        } else
            COPY_MEMBER(mClips)
        COPY_MEMBER(mWhich)
        COPY_MEMBER(mFlags)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(2, 0)

BEGIN_LOADS(CharClipGroup)
    LOAD_REVS(bs)
    ASSERT_REVS(2, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    d.stream >> mClips;
    d >> mWhich;
    mWhich = Max(mWhich, 0);
    if (d.rev > 1) {
        d >> mFlags;
    } else {
        mFlags = 0;
    }
END_LOADS

void CharClipGroup::AddClip(CharClip *clip) {
    if (!HasClip(clip)) {
        mClips.push_back(ObjOwnerPtr<CharClip>(this, clip));
    }
}

bool CharClipGroup::HasClip(CharClip *clip) const {
    for (int i = 0; i < mClips.size(); i++) {
        if ((CharClip *)mClips[i] == clip)
            return true;
    }
    return false;
}

CharClip *CharClipGroup::GetClip() {
    if (mClips.empty())
        return nullptr;
    mWhich++;
    if (mWhich >= mClips.size())
        mWhich = 0;
    return mClips[mWhich];
}

CharClip *CharClipGroup::GetClip(int flags) {
    int size = mClips.size();
    if (size == 0)
        return nullptr;
    int which = mWhich;
    for (int i = which + 1; i < size; i++) {
        CharClip *clip = mClips[i];
        if ((clip->Flags() & flags) == flags) {
            MakeMRU(i);
            return clip;
        }
    }
    for (int i = 0; i <= which; i++) {
        CharClip *clip = mClips[i];
        if ((clip->Flags() & flags) == flags) {
            MakeMRU(i);
            return clip;
        }
    }
    return nullptr;
}

void CharClipGroup::MakeMRU(int i) {
    int which = mWhich;
    if (i == which)
        return;
    unsigned int next = which + 1;
    if (next >= mClips.size())
        next = 0;
    if ((int)next == i) {
        mWhich = i;
        return;
    }
    CharClip *temp = mClips[i];
    if (i > which) {
        mWhich++;
        for (int k = i; k > mWhich; k--) {
            mClips[k] = mClips[k - 1];
        }
    } else {
        for (int k = i; k < mWhich; k++) {
            mClips[k] = mClips[k + 1];
        }
    }
    mClips[mWhich] = temp;
}

struct Alphabetically {
    bool operator()(Hmx::Object *c1, Hmx::Object *c2) const {
        return strcmp(c1->Name(), c2->Name()) < 0;
    }
};

void CharClipGroup::Sort() { std::sort(mClips.begin(), mClips.end(), Alphabetically()); }

void CharClipGroup::DeleteRemaining(int i1) {
    CharClip *clips[256];
    MILO_ASSERT(mClips.size() < 256, 0x88);
    for (int i = 0; i < mClips.size(); i++) {
        clips[i] = mClips[i];
    }
    CharClip::LockAndDelete(clips, mClips.size(), i1);
}

CharClip *CharClipGroup::FindClip(const char *clipName) const {
    for (int i = 0; i < mClips.size(); i++) {
        if (streq(clipName, mClips[i]->Name())) {
            return mClips[i];
        }
    }
    return nullptr;
}

void CharClipGroup::SetClipFlags(int flags) {
    for (int i = 0; i < mClips.size(); i++) {
        CharClip *cur = mClips[i];
        cur->SetFlags(cur->Flags() | flags);
    }
}

template <>
BinStream &operator<<(BinStream &bs, const ObjPtrVec<RndMat, ObjectDir> &c) {
    bs << (int)c.size();
    MILO_ASSERT(c.Owner(), 0x525);
    for (int i = 0; i < (int)c.size(); i++) {
        const Hmx::Object *obj = c[i];
        const char *name = obj ? obj->Name() : "";
        bs << name;
    }
    return bs;
}
