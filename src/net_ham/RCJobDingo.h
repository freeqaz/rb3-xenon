#pragma once
#include "net/DingoJob.h"
#include "obj/Object.h"
#include "obj/Msg.h"

class RCJob : public DingoJob {
public:
    RCJob(const char *url, Hmx::Object *callback);
    virtual ~RCJob();

protected:
    virtual void SendCallback(bool success, bool cancelled);
};

DECLARE_MESSAGE(RCJobCompleteMsg, "rc_job_complete")
RCJobCompleteMsg(RCJob *job, bool success) : Message(Type(), job, success) {}
RCJob *Job() const { return mData->Obj<RCJob>(2); }
bool Success() const { return mData->Int(3); }
END_MESSAGE
