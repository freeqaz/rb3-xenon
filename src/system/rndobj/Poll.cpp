#include "rndobj/Poll.h"
#include "obj/Msg.h"

BEGIN_HANDLERS(RndPollable)
    HANDLE_ACTION(enter, Enter())
    HANDLE_ACTION(poll, Poll())
    HANDLE_ACTION(exit, Exit())
END_HANDLERS

void RndPollable::Enter() {
    static Message enter("enter");
    HandleType(enter);
}

void RndPollable::Exit() {
    static Message exit("exit");
    HandleType(exit);
}
