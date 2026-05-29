#include "char/CharClip.h"
#include "CharClipGroup.h"
#include "char/CharBoneDir.h"
#include "char/CharBones.h"
#include "char/CharBonesMeshes.h"
#include "char/CharBonesSamples.h"
#include "math/Rot.h"
#include "math/Trig.h"
#include "obj/Data.h"
#include "obj/DataUtl.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "world/CameraShot.h"

const float CharClip::kBeatAccuracy = 0.02;
CharClip::FacingSet::FacingBones CharClip::FacingSet::sFacingPos;
CharClip::FacingSet::FacingBones CharClip::FacingSet::sFacingRotAndPos;

#pragma region Transitions

bool CharClip::Transitions::Replace(ObjRef *from, Hmx::Object *to) {
    NodeVector *vector = reinterpret_cast<NodeVector *>(from);
    vector->clip = (CharClip *)to;
    if (!vector->clip) {
        RemoveNodes(vector);
    }
    return true;
}

void CharClip::Transitions::Clear() {
    Resize(0, 0);
}

int CharClip::Transitions::Size() const {
    int size = 0;
    for (NodeVector *it = mNodeStart; it < mNodeEnd; it = it->Next()) {
        size++;
    }
    return size;
}

CharClip::NodeVector *CharClip::Transitions::Resize(int size, const NodeVector *old) {
    static int _x = MemFindHeap("char");
    MemHeapTracker temp(_x);
#ifdef HX_NATIVE
    intptr_t n = (intptr_t)old - (intptr_t)mNodeStart;
#else
    int n = (int)old - (int)mNodeStart;
#endif
    MILO_ASSERT((old == NULL) || (n >= 0), 0x9B);
    if (size != BytesInMemory()) {
        if (size == 0) {
            MemFree(mNodeStart);
            mNodeStart = nullptr;
            MILO_ASSERT(old == NULL, 0xA8);
        } else if (size < BytesInMemory()) {
            mNodeStart = (NodeVector *)MemTruncate(mNodeStart, size);
        } else {
            mNodeStart = (NodeVector *)MemRealloc(
                mNodeStart, size, __FILE__, 0xB0, "CharGraphNode", 0
            );
        }
    }
    // size is in bytes, not element count — use byte arithmetic
    mNodeEnd = (NodeVector *)((char *)mNodeStart + size);
    return (NodeVector *)((char *)mNodeStart + n);
}

CharClip::NodeVector *CharClip::Transitions::GetNodes(int idx) const {
    NodeVector *ret = mNodeStart;
    for (; idx > 0; idx--)
        ret = ret->Next();
    return ret;
}

CharClip::NodeVector *CharClip::Transitions::FindNodes(CharClip *clip) const {
    for (NodeVector *it = mNodeStart; it < mNodeEnd; it = it->Next()) {
        if (it->clip == clip)
            return it;
    }
    return nullptr;
}

void CharClip::Transitions::AddNode(CharClip *clip, const CharGraphNode &node) {
    NodeVector *nodes = FindNodes(clip);
    NodeVector *resized;
    if (nodes) {
        int bytes = BytesInMemory();
        NodeVector *next = nodes->Next();
        NodeVector *end = mNodeEnd;
        resized = Resize(bytes + 8, nodes);
        memmove(
            (char *)resized->Next() + 8,
            resized->Next(),
            (intptr_t)end - (intptr_t)next
        );
    } else {
        resized = Resize(BytesInMemory() + 0x10, mNodeEnd);
        resized->clip = clip;
        resized->size = 0;
    }
    int size = resized->size;
    int i = 0;
    if (size > 0) {
        for (; i < size; i++) {
            if (resized->nodes[i].curBeat > node.curBeat)
                break;
        }
    }
    if (i < size) {
        for (int j = size; j > i; j--) {
            resized->nodes[j] = resized->nodes[j - 1];
        }
    }
    resized->nodes[i] = node;
    resized->size++;
}

void CharClip::Transitions::RemoveClip(CharClip *clip) {
    NodeVector *node = FindNodes(clip);
    if (node)
        RemoveNodes(node);
}

void CharClip::Transitions::RemoveNodes(NodeVector *n) {
    MILO_ASSERT(n, 0xEC);
    NodeVector *next = n->Next();
    memmove(n, next, (intptr_t)mNodeEnd - (intptr_t)next);
    Resize(BytesInMemory() - ((intptr_t)next - (intptr_t)n), nullptr);
}

