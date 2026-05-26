#include "hamobj/MocapSkeletonIterator.h"
#include "ClipPlayer.h"
#include "HamRegulate.h"
#include "hamobj/HamCharacter.h"
#include "hamobj/HamDirector.h"
#include "hamobj/HamGameData.h"
#include "math/Utl.h"
#include "obj/Task.h"
#include "os/Debug.h"

MocapSkeletonIterator::MocapSkeletonIterator(int x, int y)
    : mDancer(TheHamDirector->GetCharacter(0)), mInput(mDancer), mStartFrame(x), mEndFrame(y) {
    MILO_ASSERT(TheGameData, 0x16);
    mSkeleton.Init();
    mPrevFrame = -kHugeFloat;
    mCurrentFrame = mStartFrame;
    mSavedSeconds = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    HamCharacter *c = mDancer;
    if (mDancer) {
        mSavedXfm = mDancer->LocalXfm();
        c->Enter();
        mDancer->DirtyLocalXfm().Reset();
        mDancer->Teleport(nullptr);
        Update();
        mInput.ResetSkeletonCharOrigin();
    }
}

MocapSkeletonIterator::~MocapSkeletonIterator() {
    if (mDancer) {
        mDancer->DirtyLocalXfm().Reset();
        mDancer->Teleport(nullptr);
        mDancer->SetLocalXfm(mSavedXfm);
    }
    TheTaskMgr.SetSeconds(mSavedSeconds, true);
}

MocapSkeletonIterator::operator bool() { return mDancer && mCurrentFrame < mEndFrame; }

void MocapSkeletonIterator::operator++() {
    mCurrentFrame++;
    Update();
}

bool MocapSkeletonIterator::PrevSkeleton(
    const Skeleton &s, int i2, ArchiveSkeleton &as, int &i3
) const {
    return PrevFromArchive(*this, s, i2, as, i3);
}

void MocapSkeletonIterator::Update() {
    MILO_ASSERT(mDancer, 0x55);
    TheTaskMgr.SetSeconds(mCurrentFrame * 0.033333335f, (mStartFrame - mCurrentFrame) == 0);
    ClipPlayer player;
    if (player.Init(0)) {
        player.PlayAnims(mDancer, mCurrentFrame, mPrevFrame, 0);
        mPrevFrame = mCurrentFrame;
    } else {
        MILO_NOTIFY(
            "Failed to init ClipPlayer for %s!", PathName(TheHamDirector->ClipDir())
        );
    }
    HamRegulate *reg = mDancer->Regulator();
    reg->SetWaypoint(nullptr);
    mDancer->Poll();
    if (mSkeleton.IsTracked()) {
        AddToHistory(0, mSkeleton);
    }
    mInput.PollTracking();
    const SkeletonFrame *frame_data = mInput.NewFrame();
    MILO_ASSERT(frame_data, 0x6F);
    MILO_ASSERT(frame_data->mElapsedMs == 33, 0x70);
    MILO_ASSERT(frame_data->mSkeletonDatas[0].mTracking == kSkeletonTracked, 0x71);
    mSkeleton.Poll(0, *frame_data);
}
