#include "char/CharClipDisplay.h"
#include "char/CharBones.h"
#include "char/CharIKFoot.h"
#include "math/Geo.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "rndobj/Rnd.h"
#include <cmath>

float CharClipDisplay::sZoom;
float CharClipDisplay::sEm;
ObjectDir *CharClipDisplay::sDir;

void CharClipDisplay::Init(ObjectDir *dir) {
    sDir = dir;
    sEm = TheRnd.DrawString("", Vector2(0, 0), Hmx::Color(1.0f, 0.0f, 0.0f), false).y;
}

void CharClipDisplay::SetClip(CharClip *clip, bool b) {
    mClip = clip;
    SetText(clip->Name());
    SetStartEnd(clip->StartBeat(), clip->EndBeat(), b);
}

void CharClipDisplay::SetText(const char *text) {
    strcpy(mClipNameBuffer, text);
    mTextWidth = TheRnd.DrawString(text, Vector2(0, 0), Hmx::Color(1.0f, 0.0f, 0.0f), false).x
        + sEm;
}

float CharClipDisplay::LineSpacing() { return sEm * 2.0f; }

float CharClipDisplay::GetX(float beat) const {
    float endBeat = mEndBeat;
    float startBeat = mStartBeat;
    float beatRange = (endBeat > startBeat) ? (endBeat - startBeat) : 1.0f;
    float leftMargin = sEm * 3.0f;
    float textWidth = mTextWidth + mPadding + leftMargin;
    return ((TheRnd.Width() - leftMargin) - textWidth) * ((beat - startBeat) / beatRange) + textWidth;
}

Hmx::Object *CharClipDisplay::FindSource(Hmx::Object *obj) {
    for (ObjDirItr<Hmx::Object> it(ObjectDir::Main(), false); it != nullptr; ++it) {
        MsgSinks *sinks = it->Sinks();
        if (sinks != nullptr && sinks->HasSink(obj)) {
            return it;
        }
    }
    return nullptr;
}

__declspec(noinline) void
CharClipDisplay::SetStartEnd(float start, float end, bool resetZoom) {
    mViewStartBeat = start;
    mViewEndBeat = end;
    mStartBeat = start;
    mEndBeat = end;
    float zoomRange = 16.0f / sZoom;
    if (resetZoom) {
        float margin = sEm * 3.0f;
        float screenWidth = (float)(long long)TheRnd.Width();
        float textOffset = mPadding + mTextWidth + margin;
        mStartBeat =
            mCursorBeat - ((screenWidth * 0.5f - textOffset) * zoomRange) / screenWidth;
        mEndBeat = (((screenWidth - margin) - textOffset) * zoomRange)
                / (float)(long long)TheRnd.Width()
            + mStartBeat;
    } else {
        if (end - start > zoomRange) {
            float cursor = mCursorBeat;
            float halfZoom = zoomRange * 0.5f;
            if (cursor < halfZoom + start) {
                mEndBeat = zoomRange + start;
            } else {
                if (cursor > end - halfZoom) {
                    mStartBeat = end - zoomRange;
                    return;
                }
                mStartBeat = cursor - halfZoom;
                mEndBeat = halfZoom + cursor;
            }
        } else {
            if (end != start) {
                return;
            }
            mStartBeat = start - zoomRange * 0.5f;
            mEndBeat = zoomRange * 0.5f + end;
        }
    }
}

void CharClipDisplay::DrawBeatString(char const *c, float f1, Hmx::Color const &color) {
    float posY = mDrawPosY - 4.0f;
    float posX = GetX(f1) - 18.0f;
    TheRnd.DrawString(c, Vector2(posY, posX), color, true);
}

