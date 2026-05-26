#include "rndobj/Graph.h"
#include "Rnd.h"
#include "obj/DataFunc.h"
#include "os/Debug.h"
#include "rndobj/Utl.h"
#include "utl/MemMgr.h"

static RndGraph *sOneFrame = nullptr;
static std::list<RndGraph *> *sGraphs = nullptr;
static std::list<FakeGraph> sFakes;
static ObjPtr<RndCam> sCam(nullptr);

void *Drawable::operator new(size_t s) {
    return MemAlloc(s, __FILE__, 0xA9, "Drawable");
}
void Drawable::operator delete(void *v) { MemFree(v, __FILE__, 0xA9, "Drawable"); }
void ScreenLine::Draw() { UtilDrawLine(mA, mB, mCol); }
void ScreenLine::DrawFixedZ(float) { UtilDrawLine(mA, mB, mCol); }
void RectFilled2D::Draw() {
    TheRnd.DrawRectScreen(mRect, mCol, nullptr, nullptr, nullptr);
}
void RectFilled2D::DrawFixedZ(float) {
    TheRnd.DrawRectScreen(mRect, mCol, nullptr, nullptr, nullptr);
}
void DrawSphere::Draw() { UtilDrawSphere(mCenter, mRadius, mCol, nullptr); }
void DrawString::Draw() { TheRnd.DrawString(mText.c_str(), mPos, mCol, true); }
void DrawString::DrawFixedZ(float) { Draw(); }
void DrawString3D::Draw() { UtilDrawString(mText.c_str(), mPos, mCol); }
void DrawString3D::DrawFixedZ(float) { Draw(); }
void Line::Draw() { TheRnd.DrawLine(mA, mB, mCol, mZBuf); }
void Line::DrawFixedZ(float f) {
    TheRnd.DrawLine(Vector3(mA.x, mA.y, f), Vector3(mB.x, mB.y, f), mCol, mZBuf);
}
void DrawSphere::DrawFixedZ(float f) {
    UtilDrawSphere(Vector3(mCenter.x, mCenter.z, f), mRadius, mCol, nullptr);
}

void RndGraph::Draw() {
    for (int i = 0; i < mStuff.size(); i++) {
        if (mDrawFixedZ) {
            mStuff[i]->DrawFixedZ(mZ);
        } else
            mStuff[i]->Draw();
    }
}

void RndGraph::SetCamera(RndCam *c) { sCam = c; }

RndGraph *RndGraph::GetOneFrame() {
    if (sOneFrame == nullptr)
        sOneFrame = new RndGraph(nullptr);
    return sOneFrame;
}

RndGraph *RndGraph::Get(const void *v) {
    for (std::list<RndGraph *>::iterator it = sGraphs->begin(); it != sGraphs->end();
         ++it) {
        if ((*it)->mId == v)
            return *it;
    }
    RndGraph *graph = new RndGraph(v);
    sGraphs->push_back(graph);
    return graph;
}

void RndGraph::AddLine(
    const Vector3 &a, const Vector3 &b, const Hmx::Color &col, bool buf
) {
    if (mEnable)
        mStuff.push_back(new Line(a, b, col, buf));
}

void RndGraph::AddScreenLine(
    const Vector2 &a, const Vector2 &b, const Hmx::Color &col, bool buf
) {
    if (mEnable)
        mStuff.push_back(new ScreenLine(a, b, col, buf));
}

void RndGraph::AddRectFilled2D(const Hmx::Rect &r, const Hmx::Color &c) {
    if (mEnable)
        mStuff.push_back(new RectFilled2D(r, c));
}

void RndGraph::AddSphere(const Vector3 &center, float radius, const Hmx::Color &col) {
    if (mEnable)
        mStuff.push_back(new DrawSphere(center, radius, col));
}

void RndGraph::AddString(const char *cc, const Vector2 &vec, const Hmx::Color &col) {
    if (mEnable)
        mStuff.push_back(new DrawString(cc, vec, col));
}

void RndGraph::AddString3D(
    const char *text, const Vector3 &worldPos, const Hmx::Color &col
) {
    if (mEnable)
        mStuff.push_back(new DrawString3D(text, worldPos, col));
}

void RndGraph::AddScreenString(const char *cc, const Vector2 &v, const Hmx::Color &c) {
    if (mEnable) {
        mStuff.push_back(
            new DrawString(cc, Vector2(v.x * TheRnd.Width(), v.y * TheRnd.Height()), c)
        );
    }
}

void RndGraph::Reset() {
    if (mEnable) {
        for (int i = 0; i < mStuff.size(); i++) {
            delete mStuff[i];
        }
        mStuff.clear();
    }
}

void RndGraph::DrawAll() {
    if (sCam)
        sCam->Select();
    auto _tmp0 = sGraphs->end();
    for (std::list<RndGraph *>::iterator it = sGraphs->begin(); _tmp0 != it;
         ++it) {
        (*it)->Draw();
    }
    if (sOneFrame) {
        sOneFrame->Draw();
        sOneFrame->Reset();
    }
    for (std::list<FakeGraph>::iterator it = sFakes.begin(); sFakes.end() != it; ++it) {
        it->mDrawCallback();
    }
    sCam = nullptr;
}

RndGraph::~RndGraph() {
    mEnable = true;
    Reset();
}

void RndGraph::ResetAll() {
    RELEASE(sOneFrame);
    while (!sGraphs->empty()) {
        RndGraph *cur = sGraphs->front();
        sGraphs->pop_front();
        delete cur;
    }
}

void RndGraph::Free(const void *id, bool notify) {
    if (sGraphs == nullptr)
        return;
    for (std::list<RndGraph *>::iterator it = sGraphs->begin(); it != sGraphs->end();
         ++it) {
        if ((*it)->mId == id) {
            delete *it;
            sGraphs->erase(it);
            return;
        }
    }
    for (std::list<FakeGraph>::iterator it = sFakes.begin(); it != sFakes.end(); ++it) {
        if (it->mId == id) {
            sFakes.erase(it);
            return;
        }
    }
    if (notify)
        MILO_NOTIFY("could not find line graph %x to free\n", (int)id);
}

DataNode OnGraphReset(DataArray *) {
    RndGraph::ResetAll();
    return 0;
}

void RndGraph::Terminate() {
    std::list<RndGraph*> *temp = sGraphs;
    if (temp != nullptr) {
        temp->clear();
        delete temp;
    }
    sGraphs = nullptr;
    if (sOneFrame != nullptr) {
        delete sOneFrame;
    }
    sOneFrame = nullptr;
}

void RndGraph::Init() {
    sGraphs = new std::list<RndGraph *>;
    TheDebug.AddExitCallback(RndGraph::Terminate);
    DataRegisterFunc("graph_reset", OnGraphReset);
}
