#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "obj/Msg.h"
#include "midi/DataEventList.h"
#include <vector>
#include <list>

class GemListInterface; // forward dec

// retail RB3-360: MidiParser : MsgSource : virtual Hmx::Object
// MSVC virtual-base layout: vfptr@0, MsgSource members, MidiParser members,
// Hmx::Object virtual base at tail (this+0xB0). Verified vs ctor fn_827C4EC8.
class MidiParser : public MsgSource {
public:
    struct PostProcess {
        PostProcess()
            : zeroLength(false), startOffset(0), endOffset(0), minLength(0),
              maxLength(kHugeFloat), minGap(-kHugeFloat), maxGap(kHugeFloat),
              useRealtimeGaps(false), variableBlendPct(0) {}
        bool zeroLength;
        float startOffset;
        float endOffset;
        float minLength;
        float maxLength;
        float minGap;
        float maxGap;
        bool useRealtimeGaps;
        float variableBlendPct;
    };

    struct Note {
        Note(int x, int y, int z) : note(z), startTick(x), endTick(y) {}
        int note; // 0x0
        int startTick; // 0x4
        int endTick; // 0x8
    };

    struct VocalEvent {
        // because midis can store text as either Text or Lyric types
        enum TextType {
            kText,
            kLyric
        };

        DataNode mTextContent; // 0x0
        int mTick; // 0x8

        TextType GetTextType() const {
            return mTextContent.Type() == kDataString ? kLyric : kText;
        }
    };

private:
    DataEventList *mEvents; // 0x18
    /** The midi track's name. */
    Symbol mTrackName; // 0x1c
    /** The typedef array to use when parsing gems. */
    DataArray *mGemParser; // 0x20
    /** The typedef array to use when parsing midi notes. */
    DataArray *mNoteParser; // 0x24
    /** The typedef array to use when parsing text. */
    DataArray *mTextParser; // 0x28
    /** The typedef array to use when parsing lyrics. */
    DataArray *mLyricParser; // 0x2c
    /** The typedef array to use when inserting idle events. */
    DataArray *mIdleParser; // 0x30
    /** The current parser in use. */
    DataArray *mCurParser; // 0x34
    /** The list of allowed midi notes for this track. */
    DataArray *mAllowedNotes; // 0x38
    /** The list of vocal events for this track. */
    std::vector<VocalEvent> *mVocalEvents; // 0x3c
    /** The list of midi notes for this track. */
    std::vector<Note> mNotes; // 0x40
    /** The list of gems for this track. */
    GemListInterface *mGems; // 0x4c
    bool mInverted; // 0x50
    PostProcess mProcess; // 0x54
    float mLastStart; // 0x78
    float mLastEnd; // 0x7c
    float mFirstEnd; // 0x80
    DataEvent *mEvent; // 0x84
    Symbol mMessageType; // 0x88
    bool mAppendLength; // 0x8c
    bool mUseVariableBlending; // 0x8d
    float mVariableBlendPct; // 0x90
    bool mMessageSelf; // 0x94
    bool mCompressed; // 0x95
    /** The index of the current gem being parsed. */
    int mGemIndex; // 0x98
    /** The index of the current note being parsed. */
    int mNoteIndex; // 0x9c
    /** The index of the current vocal event being parsed. */
    int mVocalIndex; // 0xa0
    float mStart; // 0xa4
    int mBefore; // 0xa8

    static DataNode *mpStart;
    static DataNode *mpEnd;
    static DataNode *mpLength;
    static DataNode *mpPrevStartDelta;
    static DataNode *mpPrevEndDelta;
    static DataNode *mpVal;
    static DataNode *mpSingleBit;
    static DataNode *mpLowestBit;
    static DataNode *mpLowestSlot;
    static DataNode *mpHighestSlot;
    static DataNode *mpData;
    static DataNode *mpOutOfBounds;
    static DataNode *mpBeforeDeltaSec;
    static DataNode *mpAfterDeltaSec;
    static std::list<MidiParser *> sParsers;

    /** Verify if the supplied note is in the list of allowed notes. */
    bool AllowedNote(int note);
    /** Get the index of the current item (gem, note, or vocal event) being parsed. */
    int GetIndex();
    /** Given an gem or note's index, get the corresponding start beat. */
    float GetStart(int idx);
    /** Given an gem or note's index, get the corresponding end beat. */
    float GetEnd(int idx);
    void FixGap(float *);
    void SetIndex(int idx);
    float ConvertToBeats(float f1, float f2);
    bool InsertIdle(float start, int before);
    void PushIdle(float start, float end, int at, Symbol idleMessage);
    void SetGlobalVars(int startTick, int endTick, const DataNode &data);
    void HandleEvent(int startTick, int endTick, const DataNode &data);
    void InsertDataEvent(float start, float end, const DataNode &ev);
    bool AddMessage(float start, float end, DataArray *msg, int firstArg);

    DataNode OnGetStart(DataArray *);
    DataNode OnGetEnd(DataArray *);
    DataNode OnNextStartDelta(DataArray *);
    DataNode OnDebugDraw(DataArray *);
    DataNode OnInsertIdle(DataArray *);
    DataNode OnBeatToSecLength(DataArray *);
    DataNode OnSecOffsetAll(DataArray *);
    DataNode OnSecOffset(DataArray *);
    DataNode OnPrevVal(DataArray *);
    DataNode OnNextVal(DataArray *);
    DataNode OnDelta(DataArray *);
    DataNode OnHasSpace(DataArray *);
    DataNode OnRtComputeSpace(DataArray *);

public:
    MidiParser();
    virtual ~MidiParser();
    OBJ_CLASSNAME(MidiParser);
    OBJ_SET_TYPE(MidiParser);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void SetTypeDef(DataArray *);

    DataEventList *Events() const { return mEvents; }
    Symbol TrackName() const { return mTrackName; }
    void Clear();
    void Reset(float frame);
    void Poll();
    void ParseNote(int startTick, int endTick, unsigned char data1);
    void PrintEvents();
    int ParseAll(class GemListInterface *gems, std::vector<VocalEvent> &text);

    static void Init();
    static void ClearManagedParsers();
    static std::list<MidiParser *> &Parsers() { return sParsers; }

    NEW_OBJ(MidiParser);
};
