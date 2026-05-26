#include "gesture/StreamRecorder.h"
#include "StreamRecorder.h"
#include "obj/Data.h"
#include "obj/Dir.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Dir.h"
#include "rndobj/Draw.h"
#include "rndobj/Mat.h"
#include "rndobj/Overlay.h"
#include "rndobj/Poll.h"
#include "rndobj/Anim.h"
#include "rndobj/Rnd.h"
#include "rndobj/Tex.h"
#include "rndobj/TexRenderer.h"
#include "obj/Task.h"
#include "utl/Symbol.h"

StreamRecorder::StreamRecorder()
    : mInputDir(this), mTexRenderer(this), mBuffers(this), mOutputMat(this), mMaxFrames(0),
      mOutputWidth(320), mOutputHeight(240), mFramesRecorded(0), unkb4(0),
      mDebugFrame(-1), mPlaybackSpeed(3), mRecordingPos(-1.0f), mPlaybackPos(-1.0f), mPausedPos(-1.0f),
      mUseAlpha(true), mStopDelay(5), mStopTimer(0) {}

StreamRecorder::~StreamRecorder() {}

BEGIN_HANDLERS(StreamRecorder)
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndPollable)
    HANDLE_SUPERCLASS(Hmx::Object)
    HANDLE(start_recording, OnStartRecording)
    HANDLE(stop_recording, OnStopRecording)
    HANDLE(play_recording, OnPlayRecording)
    HANDLE(stop_playback, OnStopPlayback)
    HANDLE(pause_playback, OnPausePlayback)
    HANDLE(unpause_playback, OnUnpausePlayback)
    HANDLE(reset, OnReset)
END_HANDLERS