void CharClip::Transitions::Save(BinStream &bs) {
    int num_nodes = 0;
    int num_node_vectors = 0;
    for (NodeVector *it = mNodeStart; it < mNodeEnd; it = it->Next()) {
        num_node_vectors++;
        num_nodes += it->size;
    }
    bs << num_nodes;
    bs << num_node_vectors;
    for (NodeVector *it = mNodeStart; it < mNodeEnd; it = it->Next()) {
        bs << it->clip->Name();
        bs << it->size;
        for (int i = 0; i < it->size; i++) {
            bs << it->nodes[i].curBeat;
            bs << it->nodes[i].nextBeat;
        }
    }
}

void CharClip::Transitions::Load(BinStreamRev &d, int oldRev) {
    Clear();
    static ObjectDir *sDir;
    if (oldRev < 8) {
        int num;
        d >> num;
        if (num > 0 && mOwner->Dir() != sDir) {
            MILO_LOG(
                "NOTIFY: %s has old clip format, should resave\n", PathName(mOwner->Dir())
            );
            sDir = mOwner->Dir();
        }
        for (int i = 0; i < num; i++) {
            char buf[0x100];
            d.stream.ReadString(buf, 0x100);
            CharClip *clip = mOwner->Dir()->Find<CharClip>(buf, false);
            int num2;
            d >> num2;
            for (int j = 0; j < num2; j++) {
                CharGraphNode node;
                d >> node.curBeat;
                d >> node.nextBeat;
                if (clip) {
                    AddNode(clip, node);
                }
            }
        }
    } else {
        int temp, numNodes;
        d >> temp;
        d >> numNodes;
        if (d.rev < 0x14) {
            temp /= 8;
        }
#ifdef HX_NATIVE
        // On LP64, NodeVector is ~2x larger (8-byte pointers), need more space
        // temp from file is Xbox byte count; scale up generously
        int allocSize = temp < 256 ? 4096 : temp * 4;
        NodeVector *start = (NodeVector *)_MemAllocTemp(allocSize, __FILE__, 0x4CB, "CharGraphNode", 0);
        memset(start, 0, allocSize);
#else
        NodeVector *start =
            (NodeVector *)_MemAllocTemp(temp, __FILE__, 0x4CB, "CharGraphNode", 0);
#endif
        NodeVector *it = start;

        for (int i = 0; i < numNodes; i++) {
            char buf[0x100];
            d.stream.ReadString(buf, 0x100);
            CharClip *clip = mOwner->Dir()->Find<CharClip>(buf, false);
            if (clip) {
                it->clip = clip;
                d >> it->size;
                for (int j = 0; j < it->size; j++) {
                    d >> it->nodes[j].curBeat;
                    d >> it->nodes[j].nextBeat;
                }
                it = it->Next();
            } else {
                int count;
                d >> count;
                for (int j = 0; j < count; j++) {
                    int x, y;
                    d >> x;
                    d >> y;
                }
            }
        }
        Resize((intptr_t)it - (intptr_t)start, nullptr);
        memcpy(mNodeStart, start, BytesInMemory());
        MemFree(start);
    }
}

#pragma endregion
#pragma region FacingSet

void CharClip::FacingSet::Init() {
    sFacingPos.Set(false);
    sFacingRotAndPos.Set(true);
}

void CharClip::FacingSet::Set(CharBonesSamples &samples) {
    mFacingBones = nullptr;
    mFullRot = -1;
    mFullPos = samples.FindOffset("bone_facing.pos");
    if (mFullPos != -1) {
        mFullRot = samples.FindOffset("bone_facing.rotz");
        mFacingBones = mFullRot == -1 ? &sFacingPos : &sFacingRotAndPos;
    }
}

void CharClip::FacingSet::ListBones(std::list<CharBones::Bone> &bones) {
    if (mFacingBones) {
        mFacingBones->SetWeights(mWeight);
        mFacingBones->ListBones(bones);
    }
}

void CharClip::FacingSet::ScaleAddSample(
    CharBonesSamples &samples,
    CharBones &bones,
    float f1,
    int i1,
    float f2,
    int i2,
    float f3
) {
    if (mFacingBones) {
        Vector3 v;
        samples.EvaluateChannel(&v, mFullPos, i1, f2);
        samples.EvaluateChannel(&mFacingBones->mDeltaPos, mFullPos, i2, f3);
        Subtract(v, mFacingBones->mDeltaPos, mFacingBones->mDeltaPos);
        if (mFullRot != -1) {
            float f64, f68;
            samples.EvaluateChannel(&f64, mFullRot, i1, f2);
            samples.EvaluateChannel(&f68, mFullRot, i2, f3);
            mFacingBones->mDeltaAng = LimitAng(f64 - f68);
            RotateAboutZ(mFacingBones->mDeltaPos, -f68, mFacingBones->mDeltaPos);
        }
        mFacingBones->SetWeights(f1);
        mFacingBones->ScaleAdd(bones, f1);
    }
}

