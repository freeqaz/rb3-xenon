#pragma once
#include "obj/Data.h"
#include "obj/Object.h"
#include "rndobj/Highlight.h"
#include "rndobj/Trans.h"
#include "math/Geo.h"
#include "math/Mtx.h"
#include "math/Sphere.h"
#include "utl/MemMgr.h"

class RndCam;

// Retail X360 RB3 (rev-11-era) RndDrawable own-vtable slice has TWO fewer slots
// than DC3's newer Draw.h, proven by independent machine-code anchors:
//   * the mMesh->DrawShowing() vcall (RndLine::DrawShowing, SpotlightDrawer) is
//     at target slot 0x14 vs our 0x18 (+4 = one dropped slot before DrawShowing)
//   * the (*it)->CollideList() vcall (RndGroup::CollideList) is at target slot
//     0x24 vs our 0x2c (+8 = two dropped slots before CollideList)
// The two dropped DC3 virtuals are:
//   1. Draw() is NON-VIRTUAL in retail. Smoking gun: retail RndDir::DrawShowing
//      emits `bl fn_823F3A80` (direct call) for `(*it)->Draw()` where a vcall
//      would appear if Draw were virtual; fn_823F3A80 is the single cull-wrapper
//      Draw body (tests mShowing@0x8, frustum-culls, vcalls DrawShowing@0x14).
//      Every Draw call site in the binary is a direct bl to that one body.
//   2. DrawShadow(const Transform&, float) is DC3-only. rb3-Wii has the unrelated
//      DrawShowingBudget there; retail-360 has neither (else CollideList=0x28).
// Both are called directly in retail (RndGroup::DrawShowing -> child->Draw();
// SpotlightDrawer::DrawShadow -> draw->DrawShadow), so dropping the `virtual`
// keyword keeps them callable. Subclass Draw() overrides (RndGroup/RndEnviron/
// CharClipSet/HamCharacter) become non-virtual hiding functions: they don't sit
// in the vtable and are never reached through a RndDrawable* (the direct bl
// always hits RndDrawable::Draw), so the layout is correct without relocating
// their bodies. The native engine needs virtual dispatch, so gate the keyword
// behind HX_NATIVE (same idiom as ANIM_DC3_VIRTUAL / PROPKEYS_DC3_VIRTUAL).
// See docs/decomp/research/2026-06-10-force-multipliers.md (RndDrawable section).
#ifdef HX_NATIVE
#define DRAW_DC3_VIRTUAL virtual
#else
#define DRAW_DC3_VIRTUAL
#endif

enum HighlightStyle {
    kHighlightWireframe,
    kHighlightSphere,
    kHighlightNone,
    kHighlightWireframeWithNormals,
    kNumHighlightStyles
};

/**
 * @brief An object that is drawable.
 * Original _objects description:
 * "Base class for drawable objects. Draw objects either
 * render polys or determine rendering state."
 */
class RndDrawable : public virtual RndHighlightable {
public:
    struct Collision {
        Collision() {}
        Collision(RndDrawable *o, float d, const Plane &p)
            : object(o), distance(d), plane(p) {}
        RndDrawable *object; // offset 0x0, size 0x4
        float distance; // offset 0x4, size 0x4
        Plane plane; // offset 0x10, size 0x10
    };

    OBJ_CLASSNAME(Draw);
    OBJ_SET_TYPE(Draw);
    virtual DataNode Handle(DataArray *, bool);
    virtual bool SyncProperty(DataNode &, DataArray *, int, PropOp);
    virtual void Save(BinStream &);
    virtual void Copy(const Hmx::Object *, Hmx::Object::CopyType);
    virtual void Load(BinStream &);
    /** "Calculates the bounding sphere for the object." */
    virtual void UpdateSphere() { mSphere.Zero(); }
    virtual float GetDistanceToPlane(const Plane &, Vector3 &) { return 0; }
    virtual bool MakeWorldSphere(Sphere &s, bool) { return false; }
    /** Get the current camera to use. */
    virtual RndCam *CamOverride() { return 0; }
    virtual void Mats(std::list<class RndMat *> &, bool) {}
    DRAW_DC3_VIRTUAL void Draw();
    virtual void DrawShowing() {}
    DRAW_DC3_VIRTUAL void DrawShadow(const Transform &light, float shadowPlane) {}
    /** Get the list of this Object's children that are drawable. */
    virtual void ListDrawChildren(std::list<RndDrawable *> &) {}
    virtual RndDrawable *CollideShowing(const Segment &s, float &dist, Plane &plane) {
        return nullptr;
    }
    virtual int CollidePlane(const Plane &);
    virtual void CollideList(const Segment &, std::list<Collision> &);
    virtual void DrawPreClear() {}
    virtual void UpdatePreClearState() {}
    virtual void Highlight();

