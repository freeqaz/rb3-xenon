// Retail TU 0x822CF888-0x822D2BA8.
#define RB3_OBJPTR_INLINE_OWNER_CTOR 1
#define RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT 1
#include "bandobj/CharKeyHandMidi.h"
#include "char/CharIKFingers.h"
#include "char/Character.h"
#include "math/Vec.h"
#include "obj/Object.h"
#include "obj/Task.h"
#include "os/Debug.h"
#include "rndobj/Trans.h"
#include "rndobj/Utl.h"
#include "utl/BinStream.h"
#include <algorithm>

CharKeyHandMidi::CharKeyHandMidi()
    : mIKObject(this), mFirstSpot(this), mSecondSpot(this), unk64(0), unk68(5), unk74(5),
      unk78(true), unk7c(this), unk88(0.0f), mIsRightHand(true) {
    unk6c.resize(5);
    for (int i = 0; i < 5; i++) {
        unk6c[i] = 0;
    }
    unk4c.resize(26);
    unk54.resize(26);
    Enter();
}

CharKeyHandMidi::~CharKeyHandMidi() {}

void CharKeyHandMidi::Enter() {
    for (int i = 0; i < 5; i++) {
        if (unk6c[i] != 0) {
            UnkeyFinger((CharIKFingers::FingerNum)i);
        }
    }
    unk5c.clear();
    RndPollable::Enter();
}

void CharKeyHandMidi::SetName(const char *name, ObjectDir *dir) {
    Hmx::Object::SetName(name, dir);
    unk7c = dynamic_cast<Character *>(dir);
}

void CharKeyHandMidi::RunTest() {
    unk78 = true;
    if (mIKObject) {
        mIKObject->SetFinger(unk4c[25], unk54[25], CharIKFingers::kFingerPinky);
        mIKObject->SetFinger(unk4c[13], unk54[13], CharIKFingers::kFingerThumb);
    }
}

CharIKFingers::FingerNum CharKeyHandMidi::FindPreferredFinger(
    KeyboardKey setToKey, KeyboardKey lastKeyDown, CharIKFingers::FingerNum lastFingerDown
) {
    int finger;
    if ((int)setToKey == lastKeyDown)
        return lastFingerDown;
    if (lastKeyDown == 0 || lastFingerDown == CharIKFingers::kNumFingers)
        return CharIKFingers::kFingerMiddle;
    int distance = abs(setToKey - lastKeyDown);
    if (setToKey > lastKeyDown) {
        if (mIsRightHand) {
            if (lastFingerDown == CharIKFingers::kFingerPinky)
                return CharIKFingers::kFingerPinky;
            if (distance <= 2)
                finger = lastFingerDown + 1;
            else if (distance <= 5)
                finger = lastFingerDown + 2;
            else if (distance <= 7)
                finger = lastFingerDown + 3;
            else
                return CharIKFingers::kFingerPinky;
            if (finger > 4)
                finger = CharIKFingers::kFingerPinky;
            return (CharIKFingers::FingerNum)finger;
        } else {
            if (lastFingerDown == CharIKFingers::kFingerThumb)
                return CharIKFingers::kFingerThumb;
            if (distance <= 2)
                finger = lastFingerDown - 1;
            else if (distance <= 5)
                finger = lastFingerDown - 2;
            else if (distance <= 7)
                finger = lastFingerDown - 3;
            else
                return CharIKFingers::kFingerThumb;
            // retail branches here (cmpwi/bgelr to the shared `return thumb`);
            // every spelling tried (assign, early return, enum local) compiles to
            // MSVC's branchless max(x, 0) instead -- 92.4, shape unknown
            if (finger < 0)
                finger = CharIKFingers::kFingerThumb;
            return (CharIKFingers::FingerNum)finger;
        }
    } else {
        if (mIsRightHand) {
            if (lastFingerDown == CharIKFingers::kFingerThumb)
                return CharIKFingers::kFingerThumb;
            if (distance <= 2)
                finger = lastFingerDown - 1;
            else if (distance <= 5)
                finger = lastFingerDown - 2;
            else if (distance <= 7)
                finger = lastFingerDown - 3;
            else
                return CharIKFingers::kFingerThumb;
            // retail branches here (cmpwi/bgelr to the shared `return thumb`);
            // every spelling tried (assign, early return, enum local) compiles to
            // MSVC's branchless max(x, 0) instead -- 92.4, shape unknown
            if (finger < 0)
                finger = CharIKFingers::kFingerThumb;
            return (CharIKFingers::FingerNum)finger;
        } else {
            if (lastFingerDown == CharIKFingers::kFingerPinky)
                return CharIKFingers::kFingerPinky;
            if (distance <= 2)
                finger = lastFingerDown + 1;
            else if (distance <= 5)
                finger = lastFingerDown + 2;
            else if (distance <= 7)
                finger = lastFingerDown + 3;
            else
                return CharIKFingers::kFingerPinky;
            if (finger > 4)
                finger = CharIKFingers::kFingerPinky;
            return (CharIKFingers::FingerNum)finger;
        }
    }
}