void CharClip::FacingSet::ScaleDown(CharBones &bones, float f) {
    if (mFacingBones)
        mFacingBones->ScaleDown(bones, f);
}

void CharClip::FacingSet::FacingBones::ReallocateInternal() {
    mStart = (char *)&mDeltaPos;
}

void CharClip::FacingSet::FacingBones::Set(bool b) {
    ClearBones();
    std::list<CharBones::Bone> bones;
    bones.push_back(CharBones::Bone("bone_facing_delta.pos", 1));
    if (b) {
        bones.push_back(CharBones::Bone("bone_facing_delta.rotz", 1));
    }
    AddBones(bones);
}

#pragma endregion
#pragma region BeatEvent

void CharClip::BeatEvent::Save(BinStream &bs) {
    bs << event;
    bs << beat;
}

void CharClip::BeatEvent::Load(BinStream &bs) {
    bs >> event;
    bs >> beat;
}

#pragma endregion
#pragma region CharClip

CharClip::CharClip()
    : mTransitions(this), mFramesPerSec(30), mFlags(0), mPlayFlags(0), mRange(0),
      mRelative(nullptr), mDirty(true), mOldVer(-1), mDoNotCompress(false), mSyncAnim(this),
      unk198(0) {
    mBeatTrack.resize(1);
    mBeatTrack[0].frame = 0;
    mBeatTrack[0].value = 0;
}

CharClip::~CharClip() {}

BEGIN_HANDLERS(CharClip)
    HANDLE_EXPR(in_groups, InGroups())
    HANDLE(groups, OnGroups)
    HANDLE_EXPR(shares_groups, SharesGroups(_msg->Obj<CharClip>(2)))
    HANDLE(has_group, OnHasGroup)
    HANDLE_EXPR(get_clip_events, GetClipEvents())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(CharGraphNode)
    SYNC_PROP(cur_beat, o.curBeat)
    SYNC_PROP(next_beat, o.nextBeat)
END_CUSTOM_PROPSYNC

bool PropSyncArray(
    CharClip::NodeVector &o, DataNode &val, DataArray *prop, int i, PropOp op
) {
    if (i == prop->Size()) {
        MILO_ASSERT(op == kPropSize, 0x720);
        val = o.size;
        return true;
    } else {
        CharGraphNode &node = o.nodes[prop->Int(i++)];
        if (i < prop->Size() || op & kPropGet) {
            return PropSync(node, val, prop, i, op);
        } else
            return false;
    }
}

BEGIN_CUSTOM_PROPSYNC(CharClip::NodeVector)
    SYNC_PROP_SET(clip, o.clip, ) {
        static Symbol _s("nodes");
        if (sym == _s) {
            PropSyncArray(o, _val, _prop, _i + 1, _op);
            return true;
        }
    }
END_CUSTOM_PROPSYNC

bool PropSync(
    CharClip ::Transitions &o, DataNode &_val, DataArray *_prop, int _i, PropOp _op
) {
    if (_i == _prop->Size()) {
        MILO_ASSERT(_op == kPropSize, 0x73B);
        _val = o.Size();
        return true;
    } else {
        CharClip::NodeVector &vec = *o.GetNodes(_prop->Int(_i++));
        if (_i < _prop->Size() || _op & (kPropSize | kPropGet)) {
            return PropSync(vec, _val, _prop, _i, _op);
        } else
            return false;
    }
}

