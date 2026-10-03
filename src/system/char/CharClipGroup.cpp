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
    HANDLE_EXPR(get_clip, GetClip())
    HANDLE_ACTION(delete_remaining, DeleteRemaining(_msg->Int(2)))
    HANDLE_EXPR(get_size, (int)mClips.size())
    HANDLE_EXPR(has_clip, HasClip(_msg->Obj<CharClip>(2)))
    HANDLE_EXPR(find_clip, GetClip(_msg->Int(2)))
    HANDLE_ACTION(add_clip, AddClip(_msg->Obj<CharClip>(2)))
    HANDLE_ACTION(set_clip_flags, SetClipFlags(_msg->Int(2)))
    HANDLE_ACTION(randomize_index, RandomizeIndex())
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

// RB3 retail rev dialect (LOAD_REVS / gRev): Load (0x823901e8) splits
// the packed rev into two halfword file statics -- alt at the base (retail
// 0x82CBF164), rev at +4 -- and reads mFlags only when rev > 1. No
// BinStreamRev on the stack, no clamp on mWhich. Same shape as EventTrigger:
// two SEPARATE align(4) statics, initialised to 0 so they are laid out in
// declaration order (alt first).
static __declspec(align(4)) unsigned short gAltRev_CharClipGroup = 0;
static __declspec(align(4)) unsigned short gRev_CharClipGroup = 0;

BEGIN_LOADS(CharClipGroup)
    int revs;
    bs >> revs;
    gRev_CharClipGroup = getHmxRev(revs);
    gAltRev_CharClipGroup = getAltRev(revs);
    Hmx::Object::Load(bs);
    bs >> mClips;
    bs >> mWhich;
    if (gRev_CharClipGroup > 1) {
        bs >> mFlags;
    } else {
        mFlags = 0;
    }
END_LOADS

void CharClipGroup::AddClip(CharClip *clip) {
    if (!HasClip(clip)) {
        mClips.push_back(ObjOwnerPtr<CharClip>(this, clip));
    }
}

// Retail HasClip (0x8238E3C8) and its inlined copy in AddClip (0x82390368) both
// call STLport's out-of-line random-access __find (0x8238DA40: 4-way unrolled,
// comparing ObjOwnerPtr::mObject at +8 against *(&clip)), not an index loop.
bool CharClipGroup::HasClip(CharClip *clip) const {
    return std::find(mClips.begin(), mClips.end(), clip) != mClips.end();
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

// Retail 0x8238E500: finds the clip (unsigned index compare) and tail-calls
// MakeMRU(int).
void CharClipGroup::MakeMRU(CharClip *clip) {
    for (int i = 0; i < mClips.size(); i++) {
        if (mClips[i] == clip) {
            MakeMRU(i);
            return;
        }
    }
}

struct Alphabetically {
    bool operator()(Hmx::Object *c1, Hmx::Object *c2) const {
        return strcmp(c1->Name(), c2->Name()) < 0;
    }
};

void CharClipGroup::Randomize() {
    for (int i = 0; i < mClips.size(); i++) {
        std::swap(mClips[i], mClips[RandomInt(i, mClips.size())]);
    }
}

// Retail 0x8238DC68, the randomize_index handler's callee: picks a random
// current index without reordering the clips.
void CharClipGroup::RandomizeIndex() {
    int n = mClips.size();
    if (n)
        mWhich = RandomInt(0, n);
}

// retail 0x8238F270: swap the replaced clip for `to` (or drop it), keeping
// mWhich on the same clip. As in RndMeshAnim::Replace, the first argument is
// compared as the object being replaced (retail: entry+8 == r4). Object's own
// Replace is empty in retail and is not called.
void CharClipGroup::Replace(ObjRef *from, Hmx::Object *to) {
    for (int idx = 0; idx < mClips.size(); idx++) {
        if (mClips[idx] == reinterpret_cast<Hmx::Object *>(from)) {
            mClips[idx] = dynamic_cast<CharClip *>(to);
        }
        if (!mClips[idx]) {
            mClips.erase(mClips.begin() + idx);
            int s = mClips.size();
            if (mWhich > idx) {
                mWhich--;
            } else if (mWhich == s) {
                mWhich = Min<int>(0, s - 1);
            }
            return;
        }
    }
}

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
