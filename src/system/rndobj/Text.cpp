#include "rndobj/Text.h"
#include "Text.h"
#include "obj/Msg.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"
#include "rndobj/Draw.h"
#include "rndobj/Font.h"
#include "rndobj/FontBase.h"
#include "rndobj/Mat.h"
#include "rndobj/Mesh.h"
#include "rndobj/Trans.h"
#include "rndobj/Cam.h"
#include "rndobj/Rnd.h"
#include "math/Trig.h"
#include "utl/BinStream.h"
#include "utl/MemMgr.h"
#include "utl/UTF8.h"
#include "wordwrap.h"
#include "ui/UI.h"
#include <algorithm>
#include <map>
#include <set>
#ifdef HX_NATIVE
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

// Explicit template instantiation for MakeString<char>
template const char *MakeString<char>(const char *, const char &);

std::vector<RndText::BlacklightPacket> RndText::sBlacklightPacketPool;
int RndText::sBlacklightPacketCount;
bool RndText::sBlacklightModeEnabled;
std::list<RndText::FontMapBase *> RndText::sFontMapCache;
int TEXT_REV = 0;
float gSuperscriptScale = 0.7f;
float gGuitarScale = 0.7f;
float gGuitarZOffset = 0.2f;

float SegmentLength(
    int start, int end, const float *widths, const unsigned short *chars, float scale
) {
    while (chars[start] == ' ' && start < end)
        start++;
    while (chars[end - 1] == ' ' && start < end)
        end--;
    return (widths[end] - widths[start]) * scale;
}

Transform XfmOnCircleEdge(float circumference, float pos) {
    Transform xfm;
    float sign = circumference >= 0.0f ? 1.0f : -1.0f;

    xfm.m.z.Set(0.0f, 0.0f, 1.0f);

    float offset = sign * -1.5707964f;
    float angle = (pos / circumference) * 6.2831855f + offset;

    float cosA = Cosine(angle);
    float sinA = Sine(angle);

    xfm.v.Set(cosA, sinA, 0.0f);

    float negSign = -sign;
    xfm.m.y.y = sinA * negSign;
    xfm.m.y.x = cosA * negSign;
    xfm.m.y.z = 0.0f * negSign;

    xfm.m.x.z = -(xfm.m.y.x * xfm.m.z.y - xfm.m.z.x * xfm.m.y.y);
    xfm.m.x.x = -(xfm.m.y.z * xfm.m.z.y - xfm.m.z.z * xfm.m.y.y);
    xfm.m.x.y = xfm.m.y.z * xfm.m.z.x - xfm.m.z.z * xfm.m.y.x;

    float radius = (sign * (circumference * 0.15915494f));
    xfm.v.y *= radius;
    xfm.v.x *= radius;
    xfm.v.z *= radius;

    return xfm;
}

bool CalcScreenHeight(float size, RndMesh *mesh, float &heightOut) {
    if (!mesh->Showing())
        return false;

    const Transform &worldXfm = mesh->WorldXfm();
    RndCam *cam = RndCam::Current();

    Vector3 pts[2];
    pts[0].Set(0.0f, 0.0f, size * -0.5f);
    pts[1].Set(0.0f, 0.0f, size * 0.5f);

    Vector2 screens[2];
    for (int i = 0; i < 2; i++) {
        Vector3 world;
        Multiply(pts[i], worldXfm, world);
        cam->WorldToScreen(world, screens[i]);
    }

    float dx = (float)TheRnd.Width() * (screens[0].x - screens[1].x);
    float dy = (float)TheRnd.Height() * (screens[0].y - screens[1].y);
    heightOut = std::sqrt(dx * dx + dy * dy);
    return true;
}

RndText::RndText()
    : mWidth(0), mHeight(0), mCircle(0), mAlignment(kMiddleCenter), mFitType(kFitWrap),
      mCapsMode(kCapsModeNone), mLeading(1), mFixedLength(0), mMarkup(true),
      mBasicMarkup(true), mScrollDelay(0), mScrollRate(1), mScrollPause(0), mWrapEnabled(0),
      mLineHeight(0), mScrollCopies(0), mNumLines(0), mIndentation(0),
      mAltStyle(nullptr), mScrollOffset(0), mCurScrollChars(-1), mScrollOutIndex(-1),
      mStyles(this), mBoundsLeft(0), mBoundsTop(0), mBoundsRight(0), mBoundsBottom(0),
      mNumLinesRendered(0), mConstructScale(0) {
    mStyles.resize(1);
    mFontMaps.reserve(1);
}

RndText::~RndText() {
    FOREACH (it, mFontMaps) {
        delete *it;
    }
}

BEGIN_HANDLERS(RndText)
    HANDLE_EXPR(get_text_size, GetTextSize())
    HANDLE_ACTION(update_text, UpdateText())
    HANDLE_SUPERCLASS(RndDrawable)
    HANDLE_SUPERCLASS(RndTransformable)
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_CUSTOM_PROPSYNC(RndText::Style)
    SYNC_PROP(font, o.mFont)
    SYNC_PROP(size, o.mSize)
    SYNC_PROP_SET(text_color, o.mTextColor.Pack(), o.mTextColor.Unpack(_val.Int()))
    SYNC_PROP_SET(text_alpha, o.mTextColor.alpha, o.mTextColor.alpha = _val.Float())
    SYNC_PROP(font_color_override, o.mFontColorOverride)
    SYNC_PROP_SET(font_color, o.mFontColor.Pack(), o.mFontColor.Unpack(_val.Int()))
    SYNC_PROP_SET(font_alpha, o.mFontColor.alpha, o.mFontColor.alpha = _val.Float())
    SYNC_PROP(italics, o.mItalics)
    SYNC_PROP(kerning, o.mKerning)
    SYNC_PROP(z_offset, o.mZOffset)
    SYNC_PROP(blacklight, o.mBlacklight)
END_CUSTOM_PROPSYNC

BEGIN_PROPSYNCS(RndText)
    SYNC_PROP_SET(text, TextASCII(), SetTextASCII(_val.Str()))
    SYNC_PROP_SET(fixed_length, mFixedLength, SetFixedLength(_val.Int()))
    SYNC_PROP(align, (int &)mAlignment)
    SYNC_PROP(caps_mode, (int &)mCapsMode)
    SYNC_PROP(width, mWidth)
    SYNC_PROP(height, mHeight)
    SYNC_PROP(circle, mCircle)
    SYNC_PROP(fit_type, (int &)mFitType)
    SYNC_PROP(leading, mLeading)
    SYNC_PROP(indentation, mIndentation)
    SYNC_PROP(basic_markup, mBasicMarkup)
    SYNC_PROP(markup, mMarkup)
    SYNC_PROP(scroll_delay, mScrollDelay)
    SYNC_PROP(scroll_rate, mScrollRate)
    SYNC_PROP(scroll_pause, mScrollPause)
    SYNC_PROP(styles, mStyles)
    SYNC_SUPERCLASS(RndDrawable)
    SYNC_SUPERCLASS(RndTransformable)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

RndText::Style::Style(Hmx::Object *owner)
    : mSize(30), mTextColor(1, 1, 1), mFontColorOverride(false), mFontColor(1, 1, 1),
      mItalics(0), mKerning(0), mZOffset(0), mFont(owner), mBlacklight(false) {}

RndText::Style::Style(const Style &s)
    : mFont((memcpy(this, &s, 0x34), s.mFont)) {
    mBlacklight = s.mBlacklight;
}

RndText::StyleState::StyleState(RndText *text, float size) {
    memcpy(this, &text->mStyles[0], 0x34);
    mStyle = &text->mStyles[0];
    mFontMapIdx = text->FontMapIndex(mStyle->mFont, mStyle->mBlacklight);
    mBaseSize = size;
    mSize *= size;
    mActive = true;
}

BinStream &operator<<(BinStream &bs, const RndText::Style &s) {
    bs << s.mFont;
    bs << s.mSize;
    bs << s.mTextColor;
    bs << s.mFontColorOverride;
    bs << s.mFontColor;
    bs << s.mItalics;
    bs << s.mKerning;
    bs << s.mZOffset;
    bs << s.mBlacklight;
    return bs;
}

BEGIN_SAVES(RndText)
    SAVE_REVS(0x1C, 1)
    SAVE_SUPERCLASS(Hmx::Object)
    SAVE_SUPERCLASS(RndDrawable)
    SAVE_SUPERCLASS(RndTransformable)
    bs << mAlignment;
    bs << mText;
    bs << mWidth;
    bs << mLeading;
    bs << mFixedLength;
    bs << mMarkup;
    bs << mCapsMode;
    bs << mHeight;
    bs << mCircle;
    bs << mFitType;
    bs << mStyles;
    bs << mScrollDelay;
    bs << mScrollRate;
    bs << mScrollPause;
    bs << mIndentation;
    bs << mBasicMarkup;
END_SAVES

BEGIN_COPYS(RndText)
    COPY_SUPERCLASS(Hmx::Object)
    COPY_SUPERCLASS(RndDrawable)
    COPY_SUPERCLASS(RndTransformable)
    if (ty != kCopyFromMax) {
        CREATE_COPY(RndText)
        BEGIN_COPYING_MEMBERS
            COPY_MEMBER(mAlignment)
            COPY_MEMBER(mCapsMode)
            COPY_MEMBER(mFitType)
            COPY_MEMBER(mWidth)
            COPY_MEMBER(mHeight)
            COPY_MEMBER(mCircle)
            COPY_MEMBER(mLeading)
            COPY_MEMBER(mMarkup)
            SetFixedLength(c->mFixedLength);
            SetText(c->mText.c_str());
            COPY_MEMBER(mStyles)
            COPY_MEMBER(mScrollDelay)
            COPY_MEMBER(mScrollRate)
            COPY_MEMBER(mScrollPause)
            COPY_MEMBER(mIndentation)
        END_COPYING_MEMBERS
        UpdateText();
    }
END_COPYS

BinStream &operator>>(BinStream &bs, RndText::Style &s) {
    bs >> s.mFont;
    bs >> s.mSize;
    bs >> s.mTextColor;
    bs >> s.mFontColorOverride;
    bs >> s.mFontColor;
    bs >> s.mItalics;
    bs >> s.mKerning;
    bs >> s.mZOffset;
    if (TEXT_REV >= 25) {
        bs >> s.mBlacklight;
    }
    return bs;
}

INIT_REVS(28, 1)