// Retail's RndHighlightable slot 0 for this class is the shared empty `blr`
// (0x826C3888): the debug-draw body below is not in the retail build.
void CharKeyHandMidi::Highlight() {
#ifdef HX_NATIVE
    if ((!mFirstSpot) || (!mSecondSpot)) {
        return;
    }
    UtilDrawSphere(mFirstSpot->WorldXfm().v, 1.0f, Hmx::Color(1.0f, 1.0f, 1.0f));
    UtilDrawSphere(mSecondSpot->WorldXfm().v, 1.0f, Hmx::Color(1.0f, 1.0f, 1.0f));
    for (int key = 1; key <= 0x19; key++) {
        if (IsBlackKey((KeyboardKey)key)) {
            UtilDrawSphere(unk4c[key], 0.15f, Hmx::Color(0.0f, 1.0f, 0.0f));
            UtilDrawSphere(unk54[key], 0.15f, Hmx::Color(1.0f, 1.0f, 0.0f));
        } else {
            UtilDrawSphere(unk4c[key], 0.15f, Hmx::Color(0.0f, 0.0f, 1.0f));
            UtilDrawSphere(unk54[key], 0.15f, Hmx::Color(1.0f, 0.0f, 1.0f));
        }
    }
#endif
}

// retail 0x822CF970: a linear compare chain in ascending key order (a switch
// compiles to a binary search here)
bool CharKeyHandMidi::IsBlackKey(KeyboardKey key) {
    if (key == 2 || key == 4 || key == 7 || key == 9 || key == 0xb || key == 0xe
        || key == 0x10 || key == 0x13 || key == 0x15 || key == 0x17)
        return true;
    return false;
}

void CharKeyHandMidi::EndTest() {
    if (mIKObject) {
        mIKObject->ReleaseFinger(CharIKFingers::kFingerThumb);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerIndex);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerMiddle);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerRing);
        mIKObject->ReleaseFinger(CharIKFingers::kFingerPinky);
    }
}

DataNode CharKeyHandMidi::OnFingersUp(DataArray *msg) {
    KeyboardKey key = (KeyboardKey)msg->Int(2);
    MILO_ASSERT(key > kNoKey && key <= kKeyC4, 0x65);
    for (int i = 0; i < 5; i++) {
        if (unk6c[i] == (KeyboardKey)(key - kNoKey)) {
            UnkeyFinger((CharIKFingers::FingerNum)i);
        }
    }
    std::remove(unk5c.begin(), unk5c.end(), (KeyboardKey)(key - kNoKey));
    return 0;
}

DataNode CharKeyHandMidi::OnFingersDown(DataArray *msg) {
    KeyboardKey key = (KeyboardKey)msg->Int(2);
    MILO_ASSERT(key > kNoKey && key <= kKeyC4, 0x81);
    unk5c.push_back((KeyboardKey)(key - kNoKey));
    return 0;
}

