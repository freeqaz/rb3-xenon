#include "DebugGraph.h"
#include "rndobj/Graph.h"
#include "utl/MakeString.h"
#ifdef HX_NATIVE
inline double __fsel(double a, double b, double c) { return a >= 0.0 ? b : c; }
#else
#include "xdk/LIBCMT/ppcintrinsics.h"
#endif

void DebugGraph::AddData(float data, bool b)  {
    Sample sample;
    sample.data = data;
    sample.b = b;
    mSamples.push_front(sample);

    if (mSamples.size() == mMaxSamples + 1) {
        mSamples.pop_back();
    }
}

void DebugGraph::Draw() {
    RndGraph *graph = RndGraph::GetOneFrame();
    Hmx::Rect rect(mRect.x, mRect.y, mRect.w, mRect.h);
    graph->AddRectFilled2D(rect, mColorB);

    if (mIsVisible) {
        Vector2 minPos(mRect.x, (mRect.y + mRect.h) - 0.02f);
        graph->AddScreenString(MakeString("%.3f", mMinValue), minPos, mColorA);
        Vector2 maxPos(mRect.x, mRect.y);
        graph->AddScreenString(MakeString("%.3f", mMaxValue), maxPos, mColorA);
    }

    float range = mMaxValue - mMinValue;
    if (mThresholdValue != FLT_MAX) {
        float normThresh = (mThresholdValue - mMinValue) / range;
        // Clamp to [0, 1] using __fsel to reduce register pressure
        float clamped = (float)__fsel(-normThresh, normThresh, 0.0f);
        float cx = (float)__fsel(clamped - 1.0f, 1.0f, clamped);
        float cy = cx;
        Vector2 lineStart(mRect.x, (1.0f - cx) * mRect.h + mRect.y);
        Vector2 lineEnd(mRect.x + mRect.w, (1.0f - cy) * mRect.h + mRect.y);
        Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
        graph->AddScreenLine(lineStart, lineEnd, white, false);

        Vector2 labelPos(mRect.x, 0.0f);
        labelPos.y = (1.0f - cx) * mRect.h + mRect.y;
        Hmx::Color white2(1.0f, 1.0f, 1.0f, 1.0f);
        auto thresholdStr = MakeString("%.3f", mThresholdValue);
        graph->AddScreenString(thresholdStr, labelPos, white2);
    }

    Hmx::Color white3(1.0f, 1.0f, 1.0f, 1.0f);
    Vector2 namePos(mRect.x + 0.1f, mRect.y);
    graph->AddScreenString(mGraphName.c_str(), namePos, white3);

    std::list<Sample>::iterator it = mSamples.begin();
    if (it != mSamples.end()) {
        int idx = 1;
        float normVal = (it->data - mMinValue) / range;
        float normIdx = 0.0f;
        // Clamp to [0, 1] using __fsel
        float clampedVal = (float)__fsel(-normVal, normVal, 0.0f);
        float clampedIdx = (float)__fsel(-normIdx, normIdx, 0.0f);
        float cv = (float)__fsel(clampedVal - 1.0f, 1.0f, clampedVal);
        float ci = (float)__fsel(clampedIdx - 1.0f, 1.0f, clampedIdx);
        Vector2 prevPt((1.0f - ci) * mRect.w + mRect.x, (1.0f - cv) * mRect.h + mRect.y);
        ++it;
        while (it != mSamples.end()) {
            float normVal2 = (it->data - mMinValue) / range;
            float normIdx2 = (float)idx / (float)(mMaxSamples - 1);
            // Clamp to [0, 1] using __fsel
            float clampedVal2 = (float)__fsel(-normVal2, normVal2, 0.0f);
            float clampedIdx2 = (float)__fsel(-normIdx2, normIdx2, 0.0f);
            float cv2 = (float)__fsel(clampedVal2 - 1.0f, 1.0f, clampedVal2);
            float ci2 = (float)__fsel(clampedIdx2 - 1.0f, 1.0f, clampedIdx2);
            Vector2 curPt((1.0f - ci2) * mRect.w + mRect.x, (1.0f - cv2) * mRect.h + mRect.y);
            if (it->b) {
                Vector2 topPt(curPt.x, mRect.y);
                Vector2 botPt(curPt.x, mRect.h + mRect.y);
                Hmx::Color white4(1.0f, 1.0f, 1.0f, 1.0f);
                graph->AddScreenLine(topPt, botPt, white4, false);
            }
            graph->AddScreenLine(curPt, prevPt, mColorA, false);
            prevPt = curPt;
            idx++;
            ++it;
        }
    }
}
