#include "utl/JobMgr.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "utl/DataPointMgr.h"

namespace {
    static int gJobIDCounter;
}

Job::Job() {
    mID = gJobIDCounter++;
}

void JobMgr::Poll() {
    if (!mJobQueue.empty()) {
        if (mJobQueue.front()->IsFinished()) {
            Job *job = mJobQueue.front();
            mJobQueue.pop_front();
            mPreventStart = true;
            job->OnCompletion(mCallback);
            delete job;
            mPreventStart = false;
            if (!mJobQueue.empty()) {
                mJobQueue.front()->Start();
            }
        }
    }
}

void JobMgr::CancelJob(int id) {
    std::list<Job *>::iterator it = mJobQueue.begin();
    while (it != mJobQueue.end()) {
        Job *job = *it;
        if (job->ID() == id) {
            int frontID = mJobQueue.front()->ID();
            it = mJobQueue.erase(it);
            bool oldstart = mPreventStart;
            mPreventStart = true;
            job->Cancel(mCallback);
            mPreventStart = oldstart;
            if (frontID == id && !oldstart && it != mJobQueue.end()) {
                (*it)->Start();
            }
            delete job;
            return;
        }
        ++it;
    }
    MILO_NOTIFY("This job is not in the queue %i", id);
}

JobMgr::JobMgr(Hmx::Object *o) : mCallback(o), mJobQueue(), mPreventStart(0) {}

void JobMgr::QueueJob(Job *j) {
    mJobQueue.push_back(j);
    if (mJobQueue.size() == 1 && !mPreventStart) {
        mJobQueue.front()->Start();
    }
}

void JobMgr::CancelAllJobs() {
    std::list<Job *> list = mJobQueue;
    mJobQueue.clear();
    for (std::list<Job *>::const_iterator it = list.begin(); it != list.end(); ++it) {
        (*it)->Cancel(mCallback);
        delete *it;
    }
}

JobMgr::~JobMgr() { CancelAllJobs(); }