void CharClipDisplay::DrawBlend(float beat, float weight) {
    Hmx::Rect rect(0.0f, mDrawPosY + 1.0f, 0.0f, 2.0f);
    float x1 = GetX(beat);
    rect.x = x1;
    float x2 = GetX(beat + weight);
    rect.w = x2 - x1;
    Hmx::Color blendColor(0.0f, 0.0f, 1.0f, 0.4f);
    TheRnd.DrawRect(rect, blendColor, nullptr, nullptr, nullptr);
    rect.h = 4.0f;
    rect.y = mDrawPosY - 1.0f;
    rect.w = 3.0f;
    float midX = GetX(weight * 0.5f + beat);
    rect.x = midX - 1.0f;
    Hmx::Color markerColor(0.0f, 0.0f, 1.0f, 1.0f);
    TheRnd.DrawRect(rect, markerColor, nullptr, nullptr, nullptr);
}

void CharClipDisplay::DrawBeatString(float beat, Hmx::Color const &color) {
    const char *text;
    if (beat == (float)std::floor(beat)) {
        text = MakeString("%d", (int)beat);
    } else {
        text = MakeString("%.2f", beat);
    }
    DrawBeatString(text, beat, color);
}

void CharClipDisplay::DrawTrack() {
    Hmx::Color white(1.0f, 1.0f, 1.0f, 1.0f);
    Hmx::Color green(0.0f, 1.0f, 0.0f, 1.0f);
    Hmx::Color black(0.0f, 0.0f, 0.0f, 1.0f);

    // Compute displayed start/end beats (use the min of mViewStartBeat/mStartBeat and mViewEndBeat/mEndBeat)
    float startBeat = (mStartBeat - mViewStartBeat >= 0.0f) ? mStartBeat : mViewStartBeat;
    float endBeat = (mEndBeat - mViewEndBeat >= 0.0f) ? mViewEndBeat : mEndBeat;

    float drawY = mDrawPosY;
    float halfEm = sEm * 0.5f;
    float nameY = -(halfEm - drawY);

    // Draw track background rect
    float startX = GetX(startBeat);
    float endX = GetX(endBeat);
    Hmx::Rect trackRect(startX, drawY, endX - startX, 3.0f);
    TheRnd.DrawRect(trackRect, white, nullptr, nullptr, nullptr);

    // Draw integer beat markers
    float firstBeat = (float)std::ceil(startBeat);
    float lastBeat = (float)std::floor(endBeat);
    if (firstBeat + 1.0f != firstBeat && firstBeat <= lastBeat) {
        float markerY = drawY - 3.0f;
        float markerH = 9.0f;
        float beat = firstBeat;
        do {
            Hmx::Rect markerRect(GetX(beat), markerY, 1.0f, markerH);
            TheRnd.DrawRect(markerRect, green, nullptr, nullptr, nullptr);
            beat += 1.0f;
        } while (beat <= lastBeat);
    }

    if (mClip == nullptr)
        goto drawName;

    // Draw beat events
    {
        bool firstEvent = true;
        int idx = 0;
        float eventLabelOffset = 10.0f;
        if (mClip->NumBeatEvents() != 0) {
            float eventAlpha = 0.2f;
            do {
                const CharClip::BeatEvent &ev = mClip->BeatEvents()[idx];
                float eventX = GetX(ev.beat);
                float halfEmVal = sEm * 0.5f;
                Hmx::Rect eventRect(eventX, drawY - halfEmVal, halfEmVal, 1.0f);
                Hmx::Color eventColor(eventAlpha, eventAlpha, 1.0f, 1.0f);
                TheRnd.DrawRect(eventRect, eventColor, nullptr, nullptr, nullptr);

                if (firstEvent
                    && (ev.beat > mCursorBeat
                        || (idx == 0
                            && mCursorBeat > mClip->BeatEvents().back().beat))) {
                    Hmx::Color eventLabelColor(eventAlpha, eventAlpha, 1.0f, 1.0f);
                    firstEvent = false;
                    float labelY = drawY - (halfEmVal + eventLabelOffset);
                    TheRnd.DrawString(
                        ev.event.Str(),
                        Vector2(eventX, labelY),
                        eventLabelColor,
                        true
                    );
                }
                idx += 1;
            } while ((unsigned int)idx < (unsigned int)mClip->NumBeatEvents());
        }
    }

    // Find IK feet
    {
        CharIKFoot *leftIk = sDir->Find<CharIKFoot>("left.ikfoot", false);
        CharIKFoot *rightIk = sDir->Find<CharIKFoot>("right.ikfoot", false);

        if (leftIk == nullptr) {
            if (rightIk == nullptr) {
                // No IK feet - draw sample markers
                Hmx::Rect sampleRect(0.0f, drawY + 1.0f, 1.0f, 1.0f);
                float frac;
                int startSample = mClip->BeatToSample(startBeat, &frac);
                int endSample = mClip->BeatToSample(endBeat, &frac);
                for (; startSample <= endSample; startSample++) {
                    float sampleBeat = mClip->SampleToBeat(startSample);
                    sampleRect.x = GetX(sampleBeat);
                    TheRnd.DrawRect(sampleRect, black, nullptr, nullptr, nullptr);
                }
            } else {
                RndTransformable *data = rightIk->GetData();
                goto drawIKData;
            }
        } else {
            MILO_ASSERT(
                !rightIk || !leftIk || (rightIk->GetData() == leftIk->GetData()), 0xd1
            );
            RndTransformable *data = leftIk->GetData();
        drawIKData:
            if (data != nullptr) {
                Symbol channelName
                    = CharBones::ChannelName(data->Name(), CharBones::TYPE_POS);
                void *channel = mClip->GetChannel(channelName);
                float channelData[48];
                mClip->EvaluateChannel(channelData, channel, mCursorBeat);
                Hmx::Color ikColor(1.0f, 1.0f, 0.0f, 1.0f);
                float cursorX = GetX(mCursorBeat);
                float posY = mDrawPosY;
                if (leftIk != nullptr) {
                    const char *leftText
                        = MakeString("L: %.1f", channelData[leftIk->GetDataIndex()]);
                    TheRnd.DrawString(
                        leftText,
                        Vector2(cursorX - 90.0f, posY + 10.0f),
                        ikColor,
                        true
                    );
                }
                if (rightIk != nullptr) {
                    const char *rightText
                        = MakeString("R: %.1f", channelData[rightIk->GetDataIndex()]);
                    TheRnd.DrawString(
                        rightText,
                        Vector2(cursorX - 40.0f, posY + 10.0f),
                        ikColor,
                        true
                    );
                }
            }
        }

        // Draw beat labels for first and last beats
        DrawBeatString(firstBeat, green);
        DrawBeatString(lastBeat, green);

        // Draw start beat label
        {
            float labelX = -((sEm * 2.0f) - (sEm * 3.0f + mPadding + mTextWidth));
            Vector2 startPos(labelX, nameY);
            TheRnd.DrawString(MakeString("%.1f", mViewStartBeat), startPos, white, true);
        }

        // Draw end beat label
        {
            float screenWidth = (float)TheRnd.Width();
            float labelX = -(sEm * 3.0f - screenWidth);
            Vector2 endPos(labelX, nameY);
            TheRnd.DrawString(MakeString("%.1f", mViewEndBeat), endPos, white, true);
        }
    }

drawName:
    // Draw clip name
    {
        Hmx::Color nameColor(1.0f, 1.0f, 1.0f, 1.0f);
        Vector2 namePos(mPadding + sEm, nameY);
        TheRnd.DrawString(mClipNameBuffer, namePos, nameColor, true);
    }
}

void CharClipDisplay::DrawCursor() {
    Hmx::Color yellow(1.0f, 1.0f, 0.0f, 1.0f);
    float x = GetX(mCursorBeat);
    Hmx::Rect rect(x, mDrawPosY - 3.0f, 1.0f, 9.0f);
    TheRnd.DrawRect(rect, yellow, nullptr, nullptr, nullptr);
    const char *text;
    if (!(mBlendWeight >= 1.0f)) {
        text = MakeString("%.1f", mCursorBeat);
    } else {
        text = MakeString("%.1f (%.2f)", mCursorBeat, mBlendWeight);
    }
    DrawBeatString(text, mCursorBeat, yellow);
}