BEGIN_LOADS(RndText)
    LOAD_REVS(bs)
    ASSERT_REVS(28, 1)
    Style style(this);
    TEXT_REV = d.rev;
    if (d.rev > 15) {
        Hmx::Object::Load(bs);
    }
    RndDrawable::Load(bs);
    if (d.rev < 7) {
        ObjPtrList<Hmx::Object> objects(this);
        int x;
        bs >> x;
        bs >> objects;
    }
    if (d.rev > 1) {
        RndTransformable::Load(bs);
    }
    if (d.rev < 22) {
        bs >> style.mFont;
    }
    if (d.rev < 3) {
        int idx;
        bs >> idx;
        Alignment align_choices[6] = { kTopLeft,    kTopCenter,    kTopRight,
                                       kBottomLeft, kBottomCenter, kBottomRight };
        mAlignment = align_choices[idx];
    } else {
        bs >> (int &)mAlignment;
    }
    if (d.rev < 2) {
        Vector2 v2;
        bs >> v2;
        SetLocalPos(Vector3(v2.x, 0, -v2.y * 0.75f));
    }
    bs >> mText;
    if (d.rev < 20) {
        std::vector<unsigned short> vec;
        ASCIItoWideVector(vec, mText.c_str());
        WideVectorToUTF8(vec, mText);
    }
    if (d.rev > 0 && d.rev < 22) {
        bs >> style.mTextColor;
    }
    if (d.rev > 12) {
        bs >> mWidth;
    } else if (d.rev > 3) {
        bool b;
        d >> b;
        bs >> mWidth;
        if (!b)
            mWidth = 0.0f;
        if (d.rev < 5 && (mWidth < 0.0f || mWidth > 1000.0f))
            mWidth = 0.0f;
    }
    if (d.rev == 5) {
        String str;
        bs >> str;
    }
    if (d.rev > 4 && d.rev < 11) {
        bool b;
        d >> b;
        if (style.mFont) {
            RndFont *oldfont2d = dynamic_cast<RndFont *>(style.mFont.Ptr());
            MILO_ASSERT(oldfont2d, 0xBC1);
            if (oldfont2d->NumMats() != 0 && oldfont2d->Mat(0)) {
                int zMode = b ? 2 : 0;
                style.mFont->Mat()->SetZMode((ZMode)zMode);
            }
        }
    }
    if (d.rev > 7) {
        bs >> mLeading;
    }
    if (d.rev > 11) {
        int len;
        bs >> len;
        SetFixedLength(len);
    } else if (d.rev > 8) {
        bool b;
        d >> b;
        if (b) {
            SetFixedLength(mText.length());
        } else if (mFixedLength != 0) {
            mFixedLength = 0;
        }
    }
    if (d.rev > 9 && d.rev < 22) {
        bs >> style.mItalics;
    }
    if (d.rev < 22) {
        if (d.rev > 12) {
            bs >> style.mSize;
        } else if (style.mFont) {
            RndFont *oldfont2d = dynamic_cast<RndFont *>(style.mFont.Ptr());
            MILO_ASSERT(oldfont2d, 0xBE9);
            style.mSize = oldfont2d->DeprecatedSize();
        }
        if (d.rev < 13) {
            style.mItalics /= style.mSize;
        }
    }
    if (d.rev > 13) {
        d >> mMarkup;
    }
    if (d.rev > 14) {
        bs >> (int &)mCapsMode;
    } else {
        mCapsMode = kCapsModeNone;
    }
    if (d.rev >= 18 && d.rev < 21) {
        bool b;
        d >> b;
    }
    if (d.rev >= 19 && d.rev < 21) {
        int i, j, k;
        bs >> i;
        bs >> j;
        bs >> k;
    }
    if (d.rev >= 22) {
        if (d.rev > 22) {
            if (d.rev == 23) {
                TheDebug.Notify(MakeString(
                    "%s was bad version 23, suggest reverting and resaving, lost [height] and [fit_type]",
                    PathName(this)
                ));
            } else {
                bs >> mHeight;
                if (d.rev < 24) {
                    String str;
                    bs >> str;
                }
                if (d.altRev > 0) {
                    bs >> mCircle;
                }
                bs >> (int &)mFitType;
            }
        }
        d >> mStyles;
    } else {
        mStyles.resize(1);
        memcpy(&mStyles[0], &style, 0x34);
        mStyles[0].mFont = style.mFont;
    }
    if (d.rev >= 26) {
        bs >> mScrollDelay;
        bs >> mScrollRate;
        bs >> mScrollPause;
    }
    if (d.rev >= 27) {
        bs >> mIndentation;
    }
    if (d.rev >= 28) {
        d >> mBasicMarkup;
    }
    UpdateText();
END_LOADS

void RndText::UpdateSphere() {
    Sphere s;
    s.Zero();
    FOREACH (it, mFontMaps) {
        for (int i = 0; i < (*it)->NumMeshes(); i++) {
            RndMesh *mesh = (*it)->Mesh(i);
            if (mesh) {
                mesh->UpdateSphere();
                s.GrowToContain(mesh->GetSphere());
            }
        }
    }
    SetSphere(s);
}

void RndText::Highlight() { RndDrawable::Highlight(); }

void RndText::Mats(std::list<class RndMat *> &mats, bool) {
    FOREACH (it, mFontMaps) {
        for (int i = 0; i < (*it)->NumMaterials(); i++) {
            RndMat *mat = (*it)->Material(i);
            if (mat) {
                mats.push_back(mat);
            }
        }
    }
}

RndDrawable *RndText::CollideShowing(const Segment &s, float &f, Plane &p) {
    FOREACH (it, mFontMaps) {
        for (int i = 0; i < (*it)->NumMeshes(); i++) {
            RndMesh *mesh = (*it)->Mesh(i);
            if (mesh && mesh->CollideShowing(s, f, p)) {
                return this;
            }
        }
    }
    return nullptr;
}

int RndText::CollidePlane(const Plane &p) {
    int ret = 0;
    FOREACH (it, mFontMaps) {
        for (int i = 0; i < (*it)->NumMeshes(); i++) {
            RndMesh *mesh = (*it)->Mesh(i);
            if (mesh) {
                int meshCol = mesh->CollidePlane(p);
                if (meshCol == 0) {
                    return 0;
                }
                if (meshCol > 0) {
                    if (ret < 0) {
                        return 0;
                    } else {
                        ret = meshCol;
                    }
                } else if (ret > 0) {
                    return 0;
                } else {
                    ret = meshCol;
                }
            }
        }
    }
    return ret;
}

float RndText::GetDistanceToPlane(const Plane &p, Vector3 &v) {
    if (mFontMaps.empty())
        return 0;
    float ret = 0;
    bool first = true;
    FOREACH (it, mFontMaps) {
        for (int i = 0; i < (*it)->NumMeshes(); i++) {
            RndMesh *mesh = (*it)->Mesh(i);
            if (mesh) {
                Vector3 vec;
                float dist = mesh->GetDistanceToPlane(p, vec);
                if (first || std::fabs(dist) < std::fabs(ret)) {
                    first = false;
                    v = vec;
                    ret = dist;
                }
            }
        }
    }
    return ret;
}

bool RndText::MakeWorldSphere(Sphere &s, bool b) {
    s.Zero();
    FOREACH (it, mFontMaps) {
        for (int i = 0; i < (*it)->NumMeshes(); i++) {
            RndMesh *mesh = (*it)->Mesh(i);
            if (mesh) {
                Sphere localSphere;
                if (b) {
                    mesh->MakeWorldSphere(localSphere, true);
                } else {
                    if (mesh->GetSphere().GetRadius() != 0.0f) {
                        Multiply(mesh->GetSphere(), mesh->WorldXfm(), localSphere);
                    }
                }
                s.GrowToContain(localSphere);
            }
        }
    }
    return s.GetRadius() != 0.0f;
}

void RndText::Init() {
    REGISTER_OBJ_FACTORY(RndText)
    SystemConfig("rnd")->FindData("text_superscript_scale", gSuperscriptScale, false);
    SystemConfig("rnd")->FindData("text_guitar_scale", gGuitarScale, false);
    SystemConfig("rnd")->FindData("text_guitar_z_offset", gGuitarZOffset, false);
    unsigned int ui = 1;
    static Symbol kor("kor");
    if (SystemLanguage() == kor)
        ui = 5;
    WordWrap_SetOption(ui);
}

RndText::FontMap::~FontMap() {
    while (mPages.size() != 0) {
        delete mPages.back();
        mPages.pop_back();
    }
}

void RndText::FontMap::SetFont(RndFontBase *f) {
    MILO_ASSERT(f->ClassName() == RndFont::StaticClassName(), 0x75);
    mFont = static_cast<RndFont *>(f);
    while (mPages.size() > mFont->NumMats()) {
        delete mPages.back();
        mPages.pop_back();
    }
    mPages.reserve(mFont->NumMats());
    while (mPages.size() < mFont->NumMats()) {
        mPages.push_back(new Page());
    }
}

void RndText::FontMap::ResetDisplayableChars() {
    for (int i = 0; i < mPages.size(); i++) {
        mPages[i]->displayableChars = 0;
    }
}

void RndText::FontMap::IncrementDisplayableChars(unsigned short num) {
    int page = mFont->CharPage(num);
    if (page >= 0) {
        mPages[page]->displayableChars++;
    }
}

void ResetFontMapPageMeshFaces(RndMesh *mesh, int numFaces) {
    MILO_ASSERT(mesh, 0x96);
#ifdef HX_NATIVE
    if (numFaces <= 0 || numFaces > 100000) return;
#endif
    mesh->Faces().resize(numFaces);
    std::vector<RndMesh::Face>::iterator it = mesh->Faces().begin();
    std::vector<RndMesh::Face>::iterator itEnd = mesh->Faces().end();
    int num = 0;
    for (; it != itEnd; it += 2, num += 4) {
        it[0].Set(num, num + 1, num + 2);
        it[1].Set(num, num + 2, num + 3);
    }
}

void RndText::FontMap::AllocateMeshes(RndText *text, int fixedLength) {
    for (int i = 0; i < mPages.size(); i++) {
        Page &page = *(mPages[i]);
#ifdef HX_NATIVE
        // Guard against garbage displayableChars from font loading issues
        if (page.displayableChars < 0 || page.displayableChars > 10000) {
            page.displayableChars = 0;
        }
#endif
        if (!page.mesh && mFont && page.displayableChars > 0) {
            page.mesh = Hmx::Object::New<RndMesh>();
#ifdef HX_NATIVE
            // Label text meshes for GPU debug (buffer names + frame capture).
            // Uses a side-table label instead of SetName to avoid registering
            // in ObjectDir, which would cause double-draws during traversal.
            {
                extern void SetMeshDebugLabel(RndMesh*, const char*);
                char label[64];
                snprintf(label, sizeof(label), "TEXT_%.48s_p%d", text->Name(), i);
                SetMeshDebugLabel(page.mesh, label);
            }
#endif
        }
        RndMesh *mesh = page.mesh;
        page.mSyncFlags = 0x1F;
        page.mVertStart = 0;
        if (mesh) {
            mesh->SetTransParent(text, false);
            mesh->SetTransConstraint(
                RndTransformable::kConstraintParentWorld, nullptr, false
            );
            if (mFont) {
                auto fontMat = mFont->Mat(i);
                mesh->SetMat(fontMat);
            }
            mesh->SetShowing(page.displayableChars > 0);
            if ((unsigned int)fixedLength == 0) {
                mesh->SetMutable(0);
                ResetFontMapPageMeshFaces(mesh, page.displayableChars * 2);
                page.mSyncFlags |= 0xA0;
                mesh->Verts().resize(page.displayableChars * 4);
            } else if (mesh->Mutable() == 0 || mesh->Verts().size() != fixedLength * 4) {
                mesh->SetMutable(0x1F);
                ResetFontMapPageMeshFaces(mesh, page.displayableChars * 2);
                page.mSyncFlags |= 0xA0;
                mesh->Verts().resize(page.displayableChars * 4);
            }
#ifndef HX_NATIVE
            MILO_ASSERT(mesh->Verts().size() >= page.displayableChars * 4, 0xD2);
#else
            // Clamp to available verts in native builds
            if (mesh->Verts().size() < page.displayableChars * 4)
                page.displayableChars = mesh->Verts().size() / 4;
            page.mVertStart = mesh->Verts().begin();
#endif
        }
#ifndef HX_NATIVE
        MILO_ASSERT(!fixedLength || (page.displayableChars <= fixedLength), 0xD5);
#else
        if (fixedLength && page.displayableChars > fixedLength)
            page.displayableChars = fixedLength;
#endif
    }
}

void RndText::FontMap::CleanupSyncMeshes() {
    for (int i = 0; i < mPages.size(); i++) {
        Page &page = *(mPages[i]);
        RndMesh *mesh = page.mesh;
        if (mesh) {
            while (page.mVertStart != mesh->Verts().end()) {
                RndMesh::Vert *old = page.mVertStart++;
                old->pos.x = 0.0f;
                old->pos.y = 0.0f;
                old->pos.z = 0.0f;
            }
            mesh->Sync(page.mSyncFlags);
        }
    }
}

void RndText::FontMap::SetupScrolling() {
    for (int i = 0; i < NumMeshes(); i++) {
        RndMesh *mesh = Mesh(i);
        if (mesh) {
            mesh->SetTransConstraint(RndTransformable::kConstraintNone, nullptr, false);
        }
    }
}

