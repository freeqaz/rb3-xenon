#include "bandobj/BandLabel.h"
#include "obj/Task.h"
#include "rndobj/Anim.h"
#include "ui/UI.h"
#include "ui/UILabel.h"
#include "ui/UITransitionHandler.h"
#include "utl/BinStream.h"
#include "utl/Locale.h"
#include "utl/Str.h"
#include "utl/Symbols.h"

INIT_REVS(0x11, 0)

void BandLabel::Init() { Register(); }

BandLabel::BandLabel() : UITransitionHandler(this), unk1e8(""), unk1f4(0) {}

BandLabel::~BandLabel() {}

BEGIN_COPYS(BandLabel)
    COPY_SUPERCLASS(UILabel)
    CREATE_COPY(BandLabel)
    BEGIN_COPYING_MEMBERS
        CopyHandlerData(c);
    END_COPYING_MEMBERS
END_COPYS

void BandLabel::Save(BinStream &) { MILO_ASSERT(0, 0x44); }

void BandLabel::Load(BinStream &bs) {
    PreLoad(bs);
}

void BandLabel::PreLoad(BinStream &bs) {
    LOAD_REVS(bs)
    ASSERT_REVS(0x11, 0)
    if (d.rev >= 0x11) {
        UILabel::PreLoad(d.stream);
        LoadHandlerData(d.stream);
    } else {
        MILO_FAIL("Can't load BandLabel older than rev %d", 0x11);
    }
}

void BandLabel::LoadOldBandTextComp(BinStream &bs) {
    int rev;
    bs >> rev;
    Symbol s;
    if (rev > 2)
        MILO_WARN("Can't load new BandTextComp");
    else {
        if (rev < 1) {
            int a, b, c, d;
            bs >> a >> b >> c >> d;
        }
        bs >> s;
        if (s == custom_colors) {
            int dummy;
            int num = 4;
            if (rev >= 2)
                bs >> num;
            for (int i = 0; i < num; i++)
                bs >> dummy;
        }
    }
}

void BandLabel::Poll() {
    UILabel::Poll();
    if (unk1dc.size() >= 2) {
        float val = 0;
        float uisecs = TheTaskMgr.UISeconds() * 1000.0f;
        unk1dc.AtFrame(uisecs, val);
        SetTokenFmt(unk1e4, LocalizeSeparatedInt(val, TheLocale));
        if (uisecs > unk1dc.LastFrame()) {
            unk1dc.clear();
            BandLabelCountDoneMsg msg(this);
            TheUI->Handle(msg, false);
        }
    }
    UpdateHandler();
}

void BandLabel::Count(int i1, int i2, float f, Symbol s) {
    unk1dc.clear();
    Key<float> key;
    key.value = TheTaskMgr.UISeconds() * 1000.0f;
    key.frame = i1;
    unk1dc.push_back(key);
    key.value += f;
    key.frame = i2;
    unk1dc.push_back(key);
    unk1e4 = s;
}

void BandLabel::FinishCount() {
    if (unk1dc.size() >= 2) {
        Key<float> &key = unk1dc[1];
        SetTokenFmt(unk1e4, LocalizeSeparatedInt(key.value, TheLocale));
        unk1dc.clear();
    }
}

bool BandLabel::IsEmptyValue() const { return mTextToken == gNullStr; }

void BandLabel::FinishValueChange() {
    UILabel::SetDisplayText(unk1e8.c_str(), unk1f4);
    UITransitionHandler::FinishValueChange();
}

void BandLabel::SetDisplayText(const char *cc, bool b) {
    unk1e8 = cc;
    unk1f4 = b;
    UITransitionHandler::StartValueChange();
}

BEGIN_HANDLERS(BandLabel)
    HANDLE_ACTION(
        start_count, Count(_msg->Int(2), _msg->Int(3), _msg->Float(4), _msg->Sym(5))
    )
    HANDLE_ACTION(finish_count, FinishCount())
    HANDLE_SUPERCLASS(UILabel)
END_HANDLERS

BEGIN_PROPSYNCS(BandLabel)
    SYNC_PROP_SET(in_anim, GetInAnim(), SetInAnim(_val.Obj<RndAnimatable>()))
    SYNC_PROP_SET(out_anim, GetOutAnim(), SetOutAnim(_val.Obj<RndAnimatable>()))
    SYNC_SUPERCLASS(UILabel)
END_PROPSYNCS
