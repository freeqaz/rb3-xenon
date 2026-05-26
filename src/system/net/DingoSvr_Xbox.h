#pragma once
#include "net/DingoSvr.h"
#include "net/XLSPConnection.h"
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
    XUID mXUID; // 0xb8
    String mUserName; // 0xc0
    XLSPConnection mXLSPConnection;
    String mXLSPFilter; // 0x140
    int mDingoServiceId;
    JobMgr mJobMgr; // 0x14c
    int mJobState; // 0x15c - tracks current job: 0=idle, 1=making, 2=joining, 3=starting, 4=writing, 5=ending, 6=leaving, 7=deleting
    u64 mScoreXUID; // 0x160 - XUID for leaderboard score submission
    u64 mCareerScore; // 0x168 - career score value to submit
    HANDLE mSessionHandle; // 0x170
    float mMsBetweenReconnDingo; // 0x174
    unsigned int mLeaderboardID; // 0x178
    unsigned int mLeaderboardScorePropID; // 0x17c
};

extern DingoSvrXbox gDingoSvrXbox;