CharIKFingers::FingerNum CharKeyHandMidi::DefaultSelectFinger(KeyboardKey key) {
    for (int i = 1; i < 5; i++) {
        if (unk6c[i] == 0) {
            KeyFinger((CharIKFingers::FingerNum)i, key);
            return (CharIKFingers::FingerNum)i;
        }
    }
    if (unk6c[0] == 0) {
        KeyFinger(CharIKFingers::kFingerThumb, key);
        return CharIKFingers::kFingerThumb;
    }
    return CharIKFingers::kNumFingers;
}

bool CharKeyHandMidi::KeyFinger(CharIKFingers::FingerNum finger, KeyboardKey setToKey) {
    if (!(finger >= 0 && finger < CharIKFingers::kNumFingers)) {
        MILO_NOTIFY("CharKeyHandMidi: Trying to key non-existent finger");
        return false;
    }
    // retail 0x822CFF10: signed range test and a loop over the fingers
    if (setToKey <= 0 || setToKey > 0x19) {
        MILO_NOTIFY("CharKeyHandMidi: Trying to put finger on non-existent key");
        return false;
    }
    for (int i = 0; i < 5; i++) {
        if (unk6c[i] == setToKey)
            return false;
    }
    mIKObject->SetFinger(unk4c[setToKey], unk54[setToKey], finger);
    unk6c[finger] = setToKey;
    unk68 = finger;
    unk64 = setToKey;
    unk74--;
    return true;
}

void CharKeyHandMidi::UnkeyFinger(CharIKFingers::FingerNum finger) {
    MILO_ASSERT(finger >= 0 && finger < CharIKFingers::kNumFingers, 0x16a);
    mIKObject->ReleaseFinger(finger);
    unk6c[finger] = 0;
    unk74++;
}