BEGIN_CUSTOM_PROPSYNC(CharClip::BeatEvent)
    SYNC_PROP_SET(beat, o.beat, o.beat = _val.Float())
    SYNC_PROP_SET(event, o.event, o.event = _val.Sym())
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(CharClip)
    SYNC_PROP_SET(start_beat, StartBeat(), )
    SYNC_PROP_SET(end_beat, EndBeat(), )
    SYNC_PROP_SET(length_beats, LengthBeats(), )
    SYNC_PROP_SET(frames_per_sec, mFramesPerSec, )
    SYNC_PROP_SET(length_seconds, LengthSeconds(), )
    SYNC_PROP_SET(average_beats_per_sec, AverageBeatsPerSecond(), )
    SYNC_PROP_SET(flags, mFlags, SetFlags(_val.Int()))
    SYNC_PROP_SET(default_blend, mPlayFlags & 0xF, SetDefaultBlend(_val.Int()))
    SYNC_PROP_SET(default_loop, mPlayFlags & 0xF0, SetDefaultLoop(_val.Int()))
    SYNC_PROP_SET(beat_align, mPlayFlags & 0xF600, SetBeatAlignMode(_val.Int()))
    SYNC_PROP(range, mRange)
    SYNC_PROP_SET(relative, mRelative, SetRelative(_val.Obj<CharClip>()))
    SYNC_PROP_MODIFY(events, mBeatEvents, SortEvents())
    SYNC_PROP_SET(dirty, mDirty, )
    SYNC_PROP_SET(size, AllocSize(), )
    SYNC_PROP(do_not_compress, mDoNotCompress)
    SYNC_PROP(transitions, mTransitions)
    SYNC_MEMBER(full, mFull)
    SYNC_MEMBER(one, mOne)
    SYNC_PROP_SET(compression, mFull.GetCompression(), )
    SYNC_PROP_SET(num_frames, NumFrames(), )
    SYNC_PROP(sync_anim, mSyncAnim)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(CharClip)
    SAVE_REVS(22, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    bs << mFramesPerSec;
    bs << mFlags;
    bs << mPlayFlags;
    bs << mRange;
    bs << (mRelative ? mRelative->Name() : "");
    bs << mOldVer;
    bs << mDoNotCompress;
    mTransitions.Save(bs);
    bs << mBeatEvents.size();
    for (int i = 0; i < mBeatEvents.size(); i++) {
        mBeatEvents[i].Save(bs);
    }
    mFull.Save(bs);
    mOne.Save(bs);
    bs << mZeros;
    bs << mBeatTrack;
    bs << mSyncAnim;
    bs << mBlendSamples;
    bs << unk198;
END_SAVES

BEGIN_COPYS(CharClip)
    static int _x = MemFindHeap("char");
    MemHeapTracker tmp(_x);
    COPY_SUPERCLASS(Hmx::Object)
    CREATE_COPY(CharClip)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mFramesPerSec)
        COPY_MEMBER(mBeatTrack)
        if (ty != kCopyFromMax) {
            COPY_MEMBER(mFlags)
            COPY_MEMBER(mPlayFlags)
            COPY_MEMBER(mRange)
            COPY_MEMBER(mRelative)
            mBeatEvents.resize(c->mBeatEvents.size());
            for (int i = 0; i < mBeatEvents.size(); i++) {
                mBeatEvents[i] = c->mBeatEvents[i];
            }
            COPY_MEMBER(mDoNotCompress)
            COPY_MEMBER(mSyncAnim)
        }
        mFull.Clone(c->mFull);
        mOne.Clone(c->mOne);
        COPY_MEMBER(mZeros)
        mFacing.Set(mFull);
        mDirty = true;
        COPY_MEMBER(mBlendSamples)
        COPY_MEMBER(unk198)
    END_COPYING_MEMBERS
END_COPYS

INIT_REVS(0x16, 0)