void RndText::FontMap::UpdateScrolling(float f1) {
    for (int i = 0; i < NumMeshes(); i++) {
        RndMesh *mesh = Mesh(i);
        if (mesh) {
            Vector3 pos = mesh->LocalXfm().v;
            pos.x = f1;
            mesh->SetLocalPos(pos);
        }
    }
}

RndText::FontMap3d::~FontMap3d() {
    for (int i = 0; i < mMeshes.size(); i++) {
        if (mMeshes[i]) {
            delete mMeshes[i];
        }
    }
}

void RndText::FontMap3d::SetFont(RndFontBase *f) {
    MILO_ASSERT(f->ClassName() == RndFont3d::StaticClassName(), 0x17D);
    mFont = static_cast<RndFont3d *>(f);
}

void RndText::SetFixedLength(int len) {
    if (mFixedLength != len) {
        mFixedLength = len;
        if (mFixedLength != 0) {
            const char *p = mText.c_str();
            int newLen;
            for (newLen = 0; *p != '\0' && newLen < mFixedLength; newLen++) {
                unsigned short us;
                p += DecodeUTF8(us, p);
            }
            mText.resize((intptr_t)p + mFixedLength - newLen - (intptr_t)mText.c_str());
        }
    }
}

void RndText::DoBasicMarkup() {
    while (mText.contains("\\q")) {
        mText.replace(mText.find("\\q"), 2, "\"");
    }
}

int RndText::FontMapIndex(RndFontBase *f, bool b) {
    for (int i = 0; i < mFontMaps.size(); i++) {
        if (mFontMaps[i]->Font() == f && mFontMaps[i]->mBlacklight == b) {
            return i;
        }
    }
    return -1;
}

float RndText::ComputeHeight(int i1, float f2, float &f3) {
    float f1;
    if (mStyles[0].mFont) {
        f1 = mStyles[0].mFont->AspectRatio() * mStyles[0].mSize * f2;
    } else {
        f1 = 0;
    }
    f3 = mLeading * f1;
    return ((i1 - 1) * mLeading + 1.0f) * f1;
}

struct WrapPoint {
    int charIdx;
    unsigned int cost;
    int bestPrevIdx;
    int nextIdx;
    float lineWidth;
    bool isLineEnd;
    bool isHardBreak;
};

void RndText::WrapText(
    const unsigned short *wideChars, int wLen, float *charWidths,
    HX_VECTOR(Line) &lines, Hmx::Rect &bounds, float scale
) {
    MemPushTemp();
    lines.reserve(100);
    auto _tmp0 = lines.begin();
    lines.erase(_tmp0, lines.end());
    MemPopTemp();
    StyleState style(this, scale);
    auto& _ref0 = mWidth;
    auto& _ref1 = mAlignment;
    if (mFontMaps.size() == 0 || wLen == 0 || mStyles[0].mFont == 0) {
        Line emptyLine;
        emptyLine.mStart = 0;
        emptyLine.mEnd = 0;
        emptyLine.mWidth = 0.0f;
        if (lines.size() > 1) lines.erase(lines.begin() + 1, lines.end());
        else lines.insert(lines.end(), 1 - lines.size(), emptyLine);
        lines[0].mWidth = 0.0f;
        lines[0].mStart = wideChars;
        lines[0].mEnd = wideChars;
    } else if (_ref0 == 0.0f) {
        Line emptyLine;
        emptyLine.mStart = 0;
        emptyLine.mEnd = 0;
        emptyLine.mWidth = 0.0f;
        if (lines.size() > 1) lines.erase(lines.begin() + 1, lines.end());
        else lines.insert(lines.end(), 1 - lines.size(), emptyLine);
        lines[0].mYPos = 0.0f;
        lines[0].mXStart = 0.0f;
        lines[0].mStart = wideChars;
        lines[0].mEnd = wideChars + wLen;
        lines[0].mWidth = SegmentLength(0, wLen, charWidths, wideChars, scale);
    } else {
        WrapPoint *wps = (WrapPoint *)_alloca((wLen + 1) * sizeof(WrapPoint));
        bool activeMarkup = style.mActive;
        wps[0].charIdx = 0;
        wps[0].lineWidth = 0.0f;
        wps[0].cost = 0;
        wps[0].isLineEnd = false;
        wps[0].bestPrevIdx = -1;
        wps[0].nextIdx = -1;
        wps[0].isHardBreak = true;
        float minW = _ref0 * 0.7f;
        float goodW = _ref0 * 0.95f;
        const unsigned short *brkChars = wideChars;
        if (mMarkup) {
            unsigned short *stripped = (unsigned short *)_alloca((wLen + 1) * 2);
            brkChars = stripped;
            const unsigned short *s = wideChars;
            unsigned short c = *s;
            while (c != 0) {
                if (c == 0x3c) {
                    unsigned short t = c;
                    do { if (t == 0x3e) break; s++; t = *s; } while (t != 0);
                    if (t == 0) { c = *s; continue; }
                } else { *stripped = c; stripped++; }
                s++;
                c = *s;
            }
            *stripped = 0;
        }
#ifdef HX_NATIVE
        // On Linux, wchar_t is 4 bytes but our text buffers are unsigned short (2 bytes).
        // Convert brkChars to wchar_t for WordWrap_CanBreakLineAt.
        // Allocate +2 padding: WordWrap_CanBreakLineAt reads cur[1] (lookahead)
        // which can read one past the null terminator.
        int brkLen = 0.0f;
        { const unsigned short *t = brkChars; while (*t++) brkLen++; }
        wchar_t *brkWideBuf = (wchar_t *)_alloca((brkLen + 2) * sizeof(wchar_t));
        for (int bi = 0; brkLen - bi >= 0; bi++) brkWideBuf[bi] = (wchar_t)brkChars[bi];
        brkWideBuf[brkLen + 1] = 0;
#define BRKWIDE_BASE brkWideBuf
#else
        // On PPC, wchar_t is 2 bytes (same as unsigned short) — cast directly.
#define BRKWIDE_BASE ((const wchar_t *)brkChars)
#endif

        int wpI = 0, numWp = 1;
        const unsigned short *cur = wideChars;
        int cCount = 0;
        for (;;) {
            unsigned short ch = *cur;
            const wchar_t *brkW = &BRKWIDE_BASE[cCount];
            int prevI = wpI;
            WrapPoint *nxt = &wps[numWp];
            if (ch == 0 || ch == '\n') {
                int bestWp = -1, bestC = 100000;
                bool ovf = false;
                float bestLineLen = 0.0f;
                int endI = (int)(cur - wideChars);
                float mxW = _ref0;
                for (int wi = wpI; wi >= 0; wi--) {
                    float lineLen = SegmentLength(wps[wi].charIdx, endI, charWidths, wideChars, scale);
                    unsigned int pen = 10;
                    if (lineLen > mxW) {
                        if (wi != prevI) {
                            if (bestWp != -1) { ovf = true; }
                        }
                    } else {
                        if (_ref1 & 0x20) {
                            float fw = SegmentLength(wps[wi].charIdx, wLen, charWidths, wideChars, scale);
                            if (fw >= mxW) {
                                pen = (unsigned int)(int)((1.0f - lineLen / mxW) * 30.0f);
                                if (lineLen < minW) pen += 100;
                            }
                        } else {
                            if (endI - wps[wi].charIdx <= 4) pen = 50;
                        }
                    }
                    int tc = (int)pen + wps[wi].cost;
                    if (tc < bestC) { bestC = tc; bestWp = wi; bestLineLen = lineLen; }
                    if (wps[wi].isHardBreak || ovf) break;
                }
                MILO_ASSERT(bestWp != -1, 0x6ed);
                MILO_ASSERT(numWp < wLen + 1, 0x6ef);
                nxt->charIdx = endI;
                nxt->cost = bestC;
                nxt->bestPrevIdx = bestWp;
                nxt->isLineEnd = true;
                nxt->nextIdx = -1;
                nxt->lineWidth = bestLineLen;
                nxt->isHardBreak = true;
                numWp++; wpI++;
                wps[bestWp].isLineEnd = false;
                if (ch == 0) goto buildLines;
            } else {
                unsigned short mc = ch;
                if (ch == 0x3c) {
                    if (mMarkup) {
                    cur = ParseMarkup(cur, style, mc);
                    cCount--;
                    brkW = &BRKWIDE_BASE[cCount];
                    cur--;
                    if (style.mActive) {
                        activeMarkup = true;
                    }
                }
                }
                if (mc != 0) {
                    int ci = (int)(cur - wideChars);
                    if (activeMarkup) {
                        bool canBrk = cCount > 0 && WordWrap_CanBreakLineAt(brkW, BRKWIDE_BASE);
                        if (canBrk) {
                            int bestWp = -1, bestC = 100000;
                            bool ovf = false;
                            float bestLineLen = 0.0f;
                            for (int wi = wpI; wi >= 0; wi--) {
                                float lineLen = SegmentLength(wps[wi].charIdx, ci, charWidths, wideChars, scale);
                                MILO_ASSERT(lineLen >= bestLineLen, 0x65d);
                                unsigned int pen = 10;
                                float mxW2 = _ref0;
                                if (lineLen > mxW2) {
                                    if (wi != prevI) {
                                        if (bestWp != -1) { ovf = true; }
                                    }
                                } else {
                                    if (lineLen < goodW) {
                                        float fw = SegmentLength(wps[wi].charIdx, wLen, charWidths, wideChars, scale);
                                        if (fw >= mxW2) {
                                            pen = (unsigned int)(int)((1.0f - lineLen / mxW2) * 60.0f);
                                            if (lineLen < minW) pen += 200;
                                        }
                                    }
                                }
                                if ((int)(wps[wi].cost + pen) <= bestC) {
                                    bestC = wps[wi].cost + pen;
                                    bestWp = wi; bestLineLen = lineLen;
                                }
                                if (wps[wi].isHardBreak || ovf) break;
                            }
                            MILO_ASSERT(bestWp != -1, 0x690);
                            MILO_ASSERT(numWp < wLen + 1, 0x693);
                            nxt->lineWidth = bestLineLen;
                            nxt->charIdx = ci;
                            nxt->cost = bestC;
                            nxt->bestPrevIdx = bestWp;
                            nxt->nextIdx = -1;
                            nxt->isLineEnd = true;
                            numWp++; wpI++;
                            nxt->isHardBreak = false;
                            wps[bestWp].isLineEnd = false;
                        }
                    }
                    if (activeMarkup != style.mActive) {
                        MILO_ASSERT(style.brk == false, 0x6a2);
                        activeMarkup = false;
                    }
                }
            }
            cur++; cCount++;
        }
    buildLines: {
            int idx = numWp - 1;
        if (idx != 0) {
            do {
                unsigned int pi = wps[idx].bestPrevIdx;
                wps[pi].nextIdx = idx;
                idx = pi;
            } while (idx != 0);
        }
        Line ol;
        int chi = wps[0].nextIdx;
        while (chi != -1) {
            WrapPoint *wp = &wps[chi];
            WrapPoint *pw = &wps[wp->bestPrevIdx];
            const unsigned short *cs = wideChars + (pw->charIdx & 0x7fffffff);
            const unsigned short *ce = wideChars + (wp->charIdx & 0x7fffffff);
            while (cs < ce && (*cs == ' ' || *cs == '\t')) cs++;
            while (ce > cs) {
                unsigned short p = *(ce - 1);
                if (p != ' ' && p != '\n' && p != '\t') break;
                ce--;
            }
            ol.mStart = cs;
            ol.mEnd = ce;
            ol.mWidth = wp->lineWidth;
            lines.push_back(ol);
            chi = wp->nextIdx;
        }
        if (lines.size() == 0) {
            ol.mWidth = 0.0f;
            ol.mStart = wideChars;
            ol.mEnd = wideChars;
            lines.push_back(ol);
        }
    } }
    float topY = 0.0f;
    float ls;
    float th = ComputeHeight((int)lines.size(), scale, ls);
    bounds.h = th;
    if (_ref1 & 0x20) topY = th * 0.5f;
    else if (_ref1 & 0x40) topY = th;
    bounds.w = 0.0f;
    for (unsigned int i = 0; i < lines.size(); i++) {
        Line &l = lines[i];
        bounds.w = Max(bounds.w, l.mWidth);
        if (_ref1 & 0x2) l.mXStart = l.mWidth * -0.5f;
        else if (_ref1 & 0x4) l.mXStart = -l.mWidth;
        else l.mXStart = 0.0f;
        l.mYPos = topY;
        topY -= ls;
    }
    bounds.x = lines[0].mXStart;
    bounds.y = lines[0].mYPos - bounds.h;
}
#undef BRKWIDE_BASE

