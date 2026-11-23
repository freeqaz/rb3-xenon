#pragma once
#include "rndobj/Poll.h"
#include <list>

/** "Workhorse unit of the Character system, most Character things inherit from this." */
class CharPollable : public RndPollable {
public:
    CharPollable() {}
    virtual void
    PollDeps(std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change) = 0;
};

class CharPollableSorter {
public:
    struct Dep {
        // Retail's map<Object*, Dep>::operator[] (fn_82373BB0) builds the
        // default value on its stack: obj (+0x80) and poll (+0x84) zeroed, the
        // list sentinel self-linked (+0x94, +0x98), searchID (r31+0x70) never
        // written. The Dep copy (fn_823710E8, `lwz r11,0x10(r30)` +0x3C) then
        // copies that stack slot into the node, so a fresh node's searchID is
        // stale stack bytes. ChangedByRecurse (fn_8236E980) compares it with
        // sSearchID (`lwz r10,0x10(r4)` +0x30, `cmpw` +0x38) before storing it
        // (+0x48): retail skips a fresh dep's chain if the garbage happens to
        // equal the search ID. Natively it starts at 0, which no search sees:
        // sSearchID starts at 0 and Sort (fn_823741F0, +0x3A8..+0x3B4)
        // increments it before every ChangedByRecurse. A determinism fix (the
        // memcheck read in rb3-render), not a retail behaviour.
#ifdef HX_NATIVE
        Dep() : obj(nullptr), poll(nullptr), searchID(0) {}
#else
        Dep() : obj(nullptr), poll(nullptr) {}
#endif
        Hmx::Object *obj; // 0x0
        std::list<Dep *> changedBy; // 0x4
        RndPollable *poll; // 0xc
        int searchID; // 0x10
    };

    struct AlphaSort {
        bool operator()(Dep *d1, Dep *d2) const {
            return strcmp(d1->obj->Name(), d2->obj->Name()) < 0;
        }
    };

    void Sort(std::vector<RndPollable *> &);

protected:
    bool ChangedBy(Dep *, Dep *);

    static int sSearchID;

    std::map<Hmx::Object *, Dep> mDeps;
    Dep *mTarget;

protected:
    void AddDeps(Dep *, const std::list<Hmx::Object *> &, std::list<Dep *> &, bool);
    bool ChangedByRecurse(Dep *);
};
