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
