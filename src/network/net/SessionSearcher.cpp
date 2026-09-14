#include "net/SessionSearcher.h"
#include "net/NetSearchResult.h"
#include "net/NetSession.h"
#include "obj/Dir.h"
#include "obj/Msg.h"
#include "obj/ObjMacros.h"
#include "os/Debug.h"
#include "os/PlatformMgr.h"
#include "utl/Std.h"
#include <algorithm>

// Ported from rb3-Wii SessionSearcher.cpp (lane W16-AE, 2026-09-14). Retail
// 360 bytes at 0x823ead68-0x823eb8e0 differ from the Wii dev source in three
// ways, all reproduced here: the handler Symbols are function-local statics
// (RB3_HANDLE_LOCAL_STATIC; three statics + one shared guard word), the ctor's
// `invite_accepted` is a function-local static constructed AFTER SetName, and
// StopSearching's `search_finished_msg` is a function-local static Message
// constructed AFTER std::sort and BEFORE the virtual Handle call.

namespace {
    bool NumPlayersGreaterThan(const NetSearchResult *n1, const NetSearchResult *n2) {
        return n1->NumOpenSlots() < n2->NumOpenSlots();
    }
}

SessionSearcher::SessionSearcher()
    : mLastInviteResult(0), mSearching(0), mNextResult(0) {
    SetName("session_searcher", ObjectDir::Main());
    static Symbol invite_accepted("invite_accepted");
    ThePlatformMgr.AddSink(this, invite_accepted);
}

void SessionSearcher::AllocateNetSearchResults() {
    mLastInviteResult = NetSearchResult::New();
}

SessionSearcher::~SessionSearcher() {
    ThePlatformMgr.RemoveSink(this);
    delete mLastInviteResult;
    DeleteAll(mSearchList);
}

void SessionSearcher::Poll() {}

void SessionSearcher::StartSearching(User *, const SearchSettings &) {
    MILO_ASSERT(!mSearching, 0x3D);
    DeleteAll(mSearchList);
    mSearching = true;
}

void SessionSearcher::StopSearching() {
    mSearching = false;
    mNextResult = 0;
    std::sort(mSearchList.begin(), mSearchList.end(), NumPlayersGreaterThan);
    static Message search_finished_msg("search_finished");
    Handle(search_finished_msg, false);
}

void SessionSearcher::GetSearchResults(std::vector<NetSearchResult *> &results) {
    for (std::vector<NetSearchResult *>::iterator it = mSearchList.begin();
         it != mSearchList.end();
         ++it) {
        results.push_back(*it);
    }
}

NetSearchResult *SessionSearcher::GetNextResult() {
    MILO_ASSERT(!mSearching, 0x5D);
    if (mNextResult >= mSearchList.size())
        return nullptr;
    else
        return mSearchList[mNextResult++];
}

void SessionSearcher::ClearSearchResults() {
    MILO_ASSERT(!mSearching, 0x65);
    DeleteAll(mSearchList);
}

void SessionSearcher::UpdateSearchList(NetSearchResult *res) {
    mSearchList.push_back(res);
}

bool SessionSearcher::OnMsg(const InviteAcceptedMsg &) { return true; }

BEGIN_HANDLERS(SessionSearcher)
    HANDLE_ACTION(stop_searching, StopSearching())
    HANDLE_EXPR(get_next_result, GetNextResult())
    HANDLE_EXPR(get_last_invite_result, mLastInviteResult)
    HANDLE_MESSAGE(InviteAcceptedMsg)
    HANDLE_SUPERCLASS(MsgSource)
    HANDLE_CHECK(0x7F)
END_HANDLERS