BEGIN_LOADS(CharClip)
    static int _x = MemFindHeap("char");
    MemHeapTracker temp(_x);
    int oldRev, x, y, oldVer, tv;
    LOAD_REVS(bs)
    ASSERT_REVS(0x16, 0)
    oldRev = 0;
    if (d.rev < 0x10)
        d >> oldRev;
    else
        oldRev = 0xD;
    MILO_ASSERT(oldRev > 1, 0x531);
    LOAD_SUPERCLASS(Hmx::Object)
    if (d.rev < 0x12) {
        d >> x;
        d >> y;
    }
    d >> mFramesPerSec;
    d >> mFlags;
    d >> mPlayFlags;
    if (oldRev < 0xD) {
        int x;
        d >> x;
    }
    if (oldRev > 3) {
        d >> mRange;
    }
    if (oldRev > 5) {
        char _relBuf[128];
        d.stream.ReadString(_relBuf, 128);
        mRelative = Dir() ? Dir()->Find<CharClip>(_relBuf, false) : nullptr;
    } else if (oldRev > 4) {
        bool isRelativeToSelf;
        d >> isRelativeToSelf;
        mRelative = isRelativeToSelf ? this : nullptr;
    } else {
        mRelative = nullptr;
    }
    if (oldRev > 8 && oldRev < 0xB) {
        bool unused;
        d >> unused;
    }
    if (oldRev > 9) {
        d >> mOldVer;
    }
    if (oldRev > 0xB) {
        d >> mDoNotCompress;
    }
    mTransitions.Load(d, oldRev);
    if (oldRev < 3) {
        int count;
        d >> count;
        String str;
        for (int i = 0; i < count; i++) {
            d >> str;
        }
    }
    if (oldRev > 6) {
        int count;
        d >> count;
        mBeatEvents.resize(count);
        for (int i = 0; i < mBeatEvents.size(); i++) {
            mBeatEvents[i].Load(d.stream);
        }
    } else {
        String eventName;
        d >> eventName;
        if (!eventName.empty()) {
            MILO_NOTIFY("%s has old enter event %s, must port", PathName(this), eventName);
        }
        d >> eventName;
        if (!eventName.empty()) {
            MILO_NOTIFY("%s has old exit event %s, must port", PathName(this), eventName);
        }
        int count;
        float lastFrame = -kHugeFloat;
        d >> count;
        for (int i = 0; i < count; i++) {
            float frameNum;
            d >> frameNum;
            d >> eventName;
            if (!eventName.empty()) {
                MILO_NOTIFY(
                    "%s has old frame %.2f event %s, must port", PathName(this), frameNum, eventName
                );
            }
            if (frameNum < lastFrame) {
                MILO_NOTIFY("Keyframes in %s are out of order.", (char *)Name());
            }
            lastFrame = frameNum;
        }
    }
    mDirty = false;
    tv = TransitionVersion();
    if (tv != mOldVer) {
        // The assert `MILO_ASSERT(tv < 0x7FFF, 0x5A3)` was removed to match retail behavior
        mOldVer = tv;
        mDirty = true;
    }
    if (d.rev > 0xC) {
        mFull.Load(d.stream);
        mOne.Load(d.stream);
    } else {
        mFull.LoadHeader(d);
        mOne.LoadHeader(d);
        if (d.rev > 7) {
            CharBonesSamples samples;
            samples.LoadHeader(d);
        }
        mFull.LoadData(d);
        mOne.LoadData(d);
    }
    if (d.rev > 0xE) {
        d >> mZeros;
    }
    mFacing.Set(mFull);
    if (d.rev > 0x11) {
        d >> mBeatTrack;
    } else {
        if (NumFrames() > 1) {
            mBeatTrack.resize(2);
            Key<float> &key0 = mBeatTrack[0];
            key0 = Key<float>(key0.value, 0);
            Key<float> &key1 = mBeatTrack[1];
            key1 = Key<float>(key1.value, NumFrames() - 1);
        } else {
            mBeatTrack.resize(1);
            Key<float> &key0 = mBeatTrack[0];
            key0 = Key<float>(key0.value, 0);
        }
        if (d.rev < 0x11) {
            float oldFPS = mFramesPerSec;
            if (LengthBeats() > 0) {
                mFramesPerSec = (NumFrames() - 1) * (oldFPS / LengthBeats());
            } else {
                mFramesPerSec = 30.0f;
            }
        }
    }
    if (d.rev > 0x12) {
        d >> mSyncAnim;
    }
    if (EndBeat() == StartBeat() && mFull.NumSamples() > 1) {
        MILO_NOTIFY(
            "%s has endframe == startframe == %.3f but %d samples!\n",
            Name(),
            StartBeat(),
            mFull.NumSamples()
        );
    }
    if (d.rev > 0x14) {
        d >> mBlendSamples;
    }
    if (d.rev > 0x15) {
        d >> unk198;
    }
END_LOADS

void CharClip::PreSave(BinStream &) {
    MILO_NOTIFY("You can only save a CharClip from PC");
}

void CharClip::Print() {
    TheDebug << "CharClip: " << Name() << "\n";
    TheDebug << MakeString("total allocation size %d\n", AllocSize());
    TheDebug << "Full:\n";
    mFull.Print();
    TheDebug << "One:\n";
    mOne.Print();
}

void CharClip::SetTypeDef(DataArray *def) {
    Hmx::Object::SetTypeDef(def);
    mDirty = true;
}

void CharClip::Init() {
    FacingSet::Init();
    REGISTER_OBJ_FACTORY(CharClip);
}

int CharClip::AllocSize() {
    int size = mTransitions.BytesInMemory();
    size += mFull.AllocateSize() + mOne.AllocateSize();
    size += sizeof(CharClip);
    return size;
}

void CharClip::SetPlayFlags(int i) {
    if (i != mPlayFlags) {
        mPlayFlags = i;
        mDirty = true;
    }
}

void CharClip::SetFlags(int i) {
    if (i != mFlags) {
        mFlags = i;
        mDirty = true;
    }
}

