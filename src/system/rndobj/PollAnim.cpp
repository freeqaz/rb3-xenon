#include "rndobj/PollAnim.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "rndobj/Anim.h"
#include "rndobj/Poll.h"
#include "utl/BinStream.h"

#pragma region Hmx::Object

RndPollAnim::RndPollAnim() : mAnims(this) {}

BEGIN_HANDLERS(RndPollAnim)
    HANDLE_SUPERCLASS(RndAnimatable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(RndPollAnim)
    SYNC_PROP(anims, mAnims)
    SYNC_SUPERCLASS(RndAnimatable)
    SYNC_SUPERCLASS(RndPollable)
#ifdef HX_NATIVE
    // RB3-360 retail SyncProperty chain stops at the immediate superclass;
    // DC3's extra direct Hmx::Object chain is native-only.
    SYNC_SUPERCLASS(Hmx::Object)
#endif
END_PROPSYNCS

BEGIN_SAVES(RndPollAnim)
    SAVE_REVS(0, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndAnimatable)
    SAVE_SUPERCLASS(RndPollable)
    bs << mAnims;
END_SAVES

BEGIN_COPYS(RndPollAnim)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndAnimatable)
    COPY_SUPERCLASS(RndPollable)
    CREATE_COPY(RndPollAnim)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mAnims)
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
} gRevs_PollAnim;
BEGIN_LOADS(RndPollAnim)
    int rev;
    bs >> rev;
    gRevs_PollAnim.rev = getHmxRev(rev);
    gRevs_PollAnim.altRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    RndAnimatable::Load(bs);
    RndPollable::Load(bs);
    bs >> mAnims;
END_LOADS

#pragma endregion
#pragma region RndAnimatable

float RndPollAnim::EndFrame() {
    float frame = 0;
    FOREACH (it, mAnims) {
        MaxEq(frame, (*it)->EndFrame());
    }
    return frame;
}

void RndPollAnim::ListAnimChildren(std::list<RndAnimatable *> &children) const {
    FOREACH (it, mAnims) {
        children.push_back(*it);
    }
}

#pragma endregion
#pragma region RndPollable

void RndPollAnim::Poll() {
    FOREACH (it, mAnims) {
        RndAnimatable *cur = *it;
        float frame = 0;
        switch (cur->GetRate()) {
        case RndAnimatable::k30_fps:
            frame = 30.0f * TheTaskMgr.Seconds(TaskMgr::kRealTime);
            break;
        case RndAnimatable::k480_fpb:
            frame = 480.0f * TheTaskMgr.Beat();
            break;
        case RndAnimatable::k30_fps_ui:
            frame = 30.0f * TheTaskMgr.UISeconds();
            break;
        case RndAnimatable::k1_fpb:
            frame = TheTaskMgr.Beat();
            break;
        case RndAnimatable::k30_fps_tutorial:
            frame = 30.0f * TheTaskMgr.TutorialSeconds();
            break;
        case RndAnimatable::k15_fpb:
            frame = 15.0f * TheTaskMgr.Beat();
            break;
        }
        cur->SetFrame(frame, 1);
    }
}

void RndPollAnim::Enter() {
    RndPollable::Enter();
    FOREACH (it, mAnims) {
        (*it)->StartAnim();
    }
}

void RndPollAnim::Exit() {
    RndPollable::Exit();
    FOREACH (it, mAnims) {
        (*it)->EndAnim();
    }
}

#pragma endregion