BEGIN_PROPSYNCS(StreamRecorder)
    SYNC_PROP_SET(input, mInputDir.Ptr(), SetPhotoInput(dynamic_cast<RndDir *>(_val.GetObj())))
    SYNC_PROP(use_alpha, mUseAlpha)
    SYNC_PROP(output_mat, mOutputMat)
    SYNC_PROP(playback_speed, mPlaybackSpeed)
    SYNC_PROP_MODIFY(max_frames, mMaxFrames, Reset())
    SYNC_PROP_SET(frames_recorded, mFramesRecorded, )
    SYNC_PROP_SET(debug_frame, mDebugFrame, SetDebugFrame(_val.Int()))
    SYNC_PROP(output_width, mOutputWidth)
    SYNC_PROP(output_height, mOutputHeight)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndPollable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(StreamRecorder)
    SAVE_REVS(5, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    SAVE_SUPERCLASS(RndPollable)
    bs << mOutputMat << mMaxFrames;
    bs << mUseAlpha;
    bs << mPlaybackSpeed << mOutputWidth << mOutputHeight;
END_SAVES

BEGIN_COPYS(StreamRecorder)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(RndPollable)
    CREATE_COPY_AS(StreamRecorder, c)
    BEGIN_COPYING_MEMBERS_FROM(c)
        COPY_MEMBER(mOutputMat)
        COPY_MEMBER(mMaxFrames)
        COPY_MEMBER(mUseAlpha)
        COPY_MEMBER(mPlaybackSpeed)
        COPY_MEMBER(mPlaybackPos)
        COPY_MEMBER(mPausedPos)
        COPY_MEMBER(mRecordingPos)
        COPY_MEMBER(mOutputWidth)
        COPY_MEMBER(mOutputHeight)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(5, 0)

BEGIN_LOADS(StreamRecorder)
    LOAD_REVS(bs)
    ASSERT_REVS(5, 0)
    LOAD_SUPERCLASS(Hmx::Object)
    LOAD_SUPERCLASS(RndDrawable)
    if (d.rev > 2) {
        LOAD_SUPERCLASS(RndPollable)
    }
    if (d.rev < 4) {
        ObjPtr<Hmx::Object> ptr(this);
        d >> ptr;
    }
    d >> mOutputMat;
    d >> mMaxFrames;
    if (d.rev < 2) {
        int x;
        d >> x;
    }
    d >> mUseAlpha;
    d >> (BinStreamEnum<RndAnimatable::Rate> &)mPlaybackSpeed;
    if (d.rev > 4) {
        d >> mOutputWidth;
        d >> mOutputHeight;
    }
END_LOADS

void StreamRecorder::Exit() { DeleteBuffers(); }

void StreamRecorder::SetDebugFrame(int i1) {
    int i7 = Min(mFramesRecorded - 1, mMaxFrames - 1);
    if (i1 <= i7) {
        i7 = Max(i1, -1);
    }
    mDebugFrame = i7;
}

void StreamRecorder::SetPhotoInput(RndDir *dir) {
    mInputDir = dir;
    if (mInputDir) {
        RndTexRenderer *r = mInputDir->Find<RndTexRenderer>("TexRenderer.rndtex", false);
        mTexRenderer = r;
    }
}

void StreamRecorder::StopRecordingImmediate() {
    if (mInputDir) {
        RndDrawable *r = mInputDir->Find<RndDrawable>("spotlights.grp", false);
        if (r)
            r->SetShowing(true);
    }
    mRecordingPos = -1.0f;
}

void StreamRecorder::StoppedRecordingScript() {
    static Symbol stream_recorder_stopped_recording("stream_recorder_stopped_recording");
    static DataArrayPtr p = new DataArray(1);
    p->Node(0) = stream_recorder_stopped_recording;
    p->Execute(false);
}

bool StreamRecorder::SetFrame(int index) {
    int i4 = Min(mFramesRecorded - 1, mMaxFrames - 1);
    if (index <= i4 && mOutputMat) {
        MILO_ASSERT(index < mBuffers.size(), 0x46);
        RndTex *cur = mBuffers[index];
        if (cur != mOutputMat->GetDiffuseTex()) {
            mOutputMat->SetDiffuseTex(cur);
        }
        return true;
    } else {
        return false;
    }
}

void StreamRecorder::DeleteBuffers() {
    for (int i = 0; i < mBuffers.size(); i++) {
        delete mBuffers[i];
    }
}

void StreamRecorder::CompressTextures() {
    while (!mCompressQueue.empty()) {
        int index = mCompressQueue.front();
        mCompressQueue.pop_front();
        MILO_ASSERT(index >= 0 && index < mBuffers.size(), 0x32);
        RndTex::AlphaCompress compress =
            mUseAlpha ? (RndTex::AlphaCompress)1 : (RndTex::AlphaCompress)0;
        TheRnd.CompressTexture(mBuffers[index], compress, this);
    }
}

void StreamRecorder::Reset() {
    DeleteBuffers();
    mBuffers.reserve(mMaxFrames);
    for (int i = 0; i < mMaxFrames; i++) {
        RndTex *tex = Hmx::Object::New<RndTex>();
        tex->SetMipMapK(666);
        mBuffers.push_back(tex);
    }
    StopRecordingImmediate();
    mStopTimer = 0;
    mFramesRecorded = 0;
    unkb4 = 0;
    mPlaybackPos = -1;
    mPausedPos = -1;
    mCompressQueue.clear();
}

DataNode StreamRecorder::OnReset(DataArray *d) {
    Reset();
    return 0;
}

DataNode StreamRecorder::OnStopRecording(DataArray *d) {
    mStopTimer = mStopDelay;
    return 1;
}

DataNode StreamRecorder::OnStopPlayback(DataArray *) {
    mPlaybackPos = -1.0f;
    return 1;
}

DataNode StreamRecorder::OnPausePlayback(DataArray *) {
    mPausedPos = mPlaybackPos;
    mPlaybackPos = -1.0f;
    return 0;
}

DataNode StreamRecorder::OnUnpausePlayback(DataArray *) {
    if (mPausedPos != -1.0f) {
        mPlaybackPos = mPausedPos;
        mPausedPos = -1.0f;
    }
    return 0;
}

DataNode StreamRecorder::OnPlayRecording(DataArray *) {
    if (mRecordingPos >= 0) {
        MILO_NOTIFY("Can't play back recording until recording has been finished.");
        return 0;

    } else {
        mPlaybackPos = 0;
        mCompressQueue.clear();
        return 1;
    }
}

DataNode StreamRecorder::OnStartRecording(DataArray *) {
    if (mBuffers.size() != mMaxFrames)
        Reset();
    mFramesRecorded = 0;
    unkb4 = 0;
    mRecordingPos = 0;
    mCompressQueue.clear();

    if (mInputDir && mTexRenderer) {
        RndTexRenderer *renderer = mTexRenderer;
        RndTex *rTex = mInputDir->Find<RndTex>("keep_me.tex", true);
        renderer->SetOutputTexture(rTex);
        RndDrawable *rDrawable = mInputDir->Find<RndDrawable>("spotlights.grp", false);
        if (rDrawable)
            rDrawable->SetShowing(false);
    }
    return 1;
}

void StreamRecorder::DrawShowing() {
    if (mRecordingPos >= 0.0f && mInputDir && mTexRenderer
        && mTexRenderer->GetOutputTexture()) {
        int compressIdx = (int)(mRecordingPos * 20.0f) - mStopDelay;
        if (compressIdx >= 0 && compressIdx >= unkb4) {
            if (compressIdx < mMaxFrames) {
                MILO_ASSERT(compressIdx < mBuffers.size(), 0xBC);
                RndTex *tex = mBuffers[compressIdx];
                tex->SetBitmap(
                    mOutputWidth, mOutputHeight, 16, RndTex::kRenderedNoZ, false, NULL
                );
                mTexRenderer->SetOutputTexture(tex);
                mInputDir->DrawShowing();
                mCompressQueue.push_back(compressIdx);
                unkb4++;
                if (mStopTimer <= 0)
                    return;
                mStopTimer--;
                if (mStopTimer != 0)
                    return;
            }
            StopRecordingImmediate();
            StoppedRecordingScript();
        }
    }
}

void StreamRecorder::Poll() {
#ifndef HX_NATIVE
    CompressTextures();
    if (mDebugFrame >= 0) {
        SetFrame(mDebugFrame);
    } else {
        if (mRecordingPos >= 0.0f) {
            if (mInputDir) {
                mInputDir->DrawShowing();
                mRecordingPos += TheTaskMgr.DeltaSeconds();
            }
        }
        if (mRecordingPos < 0.0f && mPlaybackPos >= 0.0f) {
            int speed = mPlaybackSpeed - 3;
            int frameIdx = (int)(mPlaybackPos * 20.0f);
            if (speed < 0) {
                frameIdx = frameIdx / (1 - speed);
            } else if (speed > 0) {
                frameIdx = (speed + 1) * frameIdx;
            }
            mPlaybackPos += TheTaskMgr.DeltaSeconds();
            if (!SetFrame(frameIdx)) {
                mPlaybackPos = -1.0f;
            }
        }
    }
#endif
}