void RndText::SetText(const char *str) {
    if (mFixedLength != 0) {
        MILO_ASSERT(mText.capacity() >= mFixedLength, 0x75E);
        const char *p = str;
        for (int newLen = 0; *p != '\0' && newLen < mFixedLength; newLen++) {
            unsigned short us;
            p += DecodeUTF8(us, p);
        }
        int newLen = p - str;
        if (mText.capacity() < newLen) {
            mText.resize(newLen);
        }
        strncpy((char *)mText.c_str(), str, newLen);
        char *last = (char *)mText.c_str() + newLen;
        *last = '\0';
    } else {
        mText = str;
    }
    if (mBasicMarkup) {
        DoBasicMarkup();
    }
}

String RndText::TextASCII() const {
    String str;
    {
        MemTemp tmp;
        str.resize(UTF8StrLen(mText.c_str()) + 1);
    }
    UTF8toASCIIs((char *)str.c_str(), str.capacity(), mText.c_str(), '*');
    return str;
}

void RndText::BuildFontMaps(bool b1) {
    if (b1) {
        for (auto it = mFontMaps.begin(); it != mFontMaps.end();
             it = mFontMaps.erase(it)) {
            sFontMapCache.push_back(*it);
        }
    }
    if (mFontMaps.empty()) {
#ifdef HX_NATIVE
#endif
        for (int i = 0; i < mStyles.size(); i++) {
            RndFontBase *font = mStyles[i].mFont;
            if (font) {
                if (FontMapIndex(font, mStyles[i].mBlacklight) == -1) {
                    FontMapBase *map = AcquireFontMap(font);
                    map->mBlacklight = mStyles[i].mBlacklight;
                    mFontMaps.push_back(map);
                }
            }
        }
    }
}

void RndText::SetTextASCII(const char *cstr) {
    String str;
    {
        MemTemp tmp;
        std::vector<unsigned short> vec;
        ASCIItoWideVector(vec, cstr);
        WideVectorToUTF8(vec, str);
    }
    SetText(str.c_str());
}

int RndText::ConvertTextToWide(const char *str, HX_VECTOR(unsigned short) &wideChars) {
    char emptyStr = 0;
    const char *p = str;
    if (str == 0) {
        p = &emptyStr;
    }

    // Manual strlen to match target inline loop
    const char *s = p;
    while ('\0' != *s++) {}
    MemPushTemp();
    unsigned short zero = 0;
    unsigned short ssChar;
    wideChars.resize(((s - p) - 1) * 2 + 1, zero);
    MemPopTemp();

    unsigned short *out = &wideChars[0];
    int capsMode = mCapsMode;

    if (capsMode == kForceUpper) {
        DecodeUTF8(ssChar, "\xC3\x9F");
        int fixedLen = mFixedLength;
        unsigned short *limit;
        if (fixedLen != 0) {
            limit = out + fixedLen;
        } else {
            limit = 0;
        }
        while (*p != '\0') {
            if (out == limit) break;
            unsigned short ch;
            p += DecodeUTF8(ch, p);
            if (ch == ssChar) {
                *out = 0x53;
                out++;
                if (out == limit) break;
                *out = 0x53;
            } else {
                *out = WToUpper(ch);
            }
            out++;
        }
    } else if (capsMode == kForceLower) {
        while (*p != '\0') {
            unsigned short ch;
            p += DecodeUTF8(ch, p);
            *out = WToLower(ch);
            out++;
        }
    } else {
        while (*p != '\0') {
            p += DecodeUTF8(*out, p);
            out++;
        }
    }

    *out = 0;
    ReplaceMissingCharacters(wideChars);
    return (int)(out - &wideChars[0]);
}

void RndText::ReplaceMissingCharacters(HX_VECTOR(unsigned short) &wideChars) {
    std::map<RndFontBase *, std::set<unsigned short> > missingMap;
    unsigned short curChar;
    HX_VECTOR(unsigned short) origChars;
    bool copied = false;
    StyleState styleState(this, 1.0f);
    unsigned short *p = &wideChars[0];
    curChar = *p;
    while (curChar != 0) {
        curChar = *p;
        if (curChar == 0x3c && mMarkup) {
            p = (unsigned short *)ParseMarkup(p, styleState, curChar);
            p = p - 1;
        } else if (curChar != 10 && styleState.mFontMapIdx != -1) {
            FontMapBase *fm = mFontMaps[styleState.mFontMapIdx];
            RndFontBase *font;
            if (fm != 0 && (font = fm->Font()) != 0
                && !font->CharDefined(curChar)) {
                missingMap[font].insert(curChar);

                unsigned short replacements[] = {0x25a1, 0x3f, 0x23, 0x2a, 0x21, 0x39};
                curChar = 0;
                int i = 0;
                unsigned short *rp = replacements;
                do {
                    unsigned short tryChar = *rp;
                    if (font->CharDefined(tryChar)) {
                        curChar = tryChar;
                        break;
                    }
                    i = i + 1;
                    rp = rp + 1;
                } while (i < 6);

                if (curChar == 0) {
                    std::vector<unsigned short> fontChars(font->mChars);
                    unsigned int j = 0;
                    unsigned int count = fontChars.size();
                    unsigned short *fp = &fontChars[0];
                    unsigned short c = curChar;
                    if (count != 0) {
                        do {
                            c = *fp;
                            bool skip;
                            if (c == 0x20 || c == 0xa0) {
                                skip = true;
                            } else {
                                skip = false;
                            }
                            if (!skip)
                                break;
                            j = j + 1;
                            fp = fp + 1;
                            c = curChar;
                        } while (j < count);
                    }
                    curChar = c;
                }

                if (curChar != 0) {
                    if (!copied) {
                        origChars = wideChars;
                        copied = true;
                    }
                    *p = curChar;
                }
            }
        }
        p = p + 1;
        curChar = *p;
    }

    {
        std::map<RndFontBase *, std::set<unsigned short> >::iterator mapIt = missingMap.begin();
        if (mapIt != missingMap.end()) {
        unsigned int origSize = origChars.size();
        do {
            RndFontBase *font = mapIt->first;
            const char *pluralS = "s";
            if (mapIt->second.size() <= 1) {
                pluralS = "";
            }
            {
                String msg(MakeString("%s:%s char%s (", PathName(this), TextToken(), pluralS));

                for (std::set<unsigned short>::iterator setIt = mapIt->second.begin();
                     setIt != mapIt->second.end(); ++setIt) {
                    unsigned short ch = *setIt;
                    bool printable = true;
                    if (ch < 0x20 || ch >= 0xff || ch == 0x25 || ch == 0x7f) {
                        printable = false;
                    }
                    char displayChar;
                    if (printable) {
                        displayChar = (char)ch;
                    } else {
                        displayChar = '?';
                    }
                    const char *sep = "";
                    if (setIt != mapIt->second.begin()) {
                        sep = ", ";
                    }
                    msg += MakeString("%s\'%c\' 0x%02X", sep, displayChar, ch);
                }

                msg += MakeString(") missing from %s in string \"", PathName(font));

                unsigned int k = 0;
                unsigned short *qp = &origChars[0];
                if (origSize != 0) {
                    do {
                        unsigned short qch = *qp;
                        if (qch == 0)
                            break;
                        bool printable = true;
                        if (qch < 0x20 || qch >= 0xff || qch == 0x25 || qch == 0x7f) {
                            printable = false;
                        }
                        if (printable) {
                            msg += MakeString("%c", (char)qch);
                        } else {
                            msg += MakeString("\\x%02X", qch);
                        }
                        k = k + 1;
                        qp = qp + 1;
                    } while (k < origSize);
                }

                msg += "\"";
                MILO_NOTIFY(msg.c_str());
            }
            ++mapIt;
        } while (mapIt != missingMap.end());
        }
    }
}

int RndText::OnComputeCharWidths(const unsigned short *wideChars, float *widths, bool marqueeWrap) {
#ifdef HX_NATIVE
    // Reset displayable char counts before recomputing — prevents unbounded growth
    // when UpdateText() is called every frame (ResetDisplayableChars was never called)
    for (auto *fm : mFontMaps) {
        fm->ResetDisplayableChars();
    }
#endif
    StyleState styleState(this, 1.0f);
    std::vector<RndFontBase *> missingFonts;
    unsigned short prevChar = 0;
    std::vector<unsigned short> negWidthChars;
    std::vector<unsigned short> missingChars;
    float cumWidth = 0.0f;
    widths[0] = 0.0f;
    const unsigned short *p = wideChars;
    float *w = widths + 1;
    for (;;) {
        if (*p == 0) {
            if (!missingChars.empty()) {
                auto pathStr = PathName(this);
                String msg = MakeString("%s:%s '", pathStr, ClassName().Str());
                String hexMsg;
                for (unsigned int i = 0; i < missingChars.size(); i++) {
                    unsigned short tmp[2] = {missingChars[i], 0};
                    msg += MakeString("%s", WideCharToChar(tmp));
                    hexMsg += MakeString("0x%02x ", missingChars[i]);
                }
                msg += MakeString("' (%s", hexMsg.c_str());
                String fontNames;
                for (unsigned int i = 0; i < missingFonts.size(); i++) {
                    fontNames += MakeString("%s ", PathName(missingFonts[i]));
                }
                msg += MakeString(") missing from font(s) (%s) in string \"", fontNames.c_str());
                const unsigned short *q = wideChars;
                while (*q != 0) {
                    unsigned short tmp[2] = {*q, 0};
                    msg += MakeString("%s", WideCharToChar(tmp));
                    q++;
                }
                msg += "\"";
                MILO_NOTIFY("%s", msg.c_str());
            }
            if (!negWidthChars.empty()) {
                String msg = MakeString("%s: '", PathName(this));
                String hexMsg;
                for (unsigned int i = 0; i < negWidthChars.size(); i++) {
                    unsigned short tmp[2] = {negWidthChars[i], 0};
                    msg += MakeString("%s", WideCharToChar(tmp));
                    hexMsg += MakeString("0x%02x ", negWidthChars[i]);
                }
                msg += MakeString("' (%s", hexMsg.c_str());
                if (styleState.mFontMapIdx != -1) {
                    RndFontBase *font = mFontMaps[styleState.mFontMapIdx]->Font();
                    msg += MakeString(") have negative widths from %s in string \"", PathName(font));
                }
                const unsigned short *q = wideChars;
                while (*q != 0) {
                    unsigned short tmp[2] = {*q, 0};
                    msg += MakeString("%s", WideCharToChar(tmp));
                    q++;
                }
                msg += "\"";
                MILO_NOTIFY("%s", msg.c_str());
            }
            *w = cumWidth;
            return (int)(w - widths) - 1;
        }
        unsigned short ch = *p;
        if (ch == '<' && mMarkup) {
            unsigned short replaceChar = 0;
            const unsigned short *next = ParseMarkup(p, styleState, replaceChar);
            if (next > p) {
                *w = cumWidth;
                int numSkipped = (int)(next - p);
                for (int i = 1; i < numSkipped; i++) {
                    *(w + i) = cumWidth;
                }
                w += numSkipped;
                p = next;
            }
            if (replaceChar != 0) {
                w--;
                p--;
                ch = replaceChar;
                goto process_char;
            }
        } else {
process_char:
            if (ch != 0 && styleState.mFontMapIdx != -1) {
                FontMapBase *fontMap = mFontMaps[styleState.mFontMapIdx];
                RndFontBase *font = fontMap->Font();
                if (font) {
                    if (mFitType == kFitScrollMarqueeWrapAlways && ch == '\n') {
                        if (!marqueeWrap) {
                            mNumLines++;
                            float lw = (float)((double)mNumLines * (double)mIndentation + (double)cumWidth);
                            mLineWidths.insert(mLineWidths.end(), lw);
                            float lo = (float)((double)mNumLines * (double)mIndentation + (double)cumWidth);
                            mLineOffsets.insert(mLineOffsets.end(), lo);
                        } else {
                            cumWidth = (float)((double)mIndentation + (double)cumWidth);
                        }
                        float charWidth;
                        if (font->CharAdvance(prevChar, (unsigned short)'\n', charWidth)) {
                            fontMap->IncrementDisplayableChars('\n');
                        }
                    } else {
                        float charWidth;
                        bool found = font->CharAdvance(prevChar, ch, charWidth);
                        if (!found) {
                            if (ch != '\n') {
                                if (std::find(missingChars.begin(), missingChars.end(), ch) == missingChars.end()) {
                                    missingChars.push_back(ch);
                                }
                                if (std::find(missingFonts.begin(), missingFonts.end(), font) == missingFonts.end()) {
                                    missingFonts.push_back(font);
                                }
                            }
                        } else {
                            charWidth = (charWidth + styleState.mKerning) * styleState.mSize;
                            if ((double)charWidth < 0.0) {
                                if (ch != '\n') {
                                    if (std::find(negWidthChars.begin(), negWidthChars.end(), ch) == negWidthChars.end()) {
                                        negWidthChars.push_back(ch);
                                    }
                                }
                            } else {
                                cumWidth = (float)((double)charWidth + (double)cumWidth);
                            }
                            fontMap->IncrementDisplayableChars(ch);
                            prevChar = ch;
                        }
                    }
                }
            }
            *w = cumWidth;
            w++;
            p++;
            continue;
        }
    }
}