#pragma fp_contract(off)
void CharKeyHandMidi::Poll() {
    if (!mFirstSpot)
        return;
    if (!mSecondSpot)
        return;

    if (unk7c && unk7c->Teleported()) {
        unk78 = true;
        if (mIKObject)
            mIKObject->mResetCurHandTrans = true;
    }

    if (unk78) {
        // retail 0x822D1980: the layout is built from three stack Vector3 slots --
        // cur (a 16-byte copy of the first spot, updated in place), then the
        // dead keyDir and up slots reused for the tip/black-key and white-key
        // tip vectors -- each copied whole into the key arrays. The helper
        // calls (Scale/Add, which evaluate z,y,x) versus the per-component
        // products below are what fix retail's instruction order; the
        // -0.4*up offset is added per component, NOT contracted into an fnmsubs.
        Vector3 cur = mFirstSpot->WorldXfm().v;
        Vector3 keyDir;
        Subtract(mSecondSpot->WorldXfm().v, cur, keyDir);
        float keyDist = Length(keyDir);
        Normalize(keyDir, keyDir);
        Vector3 up;
        Normalize(mFirstSpot->WorldXfm().m.z, up);
        const Vector3 &forward = mFirstSpot->WorldXfm().m.y;

        Vector3 tipOff;
        Scale(forward, -1.0f, tipOff);
        Vector3 down;
        Scale(up, -0.4f, down);
        cur.x = down.x + cur.x;
        cur.y = cur.y + down.y;
        cur.z = cur.z + down.z;
        float whiteX = keyDir.x * (keyDist / 14.0f);
        float whiteY = keyDir.y * (keyDist / 14.0f);
        float whiteZ = keyDir.z * (keyDist / 14.0f);
        Vector3 half;
        Scale(keyDir, keyDist / 28.0f, half);
        float backX = tipOff.x * 2.0f;
        float backY = tipOff.y * 2.0f;
        float backZ = tipOff.z * 2.0f;
        Vector3 raise;
        Scale(up, 0.5f, raise);
        Vector3 black;
        black.Set(backX + half.x, backY + half.y, backZ + half.z);
        Add(raise, black, black);

        unk4c[1] = cur;
        Vector3 tip(cur);
        Add(cur, tipOff, tip);
        unk54[1] = tip;

        for (int key = 2; key <= 0x19; key++) {
            if (IsBlackKey((KeyboardKey)key)) {
                Vector3 p;
                Add(cur, black, p);
                unk4c[key] = p;
                Add(p, tipOff, p);
                unk54[key] = p;
            } else {
                cur.x = cur.x + whiteX;
                cur.y = cur.y + whiteY;
                cur.z = cur.z + whiteZ;
                unk4c[key] = cur;
                Vector3 t;
                Add(cur, tipOff, t);
                unk54[key] = t;
            }
        }
        unk78 = false;
    }

    float now = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    if (now < unk88) {
        for (int i = 0; i < 5; i++) {
            UnkeyFinger((CharIKFingers::FingerNum)i);
        }
        unk5c.clear();
        unk88 = now;
        return;
    }
    unk88 = now;

    int numKeysDown = unk5c.size();
    if (numKeysDown > 5) {
        MILO_NOTIFY("Too many keyboard keys down in one poll: %d\n", numKeysDown);
        unk5c.clear();
        return;
    }

    if (mIsRightHand) {
        std::sort(unk5c.begin(), unk5c.end());
    } else {
        std::sort(unk5c.begin(), unk5c.end(), std::greater<int>());
    }

    if (numKeysDown <= 0)
        return;

    if (unk74 == 5) {
        switch (numKeysDown) {
        case 1: {
            KeyboardKey k0 = unk5c[0];
            CharIKFingers::FingerNum f =
                FindPreferredFinger(k0, (KeyboardKey)unk64, (CharIKFingers::FingerNum)unk68);
            if (f != CharIKFingers::kNumFingers)
                KeyFinger(f, k0);
            break;
        }
        case 2: {
            KeyboardKey k0 = unk5c[0];
            KeyboardKey k1 = unk5c[1];
            CharIKFingers::FingerNum f0 =
                FindPreferredFinger(k0, (KeyboardKey)unk64, (CharIKFingers::FingerNum)unk68);
            if (f0 > CharIKFingers::kFingerMiddle)
                f0 = CharIKFingers::kFingerMiddle;
            KeyFinger(f0, k0);
            KeyFinger(
                FindPreferredFinger(k1, (KeyboardKey)unk64, (CharIKFingers::FingerNum)unk68), k1
            );
            break;
        }
        case 3: {
            KeyboardKey k0 = unk5c[0];
            KeyboardKey k1 = unk5c[1];
            KeyboardKey k2 = unk5c[2];
            KeyFinger(CharIKFingers::kFingerThumb, k0);
            CharIKFingers::FingerNum f1 =
                FindPreferredFinger(k1, (KeyboardKey)unk64, (CharIKFingers::FingerNum)unk68);
            if (f1 == CharIKFingers::kFingerPinky)
                KeyFinger(CharIKFingers::kFingerRing, k1);
            else
                KeyFinger(f1, k1);
            KeyFinger(
                FindPreferredFinger(k2, (KeyboardKey)unk64, (CharIKFingers::FingerNum)unk68), k2
            );
            break;
        }
        case 4:
            KeyFinger(CharIKFingers::kFingerThumb, unk5c[0]);
            KeyFinger(CharIKFingers::kFingerIndex, unk5c[1]);
            KeyFinger(CharIKFingers::kFingerMiddle, unk5c[2]);
            KeyFinger(CharIKFingers::kFingerPinky, unk5c[3]);
            break;
        case 5:
            KeyFinger(CharIKFingers::kFingerThumb, unk5c[0]);
            KeyFinger(CharIKFingers::kFingerIndex, unk5c[1]);
            KeyFinger(CharIKFingers::kFingerMiddle, unk5c[2]);
            KeyFinger(CharIKFingers::kFingerRing, unk5c[3]);
            KeyFinger(CharIKFingers::kFingerPinky, unk5c[4]);
            break;
        }
    } else {
        if (numKeysDown > unk74) {
            MILO_NOTIFY(
                "Keyboard fingers: not enough free fingers to play a note, please check "
                "the authoring!"
            );
        }
        std::vector<CharIKFingers::FingerNum> usedFingers;
        bool allOk = true;
        KeyboardKey lastKey = (KeyboardKey)unk64;
        CharIKFingers::FingerNum lastFinger = (CharIKFingers::FingerNum)unk68;
        for (int i = 0; i < numKeysDown; i++) {
            CharIKFingers::FingerNum f = FindPreferredFinger(unk5c[i], lastKey, lastFinger);
            if (f == CharIKFingers::kNumFingers || unk6c[f] != 0) {
                allOk = false;
                break;
            }
            if (std::find(usedFingers.begin(), usedFingers.end(), f) != usedFingers.end()) {
                allOk = false;
                break;
            }
            usedFingers.push_back(f);
        }
        if (allOk) {
            for (int i = 0; i < numKeysDown; i++) {
                KeyFinger(usedFingers[i], unk5c[i]);
            }
        } else {
            for (int i = 0; i < numKeysDown; i++) {
                DefaultSelectFinger(unk5c[i]);
            }
        }
    }
    unk5c.clear();
}
#pragma fp_contract(on)

