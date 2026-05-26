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
#include <cstring>

#ifndef HX_NATIVE
// Explicit template instantiation (STLport only)
namespace stlpmtx_std {
    template class vector<ObjPtrVec<CharClip, ObjectDir>::Node, StlNodeAlloc<ObjPtrVec<CharClip, ObjectDir>::Node>>;
}
#endif

CharClipGroup::CharClipGroup()
    : mClips(this, (EraseMode)1), mWhich(0), unk24(0), mFlags(0) {}

BEGIN_HANDLERS(CharClipGroup)
    HANDLE_EXPR(get_clip, GetClip(0))
    HANDLE_ACTION(delete_remaining, DeleteRemaining(_msg->Int(2)))
    HANDLE_EXPR(get_size, mClips.size())
    HANDLE_EXPR(has_clip, HasClip(_msg->Obj<CharClip>(2)))
    HANDLE_EXPR(find_clip, GetClip(_msg->Int(2)))
    HANDLE_ACTION(add_clip, AddClip(_msg->Obj<CharClip>(2)))
    HANDLE_ACTION(set_clip_flags, SetClipFlags(_msg->Int(2)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharClipGroup)
    SYNC_PROP(clips, mClips)
    SYNC_PROP(flags, mFlags)
    SYNC_SUPERCLASS(Hmx::Object)
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
    mClips.Load(d.stream, true, nullptr);
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
    return mClips.end() != mClips.find(clip);
}

int CharClipGroup::QueueRandom(int pos, int end) const {
    int diff = end - pos;
    int range = (diff < 0 ? mClips.size() : 0) + diff;
    int result = Rand::sRand.FastInt(0, range) + pos;
    int size = mClips.size();
    return result - ((result >= size) ? size : 0);
}

CharClip *CharClipGroup::GetClip(int flags) {
    if (!mClips.size()) {
        return nullptr;
    }

    {
        int sz = (int)mClips.size() - 1;
        if (sz < mWhich) mWhich = sz;
    }
    unk24 = Min((int)mClips.size() - 1, unk24);

    int origWhich = mWhich;
    int origUnk24 = unk24;

    int pos = mWhich + 1;
    pos -= (pos >= mClips.size()) ? mClips.size() : 0;
    mWhich = pos;

    if (pos != origUnk24) {
        do {
            int swapIdx = QueueRandom(pos, origUnk24);
            mClips.swap(pos, swapIdx);
            CharClip *clip = mClips[pos];
            if ((clip->Flags() & flags) == flags) {
                mClips.swap(pos, mWhich);
                return clip;
            }
            pos++;
            pos -= (pos >= mClips.size()) ? mClips.size() : 0;
        } while (pos != origUnk24);
    }

    CharClip *clip = nullptr;
    if (pos != origWhich) {
        do {
            int swapIdx = QueueRandom(pos, origWhich);
            mClips.swap(pos, swapIdx);
            clip = mClips[pos];
            if ((clip->Flags() & flags) == flags) {
                mClips.swap(pos, mWhich);
                mClips.swap(pos, unk24);
                int newUnk24 = unk24 + 1;
                newUnk24 -= (newUnk24 >= mClips.size()) ? mClips.size() : 0;
                unk24 = newUnk24;
                return clip;
            }
            pos++;
            pos -= (pos >= mClips.size()) ? mClips.size() : 0;
        } while (pos != origWhich);
    }

    clip = mClips[pos];
    if ((clip->Flags() & flags) == flags) {
        mClips.swap(pos, mWhich);
        mClips.swap(pos, unk24);
        int newUnk24 = unk24 + 1;
        newUnk24 -= (newUnk24 >= mClips.size()) ? mClips.size() : 0;
        unk24 = newUnk24;
        return clip;
    }

    return nullptr;
}

struct Alphabetically {
    bool operator()(Hmx::Object *c1, Hmx::Object *c2) const {
        return strcmp(c1->Name(), c2->Name()) < 0;
    }
};

void CharClipGroup::Sort() { mClips.sort(Alphabetically()); }

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
            return (CharClip *)mClips[i];
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
