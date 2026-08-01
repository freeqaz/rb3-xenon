// rb3_render_glue.cpp -- the CONSUMER SEAMS milo-native-engine deliberately
// leaves for its consumer to fill. Compiled only into rb3-render.
//
// These are not stubs in the milo_link_stubs.cpp sense. Each one is a slot the
// engine declares in a header and does NOT define anywhere, precisely so that
// the consuming decomp supplies its own answer:
//
//   platform/MeshFilter.h   ShouldSkipMesh    -- DC3 fills it from
//                           native/src/platform/MeshFilter.cpp; the engine ships
//                           only the header. Mesh_Wgpu.cpp:174 calls it on every
//                           draw.
//   platform/DebugPanel.h   DebugPanel::*     -- the engine's CMakeLists:365-369
//                           DEFERS src/platform/DebugPanel.cpp as DC3 glue
//                           (it pairs with HttpServer/GameplayTelemetry), while
//                           Rnd_Wgpu.cpp:864 still calls DebugPanel::Toggle() on
//                           the backtick key. So every non-DC3 consumer owes a
//                           definition.
//
// Both answers are xenon's, and both are argued rather than copied from DC3.

#include "platform/DebugPanel.h"
#include "platform/MeshFilter.h"

// ---------------------------------------------------------------------------
// ShouldSkipMesh -- RB3 skips NOTHING, and that is a real difference from DC3.
//
// DC3's filter is 40 lines of name matching and every entry is KINECT: the
// silhouette/skeleton overlays, the mic and speech-warning UI, the hand-gesture
// icons, the depth-buffer projection quads, the camera preview. They are
// skipped because DC3 natively has no gesture or speech subsystem, so the DTA
// flow that would animate them to transparent never runs and they render as
// opaque sheets over the scene.
//
// RB3 on X360 has no Kinect content at all -- it is an instrument game whose
// entire UI is driven by the controller. There is no analogous class of mesh to
// suppress, so copying DC3's list would be cargo-culting: at best inert, at
// worst it would silently drop an RB3 mesh that happens to share a name
// fragment ("spotlight" and "tutorial" are both plausible RB3 mesh names, and
// both are in DC3's list).
//
// Returning false unconditionally is therefore the FAITHFUL answer, not the
// lazy one: the renderer draws exactly what the .milo contains. If an RB3 mesh
// class ever does need suppressing, this is where the argument for it goes,
// with the asset that motivated it named.
bool ShouldSkipMesh(const char * /*meshName*/, RndMat * /*mat*/) { return false; }

// ---------------------------------------------------------------------------
// DebugPanel -- no-op, because there is no ImGui context in this build.
//
// DC3's DebugPanel is an ImGui overlay of NativeSettings sliders. rb3-render is
// headless and does not compile ImGui (no HX_IMGUI, no window, no input), so
// there is no surface for it to draw on and no key for Toggle() to be bound to.
// The one live reference is Rnd_Wgpu.cpp:864, inside the engine's GLFW key
// handler -- which cannot fire without a window.
//
// IsVisible() returns false rather than true so that any future engine code
// guarding work on "is the panel up?" takes the cheap branch.
namespace DebugPanel {
    void Init() {}
    void Draw() {}
    bool IsVisible() { return false; }
    void Toggle() {}
    void SetVisible(bool) {}
} // namespace DebugPanel
