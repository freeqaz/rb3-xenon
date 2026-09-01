#pragma once
#include "net/DingoSvr.h"
#include "net/XLSPConnection.h"
#include "os/Timer.h"
#include "utl/JobMgr.h"
#include "utl/Str.h"

class DingoSvrXbox : public DingoServer {
public:
    DingoSvrXbox();
    virtual DataNode Handle(DataArray *, bool);
    virtual void Init();
    virtual void CreateAccount() {}
    virtual bool Authenticate(int padnum);
    virtual void Logout();
    virtual void Disconnect();
    virtual void Poll();
    virtual const char *GetPlatform() { return "xbl"; }
    virtual void SetLBView(unsigned int lb_id) { mLeaderboardID = lb_id; }
    virtual void SetLBScoreProperty(unsigned int prop_id) {
        mLeaderboardScorePropID = prop_id;
    }
    virtual bool HasValidLoginCandidate() const;
    virtual bool IsValidLoginCandidate(int padnum) const;
    virtual void MakeSessionJobComplete(bool success);
    virtual void JoinSessionComplete(bool success);
    virtual void StartSessionComplete(bool success);
    virtual void WriteCareerLeaderboardComplete(bool success);
    virtual void LeaveSessionComplete(bool success);
    virtual void EndSessionComplete(bool success);
    virtual void DeleteSessionComplete(bool success);
    virtual void StartUploadCareerScore(u64 career_score);

private:
    int GetValidLoginCandidate(char *, u64 &) const;
    void CreateSession();

protected:
    virtual void FillAuthParams(DataPoint &pt);
    virtual bool FillAuthParamsFromPadNum(DataPoint &pt, int padnum);
    virtual void OnAuthSuccess();

    int mXLSPState;
    int unkb4;
    XUID mXUID; // 0xc8
    String mUserName; // 0xd0
    XLSPConnection mXLSPConnection;
    String mXLSPFilter; // 0x128
    int mDingoServiceId;
    JobMgr mJobMgr; // 0x138
    int mJobState; // 0x148 - tracks current job: 0=idle, 1=making, 2=joining, 3=starting, 4=writing, 5=ending, 6=leaving, 7=deleting
    u64 mScoreXUID; // 0x150 - XUID for leaderboard score submission
    u64 mCareerScore; // 0x158 - career score value to submit
    HANDLE mSessionHandle; // 0x160
    float mMsBetweenReconnDingo; // 0x164
    unsigned int mLeaderboardID; // 0x168
    unsigned int mLeaderboardScorePropID; // 0x16c
    // Not present at these offsets in retail -- this unit is NonMatching /
    // unpinned (config/45410914/objects.json), so exact layout below this
    // point is not yet load-bearing. mReconnectTimer moved here from
    // XLSPConnection (see net/XLSPConnection.h) because retail's
    // XLSPConnection has no such member; DingoSvrXbox::Poll is its only
    // consumer, so it owns the timer directly now.
    Timer mReconnectTimer;
    int mPrevXLSPState;
};

extern DingoSvrXbox gDingoSvrXbox;