void CharClip::SetDefaultBlend(int blend) {
    int flags = mPlayFlags;
    SetDefaultBlendFlag(flags, blend);
    SetPlayFlags(flags);
}

void CharClip::SetDefaultLoop(int loop) {
    int flags = mPlayFlags;
    SetDefaultLoopFlag(flags, loop);
    SetPlayFlags(flags);
}

void CharClip::SetBeatAlignMode(int align) {
    int flags = mPlayFlags;
    SetDefaultBeatAlignModeFlag(flags, align);
    SetPlayFlags(flags);
}

struct SortByFrame {
    bool operator()(const CharClip::BeatEvent &e1, const CharClip::BeatEvent &e2) const {
        return e1.beat < e2.beat;
    }
};

void CharClip::SortEvents() {
    std::sort(mBeatEvents.begin(), mBeatEvents.end(), SortByFrame());
}

void *CharClip::GetChannel(Symbol s) {
    int off = mFull.FindOffset(s);
    if (off > -1) {
        return (void *)(off + 1);
    } else {
        off = mOne.FindOffset(s);
        if (off > -1)
            return (void *)(off + mFull.TotalSize() + 1);
        else
            return 0;
    }
}

void CharClip::ScaleDown(CharBones &bones, float f) {
    mFull.ScaleDown(bones, f);
    mOne.ScaleDown(bones, f);
    mFacing.ScaleDown(bones, f);
}

int CharClip::GetContext() const {
    if (TypeDef()) {
        DataArray *found = TypeDef()->FindArray("resource", false);
        if (found) {
            return DataGetMacro(found->Str(2))->Int(0);
        }
    }
    return 0;
}

const CharGraphNode *CharClip::FindFirstNode(CharClip *clip, float beat) const {
    NodeVector *nodes = mTransitions.FindNodes(clip);
    if (nodes) {
        for (int i = 0; i < nodes->size; i++) {
            if (nodes->nodes[i].curBeat >= beat)
                return &nodes->nodes[i];
        }
    }
    return nullptr;
}

const CharGraphNode *CharClip::FindLastNode(CharClip *clip, float beat) const {
    NodeVector *nodes = mTransitions.FindNodes(clip);
    if (nodes) {
        for (int i = nodes->size - 1; i >= 0; i--) {
            if (nodes->nodes[i].curBeat >= beat)
                return &nodes->nodes[i];
        }
    }
    return nullptr;
}

void CharClip::EvaluateChannel(void *dest, const void *channel, int frame, float blend) {
    if (!channel) {
        MILO_FAIL("%s passed in NULL for evaluate channel", (char *)PathName(this));
    }
    int offset = (intptr_t)channel - 1;
    if (offset < mFull.TotalSize()) {
        mFull.EvaluateChannel(dest, offset, frame, blend);
    } else {
        int oneOffset = offset - mFull.TotalSize();
        if (oneOffset < mOne.TotalSize()) {
            mOne.EvaluateChannel(dest, oneOffset, 0, 0);
        } else {
            MILO_FAIL("%s could not find offset %d %d", (char *)offset, oneOffset, PathName(this));
        }
    }
}

void CharClip::ScaleAddSample(
    CharBones &bones, float f1, int i1, float f2, int i2, float f3
) {
    mFull.ScaleAddSample(bones, f1, i1, f2);
    mOne.ScaleAddSample(bones, f1, 0, 0);
    mFacing.ScaleAddSample(mFull, bones, f1, i1, f2, i2, f3);
}

void CharClip::Relativize() {
    if (mFull.GetCompression() != CharBones::kCompressNone) {
        MILO_NOTIFY("%s relativizing compressed clip, should reexport", PathName(this));
    }
    MILO_ASSERT(mRelative, 0x3A3);
    mFull.Relativize(mRelative);
    mOne.Relativize(mRelative);
}

int CharClip::TransitionVersion() {
    int version = -1;
    if (!Type().Null()) {
        const DataNode *prop = Property("transition_version", false);
        if (prop)
            version = prop->Int();
    }
    return version;
}

