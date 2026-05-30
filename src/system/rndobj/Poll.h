#pragma once
#include "obj/Object.h"
#include <list>

/**
 * @brief: An object that can be polled.
 * Original _objects description:
 * "Abstract base class for pollable RND objects"
 */
class RndPollable : public virtual Hmx::Object {
public:
    OBJ_CLASSNAME(Poll);
    OBJ_SET_TYPE(Poll);
    virtual DataNode Handle(DataArray *, bool);
    /** Poll this object. */
    virtual void Poll() {}
    virtual void Enter();
    virtual void Exit();
    /** Get the list of this Object's children that are pollable. */
    virtual void ListPollChildren(std::list<RndPollable *> &) const {}
    // RB3 retail (matching rb3-Wii) has no virtual PollEnabled() in RndPollable's
    // vtable: its own-vtable slot 0 is Poll(). DC3 added a virtual PollEnabled()
    // later, which shifted Poll() to slot 1 and broke SharedGroup::TryPoll's
    // dispatch. Keep it as a non-virtual helper (always true; never overridden in
    // RB3) so the few DC3-derived callers in Utl/Dir still compile without
    // perturbing the vtable layout.
    bool PollEnabled() const { return true; }
};