void RndText::QueueBlacklightPacket(RndMesh *mesh, float f2, int i3) {
    u32 cursize = sBlacklightPacketPool.capacity();
    if ((u32)sBlacklightPacketCount >= cursize) {
        int newsize = 8;
        if (cursize != 0) {
            newsize = cursize * 2;
        }
        BlacklightPacket packet;
        sBlacklightPacketPool.resize(newsize, packet);
    }
#ifdef HX_NATIVE
    int idx = sBlacklightPacketCount++;
    BlacklightPacket &pkt = sBlacklightPacketPool[idx];
    pkt.mMesh = mesh;
    RndMat *mat = mesh->Mat();
    if (mat) {
        pkt.mSavedColor = mat->GetColor();
    }
    pkt.mSize = f2;
    pkt.mSyncFlags = i3;
    pkt.mCam = RndCam::Current();
#else
    int idx = sBlacklightPacketCount++;
    int *pkt_ptr = (int *)&sBlacklightPacketPool[0] + (idx << 3);
    pkt_ptr[0] = (int)mesh;
    int *mat = *(int **)((char *)mesh + 0x128);
    pkt_ptr[1] = *(int *)((char *)mat + 0x2C);
    pkt_ptr[2] = *(int *)((char *)mat + 0x30);
    pkt_ptr[3] = *(int *)((char *)mat + 0x34);
    pkt_ptr[4] = *(int *)((char *)mat + 0x38);
    *(float *)(pkt_ptr + 5) = f2;
    pkt_ptr[6] = i3;
    pkt_ptr[7] = (int)RndCam::Current();
#endif
}

void RndText::ClearBlacklight() { sBlacklightPacketCount = 0; }

void RndText::DrawBlacklight() {
    RndCam *savedCam = RndCam::Current();
    for (int i = 0; i < sBlacklightPacketCount; i++) {
#ifdef HX_NATIVE
        BlacklightPacket &pkt = sBlacklightPacketPool[i];
        if (pkt.mCam && pkt.mCam != RndCam::Current()) {
            pkt.mCam->Select();
        }
        RndMat *mat = pkt.mMesh->Mat();
        if (mat) {
            Hmx::Color &color = mat->GetColor();
            color.red = pkt.mSavedColor.red;
            color.green = pkt.mSavedColor.green;
            color.blue = pkt.mSavedColor.blue;
            mat->MarkDirty(1);
        }
        DrawMesh(pkt.mMesh, pkt.mSize, pkt.mSyncFlags);
#else
        int *pkt = (int *)((char *)&sBlacklightPacketPool[0] + i * 0x20);
        RndCam *cam = (RndCam *)pkt[7];
        if (cam != 0 && cam != RndCam::Current()) {
            cam->Select();
        }
        float savedB = *(float *)(pkt + 3);
        float savedG = *(float *)(pkt + 2);
        float savedR = *(float *)(pkt + 1);
        int *mat = *(int **)((char *)pkt[0] + 0x128);
        *(float *)((char *)mat + 0x2c) = savedR;
        *(float *)((char *)mat + 0x30) = savedG;
        *(float *)((char *)mat + 0x34) = savedB;
        *(int *)((char *)mat + 0x228) |= 1;
        DrawMesh((RndMesh *)pkt[0], *(float *)(pkt + 5), pkt[6]);
#endif
    }
    if (savedCam != 0 && savedCam != RndCam::Current()) {
        savedCam->Select();
    }
}

void RndText::UpdateScrollOffsets() {
    float fVar1 = TheTaskMgr.DeltaUISeconds();
    mScrollTimer += fVar1;

    if (mScrollTimer < mScrollState) {
        return;
    }

    float fVar2 = mScrollSpeed;
    int iVar9 = mFitType;
    float fVar3 = mTotalWidth;
    bool bVar10 = false;
    float dVar12 = fVar2 * fVar1 * 1000.0f;
    float fVar13 = mScrollPos + dVar12;
    mScrollPos = fVar13;
    float widthDiff = fVar3 - mWidth;

    switch (iVar9) {
    case kFitScrollMarqueeWrap: {
        if (fVar2 < 0.0f) {
            float fVar13_2 = -widthDiff;
            if (fVar13 < fVar13_2) {
                mScrollPos = fVar13_2;
                mScrollSpeed = -fVar2;
                bVar10 = true;
            }
        } else if ((fVar2 > 0.0f) && (!(fVar13 < 0.0f))) {
            mScrollSpeed = -fVar2;
            bVar10 = true;
        }
        break;
    }

    case kFitScrollMarqueeReset:
        if (mScrollPos < -fVar3) {
            mScrollPos = 0.0f;
            bVar10 = true;
        }
        mLineHeight = fVar3;
        break;

    case kFitScrollPingPong:
        if (mScrollPos < -(fVar3 + 20.0f)) {
            mScrollPos = 0.0f;
            bVar10 = true;
        }
        break;

    case kFitScrollMarqueeWrapAlways: {
        static Message textScrolledIn("text_scrolled_in", -1);
        static Message textScrolledOut("text_scrolled_out", -1);

        mScrollOffset += dVar12;

        float firstWidth = *mLineWidths.begin();
        if (!((mWidth - mScrollOffset) < firstWidth)) {
            mCurScrollChars++;
            if (mCurScrollChars >= mNumLines) {
                mCurScrollChars = 0;
            }
            textScrolledIn[0] = DataNode(mCurScrollChars);
            if (firstWidth == mTotalWidth) {
                mScrollOffset = mWidth;
            }
            unsigned int count = 0;
            for (auto it = mLineWidths.begin(); it != mLineWidths.end(); ++it) {
                count++;
            }
            if ((unsigned int)mNumLines == count) {
                mLineWidths.insert(mLineWidths.end(), firstWidth);
            }
            mLineWidths.erase(mLineWidths.begin());
            Hmx::Object *alt = mAltStyle;
            if (alt != nullptr) {
                alt->Handle(textScrolledIn, false);
            }
        }

        float firstOffset = *mLineOffsets.begin();
        if (!(mScrollPos > -firstOffset)) {
            mScrollOutIndex++;
            textScrolledOut[0] = DataNode(mScrollOutIndex);
            if (firstOffset == mTotalWidth) {
                mScrollPos = 0.0f;
                mScrollOutIndex = -1;
            }
            mLineOffsets.insert(mLineOffsets.end(), firstOffset);
            mLineOffsets.erase(mLineOffsets.begin());
            Hmx::Object *alt2 = mAltStyle;
            if (alt2 != nullptr) {
                alt2->Handle(textScrolledOut, false);
            }
        }
        break;
    }

    default:
        mScrollPos = 0.0f;
        break;
    }

    if (bVar10) {
        mScrollTimer = 0.0f;
    }

    for (auto puVar7 = mFontMaps.begin(); puVar7 != mFontMaps.end(); ++puVar7) {
        (*puVar7)->UpdateScrolling(mScrollPos);
    }
}

#ifdef HX_NATIVE
// On Linux, wchar_t is 4 bytes but our text buffers use unsigned short (2 bytes).
// Use manual u16 operations instead of wchar_t string functions to avoid buffer overflow.
static const unsigned short kEllipsisU16[] = {'.', '.', '.', 0};
static const unsigned short kBreakCharsU16[] = {' ', '\t', '\n', 0};

static int u16len(const unsigned short *s) {
    int n = 0;
    while (s[n])
        n++;
    return n;
}
static void u16cpy(unsigned short *dst, const unsigned short *src) {
    while ((*dst++ = *src++))
        ;
}
static const unsigned short *u16chr(const unsigned short *s, unsigned short ch) {
    while (*s) {
        if (*s == ch)
            return s;
        s++;
    }
    return nullptr;
}

// Tag name constants for ParseMarkup (2 bytes per char, matching unsigned short buffers)
static const unsigned short kTag_sup[] = {'s', 'u', 'p', 0};
static const unsigned short kTag_gtr[] = {'g', 't', 'r', 0};
static const unsigned short kTag_it[] = {'i', 't', 0};
static const unsigned short kTag_color[] = {'c', 'o', 'l', 'o', 'r', 0};
static const unsigned short kTag_hash[] = {'#', 0};
static const unsigned short kTag_nobreak[] = {'n', 'o', 'b', 'r', 'e', 'a', 'k', 0};
static const unsigned short kTag_alt[] = {'a', 'l', 't', 0};

// Parse up to 'max_vals' space-separated decimal integers from a u16 string buffer.
// Returns the number of values successfully parsed.
static int u16_scan_ints(const unsigned short *s, int *vals, int max_vals) {
    int count = 0;
    while (count < max_vals) {
        // skip spaces
        while (*s == ' ' || *s == '\t')
            s++;
        if (*s == 0 || *s == '>')
            break;
        // parse optional sign + digits
        bool neg = false;
        if (*s == '-') {
            neg = true;
            s++;
        }
        if (*s < '0' || *s > '9')
            break;
        int v = 0;
        while (*s >= '0' && *s <= '9') {
            v = v * 10 + (*s - '0');
            s++;
        }
        vals[count++] = neg ? -v : v;
    }
    return count;
}
#endif

#ifndef HX_NATIVE
static const wchar_t kEllipsisStr[] = L"...";
static const wchar_t kBreakChars[] = L" \t\n";
#endif