    OBJ_MEM_OVERLOAD(0x25);
    NEW_OBJ(RndDrawable);
    static void Init() { REGISTER_OBJ_FACTORY(RndDrawable); }

    void SetShowing(bool b) { mShowing = b; }
    bool Showing() const { return mShowing; }
    void SetOrder(float order) { mOrder = order; }
    float GetOrder() const { return mOrder; }
    RndDrawable *Collide(const Segment &, float &, Plane &);
    bool CollideSphere(const Segment &);
    void SetSphere(const Sphere &s) { mSphere = s; }
    const Sphere &GetSphere() const { return mSphere; }

    static void DumpLoad(BinStream &bs);
    static HighlightStyle GetHighlightStyle() { return sHighlightStyle; }
    static void SetHighlightStyle(HighlightStyle hs) { sHighlightStyle = hs; }
    static float GetNormalDisplayLength() { return sNormalDisplayLength; }
    static void SetNormalDisplayLength(float f) { sNormalDisplayLength = f; }
    static bool GetForceSubpartSelection() { return sForceSubpartSelection; }
    static void SetForceSubpartSelection(bool b) { sForceSubpartSelection = b; }

protected:
    RndDrawable();

    static HighlightStyle sHighlightStyle;
    static float sNormalDisplayLength;
    static bool sForceSubpartSelection;

    /** Handler to copy another RndDrawable's sphere to this one's.
     * @param [in] arr The supplied DataArray.
     * Expected DataArray contents:
     *     Node 2: the other RndDrawable.
     * Example usage: {$this copy_sphere other_obj}
     */
    DataNode OnCopySphere(const DataArray *arr);
    /** Handler to retrieve this RndDrawable's sphere properties.
     * @param [in] arr The supplied DataArray.
     * Expected DataArray contents:
     *     Nodes 2-5: vars to house this sphere's center X/Y/Z coordinates and radius.
     * Example usage: {$this get_sphere $x $y $z $radius}
     */
    DataNode OnGetSphere(const DataArray *arr);
    /** Handler to set whether or not this RndDrawable is showing.
     * @param [in] arr The supplied DataArray.
     * Expected DataArray contents:
     *     Node 2: a boolean for showing or hiding.
     * Example usage: {$this set_showing TRUE}
     */
    DataNode OnSetShowing(const DataArray *arr);
    /** Handler to get whether or not this RndDrawable is showing.
     * @returns True if showing, false if not.
     * Example usage: {$this showing}
     */
    DataNode OnShowing(const DataArray *);
    /** Handler to zero this RndDrawable's sphere.
     * Example usage: {$this zero_sphere}
     */
    DataNode OnZeroSphere(const DataArray *);
    DataNode OnGetDrawChildren(const DataArray *);

    /** "Whether the object and its Draw children are drawn or collided with." */
    bool mShowing; // 0x8
    /** "bounding sphere" */
    Sphere mSphere; // 0xc
    /** "Draw order within proxies, lower numbers are drawn first,
        so assign numbers from the outside-in (unless translucent), to minimize overdraw.
        In groups, draw_order will be ignored unless you explicitly
        click the sort button."*/
    float mOrder; // 0x20
};

class DrawPtrVec : public ObjPtrVec<RndDrawable> {
public:
    DrawPtrVec(Hmx::Object *owner) : ObjPtrVec<RndDrawable>(owner) {}

    void Draw() const;

    RndDrawable *CollideShowing(const Segment &, float &, Plane &) const;
    void CollideList(const Segment &s, std::list<RndDrawable::Collision> &c) const {
        FOREACH (it, *this) {
            (*it)->CollideList(s, c);
        }
    }
};
