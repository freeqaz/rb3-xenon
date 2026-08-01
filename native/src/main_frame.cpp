// rb3-frame — X1 engine link smoke target (rb3-xenon <- milo-native-engine).
//
// WHAT THIS PROVES, AND WHAT IT DELIBERATELY DOES NOT
// ---------------------------------------------------
// SPIKE-X0 (docs/plans/spike-x0-engine-dc3-flavor-2026-08-01.md) proved the
// engine's `dc3` GPU flavor COMPILES against rb3-xenon's headers. Its explicit
// deliverable was "libmilo-engine.a, not a frame" — nothing was linked and
// nothing was run. This binary closes that gap: configure -> compile -> LINK ->
// RUN -> real pixels out of a real GPU.
//
// It touches ZERO xenon rndobj objects. That is the point, not laziness.
// src/system/rndobj/Rnd.h:354-360 documents an Rnd/NgRnd member-layout shift in
// retail X360 RB3 relative to the shape the DC3-lineage engine headers assume.
// A first render that went through WgpuRnd : NgRnd could therefore produce a
// plausible-looking image while silently reading the wrong member offsets — and
// "it drew something" would read as progress. So the smoke target exercises only
// the decomp-agnostic half of the engine (gfx/GpuDevice.cpp for device +
// headless target + readback, gfx/Screenshot.cpp for PNG encode), and leaves the
// Rnd coupling to X3, where it can be debugged against this known-good baseline.
//
// DETERMINISM: the output must be byte-identical across runs, because the whole
// value of a baseline is that a later diff means something. A clear is the only
// GPU operation with no float-precision, no rasterization-rule and no driver-
// scheduling variance, which is the second reason it (and not a triangle) is the
// right smoke test. The clear colour is chosen to survive the unorm round-trip
// exactly: each channel is k/255 for integer k, so f32 -> unorm8 cannot land
// between two representable values and cannot tie-break differently.
//
// Usage:  rb3-frame <out.png> [width] [height]
// Exit 0 only if every pixel came back as the expected colour.

#include "gfx/GpuDevice.h"
#include "gfx/Screenshot.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <webgpu/webgpu_cpp.h>

// Expected clear colour, exact in RGBA8 unorm: (48, 96, 160, 255).
static const uint8_t kExpect[4] = {48, 96, 160, 255};
static const double kClear[4] = {48.0 / 255.0, 96.0 / 255.0, 160.0 / 255.0, 1.0};

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <out.png> [width] [height]\n", argv[0]);
        return 2;
    }
    const char *outPath = argv[1];
    const int W = (argc > 2) ? atoi(argv[2]) : 320;
    const int H = (argc > 3) ? atoi(argv[3]) : 180;
    if (W <= 0 || H <= 0) {
        fprintf(stderr, "rb3-frame: bad dimensions %dx%d\n", W, H);
        return 2;
    }

    GpuDevice gpu;
    GpuDeviceDesc desc;
    desc.headless = true;
    desc.width = W;
    desc.height = H;
    desc.title = "rb3-frame";
    if (!gpu.Init(desc)) {
        fprintf(stderr, "rb3-frame: GpuDevice::Init FAILED (no usable adapter?)\n");
        return 1;
    }
    printf("rb3-frame: device up (%dx%d, headless=%d, null-backend=%d, BC=%d)\n",
           gpu.WindowWidth(), gpu.WindowHeight(), (int)gpu.IsHeadless(),
           (int)gpu.IsNullBackend(), (int)gpu.HasBCCompression());

    // A null/CPU backend would happily "succeed" while producing nothing real.
    // Fail loudly rather than bank a fictional baseline.
    if (gpu.IsNullBackend()) {
        fprintf(stderr, "rb3-frame: adapter is the NULL backend — refusing to "
                        "certify a frame that no GPU produced\n");
        return 1;
    }

    wgpu::TextureView frame = gpu.AcquireHeadlessFrame();
    if (!frame) {
        fprintf(stderr, "rb3-frame: AcquireHeadlessFrame FAILED\n");
        return 1;
    }

    // Clear-only pass. No pipeline, no vertex buffer, no bind groups, no depth
    // attachment — every one of those would drag in a decomp-shaped contract
    // (VertexFormats/UniformStructs) that this target has no business asserting.
    wgpu::RenderPassColorAttachment colorAtt{};
    colorAtt.view = frame;
    colorAtt.loadOp = wgpu::LoadOp::Clear;
    colorAtt.storeOp = wgpu::StoreOp::Store;
    colorAtt.clearValue = {kClear[0], kClear[1], kClear[2], kClear[3]};

    wgpu::RenderPassDescriptor rp{};
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &colorAtt;

    wgpu::CommandEncoder enc = gpu.Device().CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
    pass.End();
    wgpu::CommandBuffer cmd = enc.Finish();
    gpu.Queue().Submit(1, &cmd);

    std::vector<uint8_t> pixels((size_t)W * H * 4);
    if (!gpu.ReadbackHeadlessFrame(pixels.data(), pixels.size())) {
        fprintf(stderr, "rb3-frame: ReadbackHeadlessFrame FAILED\n");
        return 1;
    }

    // Verify EVERY pixel, not a sample: a partial clear (wrong viewport, wrong
    // row stride in the readback path) is exactly the class of bug a spot check
    // at the centre would wave through.
    size_t bad = 0;
    size_t firstBad = 0;
    for (size_t i = 0; i < (size_t)W * H; i++) {
        const uint8_t *p = &pixels[i * 4];
        if (memcmp(p, kExpect, 4) != 0) {
            if (bad == 0)
                firstBad = i;
            bad++;
        }
    }
    if (bad != 0) {
        const uint8_t *p = &pixels[firstBad * 4];
        fprintf(stderr,
                "rb3-frame: %zu/%d pixels wrong; first at (%zu,%zu) = "
                "(%u,%u,%u,%u), expected (%u,%u,%u,%u)\n",
                bad, W * H, firstBad % (size_t)W, firstBad / (size_t)W, p[0],
                p[1], p[2], p[3], kExpect[0], kExpect[1], kExpect[2],
                kExpect[3]);
        return 1;
    }

    if (!WriteScreenshot(outPath, pixels.data(), W, H)) {
        fprintf(stderr, "rb3-frame: WriteScreenshot('%s') FAILED\n", outPath);
        return 1;
    }

    printf("rb3-frame: OK — %dx%d cleared to (%u,%u,%u,%u), all %d pixels "
           "verified, wrote %s\n",
           W, H, kExpect[0], kExpect[1], kExpect[2], kExpect[3], W * H, outPath);
    gpu.Shutdown();
    return 0;
}