void RndText::FitTextJust() {
    BuildFontMaps(true);

    HX_VECTOR(unsigned short) wideChars;
    HX_VECTOR(Line) lines;
    int numChars = ConvertTextToWide(mText.c_str(), wideChars);
    float *charWidths = (float *)_alloca((numChars + 2) * sizeof(float));
    OnComputeCharWidths(&wideChars[0], charWidths, false);

    Hmx::Rect bounds;
    float scale = 1.0f;
    WrapText(&wideChars[0], numChars, charWidths, lines, bounds, scale);

    float lo = 0.2f;
    float hi = mStyles[0].mSize;
    float cur = hi;

    if ((mWidth != 0.0f && mWidth < bounds.w) || (mHeight != 0.0f && mHeight < bounds.h)) {
        if (hi - lo > 0.2f) {
            do {
                cur = (lo + hi) * 0.5f;
                scale = cur / mStyles[0].mSize;
                WrapText(&wideChars[0], numChars, charWidths, lines, bounds, scale);
                if ((mWidth != 0.0f && mWidth < bounds.w) || (mHeight != 0.0f && mHeight < bounds.h)) {
                    hi = cur;
                } else {
                    lo = cur;
                }
            } while (hi - lo > 0.2f);
        }
        if (hi == cur) {
            scale = lo / mStyles[0].mSize;
            WrapText(&wideChars[0], numChars, charWidths, lines, bounds, scale);
        }
    }

    ConstructMeshes(lines, bounds, scale);
}

void RndText::FitTextEllipsis() {
    BuildFontMaps(true);

    HX_VECTOR(unsigned short) wideChars;
    HX_VECTOR(Line) lines;
    auto textStr = mText.c_str();
    int numChars = ConvertTextToWide(textStr, wideChars);

    float *charWidths = (float *)_alloca((numChars + 2) * sizeof(float));
    OnComputeCharWidths(&wideChars[0], charWidths, false);

    float scale = 1.0f;
    Hmx::Rect bounds;

    if (!(charWidths[numChars] <= mWidth)) {
        // Text doesn't fit — need to truncate and add ellipsis
#ifdef HX_NATIVE
        int ellipsisLen = u16len(kEllipsisU16);
#else
        int ellipsisLen = wcslen(kEllipsisStr);
#endif

        // Binary search for how many chars fit
        int lo = 1;
        int hi = numChars;
        if (numChars > 2) {
            int tmpLo = lo;
            do {
                int mid = ((int)tmpLo + (int)hi) >> 1;
                lo = mid;
                if (charWidths[mid] >= mWidth) {
                    hi = mid;
                    lo = tmpLo;
                }
                tmpLo = lo;
            } while ((int)lo + 1 < (int)hi);
        }

        // Total length = truncated text + ellipsis
        int totalLen = lo + ellipsisLen;

        // Allocate buffer for truncated text + ellipsis
        unsigned short *buf =
            (unsigned short *)_alloca((totalLen + 1) * sizeof(unsigned short));
        memcpy(buf, &wideChars[0], lo * sizeof(unsigned short));

        // Respect mFixedLength if set
        if (ellipsisLen + 1 < mFixedLength && mFixedLength < totalLen) {
            totalLen = mFixedLength;
        }

        // Place ellipsis at the truncation point
        int truncPos = totalLen - ellipsisLen;
#ifdef HX_NATIVE
        u16cpy(&buf[truncPos], kEllipsisU16);
#else
        wcscpy((wchar_t *)&buf[truncPos], kEllipsisStr);
#endif

        BuildFontMaps(true);
        OnComputeCharWidths(buf, charWidths, false);
        WrapText(buf, totalLen, charWidths, lines, bounds, 1.0f);

        // Iteratively shrink if text still doesn't fit
#ifdef HX_NATIVE
        auto breakChar = u16chr(kBreakCharsU16, buf[truncPos - 1]);
#else
        auto breakChar = wcschr(kBreakChars, (wchar_t)buf[truncPos - 1]);
#endif
        while (truncPos > 1
            && (lines.size() > 1 || mWidth <= bounds.w
                || breakChar != 0)) {
            totalLen = totalLen - 1;
            // Try to find a space to break at (within ~87.5% of current length)
            int minPos = (int)totalLen * 0xe >> 4;
            if (minPos <= (int)totalLen - 1) {
                int searchPos = totalLen - 1;
                unsigned short *searchPtr = &buf[searchPos];
                while (true) {
                    if (*searchPtr == 0x20) {
                        // Found a space — break here
                        totalLen = searchPos + ellipsisLen;
                        break;
                    }
                    searchPos--;
                    searchPtr--;
                    if (minPos > searchPos)
                        break;
                }
            }

            truncPos = totalLen - ellipsisLen;
#ifdef HX_NATIVE
            u16cpy(&buf[truncPos], kEllipsisU16);
#else
            wcscpy((wchar_t *)&buf[truncPos], kEllipsisStr);
#endif
            BuildFontMaps(true);
            OnComputeCharWidths(buf, charWidths, false);
            WrapText(buf, totalLen, charWidths, lines, bounds, 1.0f);
        }
    } else {
        // Text fits, just wrap normally
        WrapText(&wideChars[0], numChars, charWidths, lines, bounds, 1.0f);
    }

    ConstructMeshes(lines, bounds, scale);
}

void RndText::FitTextScroll() {
    BuildFontMaps(true);

    HX_VECTOR(unsigned short) wideChars;
    HX_VECTOR(Line) lines;
    int numChars = ConvertTextToWide(mText.c_str(), wideChars);
    float *charWidths = (float *)_alloca((numChars + 2) * sizeof(float));

    mNumLines = 0;
    mLineWidths.clear();
    mLineOffsets.clear();

    OnComputeCharWidths(&wideChars[0], charWidths, false);

    mWrapEnabled = false;
    float scrollCharWidth = 0.0f;

    Hmx::Rect bounds;
    if ((mWidth > 0.0f && charWidths[numChars] > mWidth) || (mFitType == kFitScrollMarqueeWrapAlways)) {
        mWidth = 0.0f;
        mWrapEnabled = true;

        RndFontBase *font = mStyles[0].mFont;
        if (font == nullptr) {
            TheDebug.Fail(
                MakeString(kAssertStr, "Text.cpp", 2718, "font"),
                nullptr
            );
        } else {
            unsigned short charCode = 0;
            DecodeUTF8(charCode, "8");
            scrollCharWidth = (mStyles[0].mKerning + scrollCharWidth) * mStyles[0].mSize;
        }
    }

    WrapText(&wideChars[0], numChars, charWidths, lines, bounds, 1.0f);
    ConstructMeshes(lines, bounds, 1.0f);

    if (mWrapEnabled) {
        mWidth = bounds.w;
        mScrollCopies = 1;
        mScrollTimer = 0.0f;
        mScrollSpeed = (mScrollRate * scrollCharWidth) * -0.001f;

        if (mFitType == kFitScrollMarqueeWrapAlways) {
            mScrollPos = bounds.w;
            mNumLines = mNumLines + 1;
            mScrollOffset = bounds.w;
            mTotalWidth = (mIndentation * (float)mNumLines) + charWidths[numChars];
            mLineWidths.push_back(bounds.w);
            mLineOffsets.push_back(0.0f);
            mLineWidths.push_back(charWidths[numChars]);
            mLineOffsets.push_back(scrollCharWidth);
            mLineHeight = mTotalWidth;

            if (mTotalWidth > 0.0f) {
                float f = mTotalWidth;
                while (!(f > mWidth)) {
                    f += mTotalWidth;
                    mScrollCopies += 1;
                }
            }
            mCurScrollChars = -1;
            mScrollOutIndex = -1;
        } else {
            mScrollPos = 0.0f;
            mTotalWidth = charWidths[numChars];
            mLineHeight = 0.0f;
        }

        mScrollState = mScrollDelay;
        for (size_t i = 0; i < mFontMaps.size(); ++i) {
            mFontMaps[i]->SetupScrolling();
        }
    }
}

void RndText::DrawMesh(RndMesh *mesh, float size, int syncFlags) {
    mesh->DrawShowing();
    if (size != 0.0f && syncFlags > 0) {
        float offset = size;
        do {
            Vector3 pos = mesh->LocalXfm().v;
            pos.x += offset;
            mesh->SetLocalPos(pos);
            mesh->DrawShowing();
            pos.x -= offset;
            mesh->SetLocalPos(pos);
            syncFlags--;
            offset += size;
        } while (syncFlags != 0);
    }
}

RndText::FontMapBase *RndText::AcquireFontMap(RndFontBase *font) {
    Symbol fontMapClassName;

    if (font->ClassName() == RndFont::StaticClassName()) {
        fontMapClassName = FontMap::StaticClassName();
    } else if (font->ClassName() == RndFont3d::StaticClassName()) {
        fontMapClassName = FontMap3d::StaticClassName();
    } else {
        TheDebug.Fail(MakeString("Unknown Font type: %s", font->ClassName()), 0);
        fontMapClassName = FontMap::StaticClassName();
    }

    for (auto it = sFontMapCache.begin(); it != sFontMapCache.end(); ++it) {
        if ((*it)->Font() == font) {
            FontMapBase *cached = *it;
            sFontMapCache.erase(it);
            return cached;
        }
    }

    FontMapBase *result;

    auto _tmp9 = FontMap3d::StaticClassName();
    if (fontMapClassName == FontMap::StaticClassName()) {
        result = (FontMapBase *)MemAlloc(sizeof(FontMap), __FILE__, 0xd7, "FontMapBase", 0);
        if (result) {
            new (result) FontMap();
        }
    } else if (fontMapClassName == _tmp9) {
        result = (FontMapBase *)MemAlloc(sizeof(FontMap3d), __FILE__, 0xd7, "FontMapBase", 0);
        if (result) {
            new (result) FontMap3d();
        }
    } else {
        TheDebug.Fail(MakeString("Unknown FontMap type: %s", fontMapClassName), 0);
        result = (FontMapBase *)MemAlloc(sizeof(FontMap), __FILE__, 0xd7, "FontMapBase", 0);
        if (result) {
            new (result) FontMap();
        }
    }

    if (result) {
        result->SetFont(font);
        result->ResetDisplayableChars();
    }

    return result;
}

void RndText::ConstructMeshes(
    const HX_VECTOR(Line) &lines, const Hmx::Rect &bounds, float scale
) {
    // Store scale and number of lines
    mConstructScale = scale;
    mNumLinesRendered = lines.size();

    // Copy bounds using integer word copies (matching target codegen)
#ifdef HX_NATIVE
    mBoundsLeft = bounds.x;
    mBoundsTop = bounds.y;
    mBoundsRight = bounds.w;
    mBoundsBottom = bounds.h;
#else
    {
        const int *bsrc = (const int *)&bounds;
        int *bdst = (int *)&mBoundsLeft;
        bdst[0] = bsrc[0];
        bdst[1] = bsrc[1];
        bdst[2] = bsrc[2];
        bdst[3] = bsrc[3];
    }
#endif

    // Allocate meshes for each font map
    for (std::vector<FontMapBase *>::iterator it = mFontMaps.begin(); it != mFontMaps.end();
         ++it) {
        (*it)->AllocateMeshes(this, mFixedLength);
    }

    // Build character meshes if we have a font set
    if (mStyles[0].mFont != NULL) {
        StyleState state(this, scale);

        for (unsigned int i = 0; i < (unsigned int)lines.size(); i++) {
            const Line &line = lines[i];
            float yPos = line.mYPos;
            float xPos = line.mXStart;

            const unsigned short *cur = line.mStart;
            unsigned short prevChar = 0;
            int charIdx = 0;

            while (cur != line.mEnd && cur < line.mEnd) {
                unsigned short ch = *cur;

                if (ch == 0x3c && mMarkup) {
                    cur = ParseMarkup(cur, state, ch);
                    if (cur > line.mEnd) break;
                    cur--; // compensate for cur++ at end of loop
                }

                if (ch != 0) {
                    FontMapBase *fontMap = mFontMaps[state.mFontMapIdx];
                    fontMap->SetupCharacter(
                        ch,
                        xPos,
                        yPos,
                        state,
                        prevChar,
                        mCircle,
                        mFitType,
                        mIndentation
                    );
                    prevChar = ch;
                    charIdx++;
                }

                cur++;
            }
        }
    }

    // Cleanup and sync meshes
    for (std::vector<FontMapBase *>::iterator it = mFontMaps.begin(); it != mFontMaps.end();
         ++it) {
        (*it)->CleanupSyncMeshes();
    }
}