BEGIN_SAVES(CharKeyHandMidi)
    SAVE_REVS(2, 0)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(CharWeightable)
    bs << mIKObject;
    bs << mFirstSpot;
    bs << mSecondSpot;
    bs << mIsRightHand;
END_SAVES

BEGIN_COPYS(CharKeyHandMidi)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(CharWeightable)
    CREATE_COPY(CharKeyHandMidi)
    BEGIN_COPYING_MEMBERS
        COPY_MEMBER(mIKObject)
        COPY_MEMBER(mFirstSpot)
        COPY_MEMBER(mSecondSpot)
        COPY_MEMBER(mIsRightHand)
    END_COPYING_MEMBERS
END_COPYS

void CharKeyHandMidi::PollDeps(
    std::list<Hmx::Object *> &changedBy, std::list<Hmx::Object *> &change
) {
    changedBy.push_back(mFirstSpot);
    changedBy.push_back(mSecondSpot);
}

// Retail Load (0x822D04C8) stores both rev halves through ONE base register
// (altRev at lbl_82CBD17C, rev at +4) and tests `gRev > 1` with lhz/cmplwi:
// two internal-linkage statics, altRev declared first (BandCamShot.cpp's shape),
// not Object.h's BinStreamRev wrapper.
static unsigned short gAltRev = 0;
static unsigned short gRev = 0;

void CharKeyHandMidi::Load(BinStream &bs) {
    int rev;
    bs >> rev;
    gRev = getHmxRev(rev);
    gAltRev = getAltRev(rev);
    Hmx::Object::Load(bs);
    CharWeightable::Load(bs);
    bs >> mIKObject;
    bs >> mFirstSpot;
    bs >> mSecondSpot;
    if (gRev > 1)
        bs >> mIsRightHand;
}

BEGIN_HANDLERS(CharKeyHandMidi)
    HANDLE(fingers_up, OnFingersUp)
    HANDLE(fingers_down, OnFingersDown)
    HANDLE_ACTION(run_test, RunTest())
    HANDLE_ACTION(end_test, EndTest())
    HANDLE_SUPERCLASS(CharWeightable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(CharKeyHandMidi)
    SYNC_PROP(ik_object, mIKObject)
    SYNC_PROP(first_spot, mFirstSpot)
    SYNC_PROP(second_spot, mSecondSpot)
    SYNC_PROP(is_right_hand, mIsRightHand)
    SYNC_SUPERCLASS(CharWeightable)
END_PROPSYNCS
