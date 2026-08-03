#include "char/CharPollGroup.h"
#include "char/CharPollable.h"
#include "char/CharWeightable.h"
#include "obj/Object.h"
#include "rndobj/Trans.h"
#include <algorithm>

CharPollGroup::CharPollGroup() : mPolls(this), mChangedBy(this), mChanges(this) {}


BEGIN_HANDLERS(CharPollGroup)
    HANDLE_ACTION(sort_polls, SortPolls())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharPollGroup)
    SYNC_PROP(polls, mPolls)
    SYNC_PROP(changed_by, mChangedBy)
    SYNC_PROP(changes, mChanges)
    SYNC_SUPERCLASS(CharWeightable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(CharPollGroup)
    SAVE_REVS(3, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(CharWeightable)
    bs << mPolls;
    bs << mChangedBy;
    bs << mChanges;
END_SAVES

BEGIN_COPYS(CharPollGroup)
    COPY_SUPERCLASS(Hmx : Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharPollGroup)
    BEGIN_COPYING_MEMBERS
        if (ty == kCopyFromMax) {
            FOREACH (it, c->mPolls) {
                if (!mPolls.find(*it)) {
                    mPolls.push_back(*it);
                }
            }
        } else {
            COPY_MEMBER(mPolls)
            COPY_MEMBER(mChangedBy)
            COPY_MEMBER(mChanges)
        }
    END_COPYING_MEMBERS
END_COPYS

// RB3-360 retail rev dialect (rb3-Wii/ObjMacros shape): the packed rev is split
// into two HALFWORDS stored four bytes apart onto ONE internal-linkage align(4)
// base, and the RAW incoming BinStream is forwarded to every read and to the
// superclass Load.  DC3's Object.h BinStreamRev stack decorator additionally
// emits ??0BinStream, a ??_7BinStreamRev@@6B@ vtable store and a ??1BinStream
// destructor that retail has none of, and dispatches each read on `&d`.
//
// Written longhand rather than by including obj/ObjMacros.h: that header also
// swaps the SYNC_PROP and HANDLE families, which are already byte-exact here.
// The pair MUST share one aggregate -- two separate file statics are laid out
// independently and will not fold onto a single base register.  No `#define
// gRev` alias: several of these TUs are scatter-INCLUDED into another unit
// (e.g. rndobj/Anim.cpp includes rndobj/MotionBlur.cpp) whose own gRev macro
// the alias would silently shadow for the rest of the amalgamated TU.
static struct {
    __declspec(align(4)) unsigned short altRev;
    __declspec(align(4)) unsigned short rev;
} gRevs_CharPollGroup;
BEGIN_LOADS(CharPollGroup)
    int rev;
    bs >> rev;
    gRevs_CharPollGroup.rev = getHmxRev(rev);
    gRevs_CharPollGroup.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    if (gRevs_CharPollGroup.rev > 2) {
        CharWeightable::Load(bs);
    }
    bs >> mPolls;
    if (gRevs_CharPollGroup.rev > 1) {
        bs >> mChangedBy;
        bs >> mChanges;
    }
END_LOADS

void CharPollGroup::Poll() {
    if (Weight()) {
        FOREACH (it, mPolls) {
            (*it)->Poll();
        }
    }
}

void CharPollGroup::Enter() {
    FOREACH (it, mPolls) {
        (*it)->Enter();
    }
}

void CharPollGroup::Exit() {
    FOREACH (it, mPolls) {
        (*it)->Exit();
    }
}

void CharPollGroup::ListPollChildren(std::list<RndPollable *> &l) const {
    FOREACH (it, mPolls) {
        l.push_back(*it);
    }
}

void CharPollGroup::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    if (mChangedBy || mChanges) {
        changedBy.push_back(mChangedBy);
        change.push_back(mChanges);
    } else {
        FOREACH (it, mPolls) {
            (*it)->PollDeps(changedBy, change);
        }
    }
}

void CharPollGroup::SortPolls() {
    CharPollableSorter sorter;
    std::vector<RndPollable *> polls;
    polls.reserve(mPolls.size());
    FOREACH (it, mPolls) {
        polls.push_back(*it);
    }
    sorter.Sort(polls);
    mPolls.clear();
    for (int i = 0; i < polls.size(); i++) {
        mPolls.push_back(dynamic_cast<CharPollable *>(polls[i]));
    }
}

int CharPollableSorter::sSearchID = 0;

void CharPollableSorter::AddDeps(
    Dep *dep,
    const std::list<Hmx::Object *> &objs,
    std::list<Dep *> &deps,
    bool isChangedBy
) {
    for (std::list<Hmx::Object *>::const_iterator it = objs.begin(); it != objs.end();
         ++it) {
        Hmx::Object *cur = *it;
        if (cur) {
            Dep *mapDep = &mDeps[cur];
            if (!mapDep->obj) {
                mapDep->obj = cur;
                deps.push_back(mapDep);
            }
            if (isChangedBy) {
                dep->changedBy.push_back(mapDep);
            } else {
                mapDep->changedBy.push_back(dep);
            }
        }
    }
}

bool CharPollableSorter::ChangedByRecurse(Dep *dep) {
    if (!dep)
        return false;
    if (dep == mTarget)
        return true;
    if (dep->searchID == sSearchID)
        return false;
    dep->searchID = sSearchID;
    for (std::list<Dep *>::iterator it = dep->changedBy.begin();
         it != dep->changedBy.end();
         ++it) {
        if (ChangedByRecurse(*it))
            return true;
    }
    return false;
}

bool CharPollableSorter::ChangedBy(Dep *a, Dep *b) {
    mTarget = b;
    sSearchID++;
    return ChangedByRecurse(a);
}

void CharPollableSorter::Sort(std::vector<RndPollable *> &polls) {
    std::vector<Dep *> deps;
    deps.reserve(polls.size());
    for (int i = polls.size() - 1, last = i; i >= 0; i--) {
        CharPollable *c = dynamic_cast<CharPollable *>(polls[i]);
        if (c) {
            Dep &dep = mDeps[c];
            dep.obj = c;
            dep.poll = c;
            deps.push_back(&dep);
        } else {
            polls[last--] = polls[i];
        }
    }
    if (deps.empty())
        return;
    else {
        std::sort(deps.begin(), deps.end(), CharPollableSorter::AlphaSort());
        std::list<Dep *> depList;
        for (int i = 0; i < deps.size(); i++)
            depList.push_back(deps[i]);
        while (!depList.empty()) {
            Dep *curDep = depList.back();
            depList.pop_back();
            CharPollable *c = dynamic_cast<CharPollable *>(curDep->obj);
            if (c) {
                std::list<Hmx::Object *> depList1;
                std::list<Hmx::Object *> depList2;
                c->PollDeps(depList1, depList2);
                AddDeps(curDep, depList1, depList, true);
                AddDeps(curDep, depList2, depList, false);
            }
            RndTransformable *t = dynamic_cast<RndTransformable *>(curDep->obj);
            if (t) {
                std::list<Hmx::Object *> tDepList;
                tDepList.push_back(t->TransParent());
                AddDeps(curDep, tDepList, depList, true);
            }
        }

        std::list<Dep *> otherDepList;
        for (int i = 0; i < deps.size(); i++) {
            Dep *curDep = deps[i];
            std::list<Dep *>::iterator it = otherDepList.begin();
            for (; it != otherDepList.end(); ++it) {
                if (ChangedBy(curDep, *it))
                    break;
            }
            otherDepList.insert(it, curDep);
        }

        int idx = 0;
        for (std::list<Dep *>::iterator it = otherDepList.begin();
             it != otherDepList.end();
             ++it) {
            polls[idx++] = (*it)->poll;
        }
    }
}