const CharGraphNode *
CharClip::FindNode(CharClip *clip, float f1, int iii, float f2) const {
    unsigned int blendMode = iii & 0xFu;
    const CharGraphNode *n = nullptr;

    if (blendMode >= kPlayNoBlend) {
        if (blendMode != kPlayNoBlend) {
            if (blendMode >= kPlayLast) {
                if (blendMode != kPlayLast) {
                    if (blendMode != kPlayDirty) {
                        MILO_NOTIFY(
                            "Unknown mode flags %x, default to kPlayNow",
                            (const CamShotFrame::BlendEaseMode &)iii
                        );
                    }
                } else {
                    n = FindLastNode(clip, f1);
                }
            } else {
                n = FindFirstNode(clip, f1);
            }
        }
    }

    if (!n) {
        float halfBlend = f2 * 0.5f;
        static CharGraphNode node;
        static int sFlag;
        if (!(sFlag & 1)) {
            sFlag |= 1;
        }
        node.curBeat = f1;
        if ((int)blendMode == 4) {
            MaxEq(node.curBeat, EndBeat() - halfBlend);
        }
        n = &node;
        node.nextBeat = clip->StartBeat();
    }
    return n;
}

float CharClip::LengthSeconds() const {
    if (NumFrames() < 2)
        return 0;
    else
        return (NumFrames() - 1) / mFramesPerSec;
}

float CharClip::AverageBeatsPerSecond() const {
    if (LengthSeconds()) {
        return LengthBeats() / LengthSeconds();
    } else
        return 1;
}

float CharClip::FrameToBeat(float frame) const {
    float ret = 0;
    mBeatTrack.Linear(frame, ret);
    return ret;
}

float CharClip::BeatToFrame(float beat) const {
    float ret = 0;
    mBeatTrack.ReverseLinear(beat, ret);
    return ret;
}

float CharClip::DeltaSecondsToDeltaBeat(float f1, float beat) {
    if (mBeatTrack.size() == 1)
        return f1;
    float beatFrame = BeatToFrame(beat);
    float ret = FrameToBeat(f1 * mFramesPerSec + beatFrame);
    return ret - beat;
}

int CharClip::BeatToSample(float f, float *fp) const {
    float frame = BeatToFrame(f);
    float f1 = 0;
    if (mBeatTrack.back().frame != 0) {
        f1 = frame / mBeatTrack.back().frame;
    } else {
        f1 = 0;
    }
    *fp = f1;
    return mFull.FracToSample(fp);
}

void CharClip::EvaluateChannel(void *v1, const void *v2, float f3) {
    float fp;
    int sample = BeatToSample(f3, &fp);
    EvaluateChannel(v1, v2, sample, fp);
}

void CharClip::RotateBy(CharBones &bones, float f) {
    float frac;
    int samp = BeatToSample(f, &frac);
    MILO_ASSERT(frac == 0, 0x36E);
    mFull.RotateBy(bones, samp);
    mOne.RotateBy(bones, 0);
}

void CharClip::RotateTo(CharBones &bones, float f1, float f2) {
    float fp;
    int sample = BeatToSample(f2, &fp);
    mFull.RotateTo(bones, f1, sample, fp);
    mOne.RotateTo(bones, f1, 0, 0);
}

void CharClip::ScaleAdd(CharBones &bones, float f1, float f2, float f3) {
    float fp;
    float fp2;
    int samp1 = BeatToSample(f2, &fp);
    int samp2 = BeatToSample(f2 - f3, &fp2);
    ScaleAddSample(bones, f1, samp1, fp, samp2, fp2);
}

void CharClip::SetRelative(CharClip *clip) {
    if (clip != mRelative) {
        if (clip == this) {
            MILO_NOTIFY("%s cannot be relative to itself", PathName(this));
        } else {
            mRelative = clip;
            if (mRelative)
                Relativize();
            else
                MILO_NOTIFY("%s cannot de-relativize clip, must reexport", PathName(this));
        }
    }
}

void CharClip::ListBones(std::list<CharBones::Bone> &bones) {
    mFull.ListBones(bones);
    mOne.ListBones(bones);
    mFacing.ListBones(bones);
    for (int i = 0; i < mZeros.size(); i++) {
        bones.push_back(mZeros[i]);
    }
}

void CharClip::StuffBones(CharBones &bones) {
    std::list<CharBones::Bone> blist;
    ListBones(blist);
    bones.AddBones(blist);
}

void CharClip::PoseMeshes(ObjectDir *dir, float f) {
    CharBonesMeshes meshes;
    meshes.SetName("tmp_viseme_bones", dir);
    StuffBones(meshes);
    ScaleDown(meshes, 0.0f);
    ScaleAdd(meshes, 1.0f, f, 0.0f);
    meshes.PoseMeshes();
}

DataNode CharClip::GetClipEvents() {
    static Symbol events("events");
    static DataArray *cfg = SystemConfig("objects", "CharClip");
    DataNode ret = 0;
    DataArray *clipArr = cfg->FindArray(events, false);
    if (clipArr) {
        ret = clipArr->Array(1);
    } else {
        DataArray *arr = new DataArray(1);
        arr->Node(0) = Symbol();
        ret = arr;
    }
    return ret;
}