const unsigned short *
RndText::ParseMarkup(const unsigned short *str, StyleState &state, unsigned short &ch) {
    const unsigned short *cur = str;
    unsigned int isClosing = (unsigned int)(*++cur - 0x2f) == 0;
    if (isClosing) {
        cur++;
    }
    ch = 0;

    float fVar12;
    auto& _ref0 = mStyles;
#ifdef HX_NATIVE
    if (WStrniCmp(cur, kTag_sup, 3) == 0) {
#else
    if (WStrniCmp(cur, (const unsigned short *)L"sup", 3) == 0) {
#endif
        cur += 3;
        if (isClosing) {
            fVar12 = state.mStyle->mSize;
        } else {
            fVar12 = state.mStyle->mSize * gSuperscriptScale;
        }
        goto set_size;
    }
#ifdef HX_NATIVE
    else if (WStrniCmp(cur, kTag_gtr, 3) == 0) {
#else
    else if (WStrniCmp(cur, (const unsigned short *)L"gtr", 3) == 0) {
#endif
        cur += 3;
        Style *style = state.mStyle;
        float scale;
        if (isClosing) {
            scale = style->mSize;
        } else {
            scale = style->mSize * gGuitarScale;
        }
        state.mSize = state.mBaseSize * scale;
        float zOff = gGuitarZOffset;
        if (isClosing) {
            zOff = style->mZOffset;
        }
        state.mZOffset = zOff;
    }
#ifdef HX_NATIVE
    else if (WStrniCmp(cur, kTag_it, 2) == 0) {
#else
    else if (WStrniCmp(cur, (const unsigned short *)L"it", 2) == 0) {
#endif
        cur += 2;
        if (isClosing) {
            state.mItalics = state.mStyle->mItalics;
        } else {
            state.mItalics = state.mStyle->mItalics + 0.1f;
        }
    }
#ifdef HX_NATIVE
    else if (WStrniCmp(cur, kTag_color, 5) == 0) {
#else
    else if (WStrniCmp(cur, (const unsigned short *)L"color", 5) == 0) {
#endif
        cur += 5;
        if (isClosing) {
                        state.mTextColor = state.mStyle->mTextColor;
        } else {
            int r = 0, g = 0, b = 0;
            int a = (int)(state.mTextColor.alpha * 255.999f);
            cur++;
#ifdef HX_NATIVE
            int rgba[4] = {0, 0, 0, a};
            u16_scan_ints(cur, rgba, 4);
            r = rgba[0]; g = rgba[1]; b = rgba[2]; a = rgba[3];
#else
            swscanf((const wchar_t *)cur, L"%d %d %d %d", &r, &g, &b, &a);
#endif
            state.mTextColor.blue = (float)b * (1.0f / 255.0f);
            state.mTextColor.green = (float)g * (1.0f / 255.0f);
            state.mTextColor.red = (float)r * (1.0f / 255.0f);
            state.mTextColor.alpha = (float)a * (1.0f / 255.0f);
        }
    }
#ifdef HX_NATIVE
    else if (WStrniCmp(cur, kTag_hash, 2) == 0) {
#else
    else if (WStrniCmp(cur, (const unsigned short *)L"#", 2) == 0) {
#endif
        cur += 2;
        int code = 0x3f;
#ifdef HX_NATIVE
        u16_scan_ints(cur, &code, 1);
#else
        swscanf((const wchar_t *)cur, L"%d", &code);
#endif
        ch = (unsigned short)code;
    }
#ifdef HX_NATIVE
    else if (WStrniCmp(cur, kTag_nobreak, 7) == 0) {
#else
    else if (WStrniCmp(cur, (const unsigned short *)L"nobreak", 7) == 0) {
#endif
        cur += 7;
        if (isClosing) {
            state.brk = true;
        } else {
            state.brk = false;
        }
    }
#ifdef HX_NATIVE
    else if (WStrniCmp(cur, kTag_alt, 3) == 0) {
#else
    else if (WStrniCmp(cur, (const unsigned short *)L"alt", 3) == 0) {
#endif
        cur += 3;
        bool bBlacklight = false;
        unsigned int styleIdx = 1;

        if (isClosing) {
            unsigned short scanChar = *cur;
            while (scanChar != 0x3e && scanChar != 0) {
                cur++;
                scanChar = *cur;
            }
        }

        unsigned short markupChar = *cur;
        if ((markupChar >= 0x32) && (markupChar <= 0x39)) {
            styleIdx = markupChar - 0x30;
            cur++;
        } else if ((markupChar == 0x62) || (markupChar == 0x42)) {
            bBlacklight = true;
            styleIdx = (1 < (unsigned int)_ref0.size()) ? 1 : 0;
            Style *fallback = &_ref0[0];
            Style *stylePtr = &_ref0[styleIdx];
            if (stylePtr->mFont != nullptr) {
                fallback = stylePtr;
            }
            RndFontBase *font = fallback->mFont;
            if (font != nullptr) {
                if (FontMapIndex(font, true) == -1) {
                    FontMapBase *fm = AcquireFontMap(font);
                    fm->mBlacklight = true;
                    mFontMaps.push_back(fm);
                }
            }
            cur++;
        }

        styleIdx = styleIdx & -(isClosing == 0);

        unsigned int numStyles = (unsigned int)_ref0.size();
        if (styleIdx < numStyles) {
            state.mStyle = &_ref0[styleIdx];
        } else {
            state.mStyle = &_ref0[0];
        }

        memcpy(&state, state.mStyle, 0x34);

        bool bFontColorOverride = state.mFontColorOverride || bBlacklight;
        if (!state.mStyle->mFont) {
            state.mStyle = &_ref0[0];
        }
        state.mFontMapIdx = FontMapIndex(state.mStyle->mFont, bFontColorOverride);

        fVar12 = state.mSize;
        goto set_size;
    }

    goto scan_close;

set_size:
    state.mSize = state.mBaseSize * fVar12;
scan_close:
    {
        short scanChar = *cur;
        while (scanChar != 0x3e && scanChar != 0) {
            cur++;
            scanChar = *cur;
        }
        if (scanChar == 0x3e) {
            cur++;
        }
    }
    return cur;
}

void RndText::UpdateText() {
    if (mFitType == kFitEllipsis) {
        FitTextJust();
        return;
    }
    if (mStyles[0].mSize > 0.0f && mWidth > 0.0f) {
        if (mFitType == kFitStretch) {
            FitTextEllipsis();
            return;
        }
        if (mFitType == kFitScrollPingPong
            || mFitType == kFitScrollMarqueeReset
            || mFitType == kFitScrollMarqueeWrap
            || mFitType == kFitScrollMarqueeWrapAlways) {
            for (unsigned int i = 0; i < (unsigned int)mStyles.size(); i++) {
                RndFontBase *font = mStyles[i].mFont;
                const char *fontName;
                if (font != 0) {
                    if (font->ClassName() != RndFont::StaticClassName()) {
                        fontName = font->Name();
                    } else {
                        continue;
                    }
                } else {
                    fontName = "NULL";
                }
                MILO_NOTIFY(
                    "%s %s requests scrolling, but uses a font that does not support it (%s)",
                    PathName(this), Name(), fontName
                );
                mFitType = kFitStretch;
                FitTextEllipsis();
                return;
            }
            FitTextScroll();
            return;
        }
    }
    // Normal wrap path
    {
        HX_VECTOR(Line) lines;
        BuildFontMaps(true);
        HX_VECTOR(unsigned short) wideChars;
        int numChars = ConvertTextToWide(mText.c_str(), wideChars);
        float *charWidths = (float *)_alloca((numChars + 2) * sizeof(float));
        OnComputeCharWidths(&wideChars[0], charWidths, false);
        Hmx::Rect bounds;
        WrapText(&wideChars[0], numChars, charWidths, lines, bounds, 1.0f);
        ConstructMeshes(lines, bounds, 1.0f);
    }
}

void RndText::DrawShowing() {
    SizeCheck();


    // Count total materials across all font maps for VLA allocation
    int totalMats = 0;
    for (auto it = mFontMaps.begin(); it != mFontMaps.end(); ++it) {
        totalMats += (*it)->NumMaterials();
    }

    // Allocate VLA on stack to save material colors (one Hmx::Color per material)
    Hmx::Color *savedColors = (Hmx::Color *)_alloca(totalMats * sizeof(Hmx::Color));

    // Save material colors
    int vlaIdx = 0;
    for (auto it = mFontMaps.begin(); it != mFontMaps.end(); ++it) {
        FontMapBase *fontMap = *it;
        for (int i = 0; i < fontMap->NumMaterials(); i++) {
            RndMat *mat = fontMap->Material(i);
            savedColors[vlaIdx] = mat->GetColor();
            vlaIdx++;
        }
    }

    // Apply font color overrides from styles
    bool hasOverride = false;
    auto stylesEnd = mStyles.end();
    for (auto it = mStyles.begin(); it != stylesEnd; ++it) {
        Style &style = *it;
        if (style.mFont && style.mFontColorOverride) {
            int fmIdx = FontMapIndex(style.mFont, style.mBlacklight);
            if (fmIdx != -1) {
                hasOverride = true;
                FontMapBase *fontMap = mFontMaps[fmIdx];
                int numMats = fontMap->NumMaterials();
                for (int i = 0; i < numMats; i++) {
                    RndMat *mat = fontMap->Material(i);
                    mat->GetColor() = style.mFontColor;
                    mat->MarkDirty(1);
                }
            }
        }
    }

    // Update scroll offsets if wrapping is enabled
    if (mWrapEnabled) {
        UpdateScrollOffsets();
    }

    // Draw each mesh — text inherits the current camera (PanelDir's CamOverride).
    // On Xbox, text was drawn in 3D world space under the active camera.
    for (auto it = mFontMaps.begin(); it != mFontMaps.end(); ++it) {
        FontMapBase *fontMap = *it;
        int numMeshes = fontMap->NumMeshes();
        for (int i = 0; i < numMeshes; i++) {
            RndMesh *mesh = fontMap->Mesh(i);
            if (mesh) {
                auto blacklightDisabled = TheUI ? TheUI->DisableScreenBlacklight() : true;
                if (!sBlacklightModeEnabled || !fontMap->mBlacklight ||
                    blacklightDisabled) {
                    DrawMesh(mesh, mStyles[0].mSize, 0);
                } else {
                    QueueBlacklightPacket(mesh, mStyles[0].mSize, 0);
                }
            }
        }
    }

    // Restore material colors (r, g, b only — not alpha)
    if (hasOverride) {
        vlaIdx = 0;
        auto fontMapsEnd = mFontMaps.end();
        for (auto it = mFontMaps.begin(); it != fontMapsEnd; ++it) {
            FontMapBase *fontMap = *it;
            auto numMaterials = fontMap->NumMaterials();
            for (int i = 0; i < numMaterials; i++) {
                RndMat *mat = fontMap->Material(i);
                Hmx::Color &color = mat->GetColor();
                color.red = savedColors[vlaIdx].red;
                color.green = savedColors[vlaIdx].green;
                color.blue = savedColors[vlaIdx].blue;
                mat->MarkDirty(1);
                vlaIdx++;
            }
        }
    }
}

void RndText::SizeCheck() {
#ifdef HX_NATIVE
    UpdateText();
#else
    static float sLastHeight;
    static RndText *sLastText;

    StyleState ss(this, mConstructScale);
    for (FontMapBase **it = mFontMaps.begin(); it != mFontMaps.end(); ++it) {
        RndFontBase *font = (*it)->Font();
        if (font != nullptr && font->BitmapFont()) {
            for (int i = 0; i < (*it)->NumMeshes(); i++) {
                RndMesh *mesh = (*it)->Mesh(i);
                if (mesh != nullptr) {
                    float screenHeight;
                    if (!CalcScreenHeight(
                            ss.mSize * font->AspectRatio(), mesh, screenHeight
                        )) {
                        return;
                    }
                    float fontUnit = font->FontUnit();
                    float aspectRatio = font->AspectRatio();
                    float cap = 127.5f;
                    if (screenHeight < 127.5f) {
                        cap = screenHeight;
                    }
                    if (cap <= fontUnit * aspectRatio * 1.25f) {
                        return;
                    }
                    if (sLastText == this && screenHeight <= sLastHeight) {
                        return;
                    }
                    int heightInt = (int)screenHeight;
                    int productInt = (int)(fontUnit * aspectRatio);
                    MILO_NOTIFY(
                        "oversized: %s font: %s token:'%s' text:'%s' %d < %d",
                        PathName(this),
                        font->Name(),
                        TextToken(),
                        mText,
                        productInt,
                        heightInt
                    );
                    sLastHeight = screenHeight;
                    sLastText = this;
                    return;
                }
            }
        }
    }
#endif
}

void RndText::GetWidthHeightBox(Box &box) const {
    if (mAlignment & 1) {
        box.mMin.x = 0;
    } else if (mAlignment & 2) {
        box.mMin.x = mWidth * -0.5f;
    } else {
        box.mMin.x = -mWidth;
    }

    if (mAlignment & 0x10) {
        box.mMin.z = -mHeight;
    } else if (mAlignment & 0x20) {
        box.mMin.z = mHeight * -0.5f;
    } else {
        box.mMin.z = 0;
    }

    box.mMax.x = mWidth + box.mMin.x;
    box.mMax.z = mHeight + box.mMin.z;
    box.mMax.y = 0;
    box.mMin.y = 0;
}

void RndText::ReFitTextScroll(String str) {
    auto& maxWidth = mWidth;
    if (mFitType != kFitScrollMarqueeWrapAlways) {
        return;
    }
    SetText(str.c_str());
    FitTextScroll();
    mScrollPos = 0.0f;
    mScrollOffset = 0.0f;
    float width = maxWidth;
    while (width >= *mLineWidths.begin()) {
        mCurScrollChars++;
        if (mCurScrollChars >= mNumLines) {
            mCurScrollChars = 0;
        }
        if (*mLineWidths.begin() == mTotalWidth) {
            mScrollOffset += maxWidth;
        }
        if ((unsigned int)mNumLines == (unsigned int)mLineWidths.size()) {
            mLineWidths.insert(mLineWidths.end(), *mLineWidths.begin());
        }
        mLineWidths.erase(mLineWidths.begin());
        width = maxWidth - mScrollOffset;
    }
    mScrollTimer = mScrollState;
}

float RndText::ComputeCharWidthsForText(String str) {
    BuildFontMaps(false);
#ifdef HX_NATIVE
    std::vector<unsigned short> wideChars;
#else
    std::vector<unsigned short, std::StlNodeAlloc<unsigned short> > wideChars;
#endif
    int numChars = ConvertTextToWide(str.c_str(), wideChars);
    float *widths = (float *)_alloca((numChars + 2) * sizeof(float));
    OnComputeCharWidths(wideChars.data(), widths, true);
    return widths[numChars];
}

void RndText::FontMap3d::IncrementDisplayableChars(unsigned short us) {
    RndFont3d::CharInfo *info = mFont->GetCharInfo(us);
    if (info != nullptr && info->mMesh != nullptr) {
        mDisplayableChars++;
    }
}

void RndText::FontMap3d::AllocateMeshes(RndText *text, int fixedLength) {
    unsigned int targetSize = 0;
    if (mFont != NULL) {
        targetSize = fixedLength;
        if (fixedLength == 0) {
            targetSize = mDisplayableChars;
        }
    }

    unsigned int oldSize = (unsigned int)mMeshes.size();

    if (targetSize < oldSize) {
        unsigned int i = targetSize;
        do {
            RndMesh *mesh = mMeshes[i];
            if (mesh != NULL) {
                delete mesh;
            }
            i++;
        } while (i < (unsigned int)mMeshes.size());
    }

    RndMesh *nullMesh = NULL;

    if (targetSize < oldSize) {
        mMeshes.erase(mMeshes.begin() + targetSize, mMeshes.end());
    } else {
        mMeshes.insert(mMeshes.end(), (int)targetSize - (int)oldSize, nullMesh);
    }

    if ((unsigned int)mMeshes.size() > 0) {
        unsigned int i = 0;
        do {
            if ((int)i >= (int)oldSize) {
                mMeshes[i] = Hmx::Object::New<RndMesh>();
            }
            RndMesh *mesh = mMeshes[i];
            RndTransformable *parent = NULL;
            if (text != NULL) {
                parent = text;
            }
            mesh->SetTransParent(parent, false);
            mesh->SetTransConstraint(RndTransformable::kConstraintNone, NULL, false);
            mesh->SetMat(mFont->Mat());
            mesh->SetShowing(true);
            i++;
        } while (i < (unsigned int)mMeshes.size());
    }

    mMeshCursor = mMeshes.data();
}

void RndText::FontMap3d::CleanupSyncMeshes() {
#ifdef HX_NATIVE
    if (mMeshes.empty())
        return;
#endif
    for (; mMeshCursor != &mMeshes.back() + 1; mMeshCursor++) {
        (*mMeshCursor)->SetShowing(false);
    }
}

void RndText::FontMap::SetupCharacter(
    unsigned short charCode,
    float &xPos,
    float yPos,
    const StyleState &state,
    unsigned short prevChar,
    float circle,
    FitType fitType,
    float leading
) {
    if ((fitType == kFitScrollMarqueeWrapAlways) && ((charCode & 0xffff) == 10)) {
        xPos = leading + xPos;
        return;
    }

    int page = mFont->CharPage(charCode);
    if (page < 0) return;
    Page &pg = *(mPages[page]);

    float charW, advW;
    if (!mFont->CharWidthAdvanceCoords(charCode, charW, advW, pg.mVertStart[0].tex, pg.mVertStart[2].tex)) {
        return;
    }

    xPos += (mFont->Kerning(prevChar, charCode) + state.mKerning) * state.mSize;

    float width = charW;
    if (charW <= 0.0f) {
        width = advW;
    }

    float centerOfs = 0.0f;
    if (mFont->IsMonospace()) {
        centerOfs = Max((advW - width) * 0.5f, 0.0f);
    }

    float scaledCenter = state.mSize * centerOfs;
    float scaledW = state.mSize * width;
    if (scaledW <= 0.0f) return;

    float z0 = yPos + state.mZOffset * state.mSize;
    auto _tmp1 = mFont->AspectRatio();
    float italics = state.mItalics * state.mSize;
    float x = xPos;
    float z1 = z0 - _tmp1 * state.mSize;

    pg.mVertStart[0].pos.Set(italics + scaledCenter + x, 0.0f, z0);
    pg.mVertStart[1].pos.Set(scaledCenter + x - italics, 0.0f, z1);
    pg.mVertStart[2].pos.Set(scaledCenter + x - italics + scaledW, 0.0f, z1);
    pg.mVertStart[3].pos.Set(italics + scaledCenter + scaledW + x, 0.0f, z0);

    if (circle != 0.0f) {
        float midX = (pg.mVertStart[3].pos.x - pg.mVertStart[1].pos.x) * 0.5f
            + pg.mVertStart[1].pos.x;
        Transform xfm = XfmOnCircleEdge(circle, midX);
        xfm.v.x -= xfm.m.x.x * midX;
        xfm.v.y -= xfm.m.x.y * midX;
        xfm.v.z -= xfm.m.x.z * midX;
        Multiply(pg.mVertStart[0].pos, xfm, pg.mVertStart[0].pos);
        Multiply(pg.mVertStart[1].pos, xfm, pg.mVertStart[1].pos);
        Multiply(pg.mVertStart[2].pos, xfm, pg.mVertStart[2].pos);
        Multiply(pg.mVertStart[3].pos, xfm, pg.mVertStart[3].pos);
    }

    pg.mVertStart[1].tex.y = pg.mVertStart[2].tex.y;
    pg.mVertStart[1].tex.x = pg.mVertStart[0].tex.x;
    pg.mVertStart[3].tex.x = pg.mVertStart[2].tex.x;
    pg.mVertStart[3].tex.y = pg.mVertStart[0].tex.y;

    pg.mVertStart[0].norm.Set(0.0f, -1.0f, 0.0f);
    pg.mVertStart[3].norm = pg.mVertStart[0].norm;
    pg.mVertStart[2].norm = pg.mVertStart[3].norm;
    pg.mVertStart[1].norm = pg.mVertStart[2].norm;

    pg.mVertStart[3].color = state.mTextColor;
    pg.mVertStart[2].color = pg.mVertStart[3].color;
    pg.mVertStart[1].color = pg.mVertStart[2].color;
    pg.mVertStart[0].color = pg.mVertStart[1].color;

    pg.mVertStart += 4;

    xPos = advW * state.mSize + xPos;
}

void RndText::FontMap3d::SetupCharacter(
    unsigned short charCode,
    float &xPos,
    float yPos,
    const StyleState &state,
    unsigned short prevChar,
    float size,
    FitType fitType,
    float leading
) {
    float width, advance;
    RndMesh *charMesh;
    if (!mFont->CharWidthAdvanceMesh(charCode, width, advance, &charMesh))
        return;

    // Apply kerning + style kerning
    xPos += (mFont->Kerning(prevChar, charCode) + state.mKerning) * state.mSize;

    // Use advance as display width if width <= 0
    if (width <= 0.0f) {
        width = advance;
    }

    // Monospace centering
    float centerOffset = 0.0f;
    if (mFont->IsMonospace()) {
        centerOffset = Max((advance - width) * 0.5f, 0.0f);
    }

    float scaledWidth = state.mSize * width;
    float scaledCenter = state.mSize * centerOffset;

    if (scaledWidth <= 0.0f)
        return;

    yPos += state.mZOffset * state.mSize;

    if (charMesh && mMeshCursor != mMeshes.end()) {
        RndMesh *mesh = *mMeshCursor;
        mMeshCursor++;
        mesh->SetGeomOwner(charMesh);

        // Copy origin to transform position, then scale in-place
        Vector3 origin = mFont->CharOriginOffset();

        Transform xfm;
        xfm.v = origin;
        xfm.v.x = xfm.v.x * state.mSize + scaledCenter + xPos;
        xfm.v.y *= state.mSize;
        xfm.v.z = xfm.v.z * state.mSize + yPos;

        // Scale matrix by cell height
        float cellHeight = mFont->FontUnitInverse() * state.mSize;
        xfm.m.x.Set(cellHeight, 0.0f, 0.0f);
        xfm.m.y.Set(0.0f, cellHeight, 0.0f);
        xfm.m.z.Set(0.0f, 0.0f, cellHeight);

        if (size != 0.0f) {
            float circlePos = scaledWidth * 0.5f + xfm.v.x;
            Transform circleXfm = XfmOnCircleEdge(circlePos, size);
            xfm.v.x -= circlePos;
            Multiply(xfm, circleXfm, xfm);
        }

        memcpy(&mesh->mWorldXfm, &xfm, sizeof(Transform));
        if (!mesh->mDirty) {
            mesh->SetDirty_Force();
        }
    }

    xPos += state.mSize * advance;
}

#ifndef HX_NATIVE
// Template instantiation for map<RndFontBase*, set<unsigned short>>
#include <map>
#include <set>
#include "utl/StlAlloc.h"
namespace stlpmtx_std {
typedef set<unsigned short, less<unsigned short>, StlNodeAlloc<unsigned short> > _FontCharSet;
typedef pair<RndFontBase* const, _FontCharSet> _FontMapValue;
template class _Rb_tree<RndFontBase*,
    less<RndFontBase*>,
    _FontMapValue,
    _Select1st<_FontMapValue>,
    priv::_MapTraitsT<_FontMapValue>,
    StlNodeAlloc<_FontMapValue> >;
}
#endif