void CharClip::ApplyBlendedSkeletons(
    CharClip **clips, CharBones &bones, float f1, float f2
) {
    float f60;
    int sample = BeatToSample(f1, &f60);
    float zero = 0.0f;
    FOREACH (it, mBlendSamples[sample]) {
        clips[it->first]->ScaleAdd(bones, (1.0f - f60) * it->second * f2, zero, zero);
    }
    if (zero < f60) {
        int nextSample = sample + 1;
        FOREACH (it, mBlendSamples[nextSample]) {
            clips[it->first]->ScaleAdd(bones, f60 * it->second * f2, zero, zero);
        }
    }
}

bool CharClip::SharesGroups(CharClip *clip) {
    FOREACH (it, mRefs) {
        Hmx::Object *owner = it->RefOwner();
        CharClipGroup *group = dynamic_cast<CharClipGroup *>(owner);
        if (group && group->HasClip(clip))
            return true;
    }
    return false;
}

int CharClip::InGroups() {
    int num = 0;
    FOREACH (it, mRefs) {
        Hmx::Object *owner = it->RefOwner();
        CharClipGroup *group = dynamic_cast<CharClipGroup *>(owner);
        if (group)
            num++;
    }
    return num;
}

DataNode CharClip::OnGroups(DataArray *) {
    DataArray *groups = new DataArray(0);
    FOREACH (it, mRefs) {
        Hmx::Object *owner = it->RefOwner();
        CharClipGroup *group = dynamic_cast<CharClipGroup *>(owner);
        if (group) {
            groups->Insert(groups->Size(), group);
        }
    }
    DataNode ret(groups);
    groups->Release();
    return ret;
}

DataNode CharClip::OnHasGroup(DataArray *arr) {
    const char *str = arr->Str(2);
    FOREACH (it, mRefs) {
        Hmx::Object *owner = it->RefOwner();
        CharClipGroup *group = dynamic_cast<CharClipGroup *>(owner);
        if (group && streq(group->Name(), str))
            return 1;
    }
    return 0;
}

CharBoneDir *CharClip::GetResource() const {
    CharBoneDir *dir = 0;
    if (TypeDef()) {
        DataArray *found = TypeDef()->FindArray("resource", false);
        if (found)
            dir = CharBoneDir::FindBoneDirResource(found->Str(1));
    }
    if (!dir) {
        MILO_NOTIFY("%s has no resource", PathName(this));
    }
    return dir;
}

float CharClip::SampleToBeat(int sample) const {
    if (!mFull.NumFrames()) {
        return FrameToBeat((float)sample);
    } else {
        const std::vector<float> &frames = mFull.Frames();
        const float *lower = std::lower_bound(frames.begin(), frames.end(), (float)sample);
        return FrameToBeat(lower - frames.begin());
    }
}

void CharClip::LockAndDelete(CharClip **const clips, int remaining, int maxToDelete) {
    int loopIdx;
    CharClip *clip;

    MILO_ASSERT(remaining >= 0, 0x42A);

    // Cap maxToDelete at remaining
    if (remaining < maxToDelete) {
        maxToDelete = remaining;
    }

    loopIdx = 0;

    // Phase 1: Partition clips with flag 0x10000 set
    if (remaining > 0) {
        CharClip **readPtr = &clips[0];
        CharClip **writeBackPtr = &clips[remaining];
        do {
            readPtr++;
            clip = readPtr[-1];
            if ((clip->mPlayFlags & 0x10000) != 0) {
                writeBackPtr--;
                remaining--;
                maxToDelete--;
                loopIdx--;
                readPtr[-1] = *writeBackPtr;
                *writeBackPtr = clip;
                readPtr--;
            }
            loopIdx++;
        } while (loopIdx < remaining);
    }

    // Phase 2: Mark additional clips with deletion flag
    if (maxToDelete > 0) {
        int cnt = maxToDelete;
        CharClip **markPtr = &clips[remaining];
        remaining -= maxToDelete;
        do {
            markPtr--;
            (*markPtr)->mPlayFlags |= 0x10000;
            cnt--;
        } while (cnt != 0);
    }

    // Phase 3: Release all marked clips
    if (remaining > 0) {
        CharClip **releasePtr = &clips[remaining];
        do {
            releasePtr--;
            CharClip *clip = *releasePtr;
            remaining--;
            if ((unsigned int)clip) {
                clip->Release((ObjRef *)1);
            }
        } while (remaining > 0);
    }
}
