// Low-rate telemetry for interactive sessions. No changes to the game clock or input.
#include "game/BandUser.h"
#include "game/Game.h"
#include "game/GemPlayer.h"
#include "game/Stats.h"
#include "beatmatch/BeatMatcher.h"
#include "beatmatch/BeatMaster.h"
#include "bandtrack/GemTrack.h"
#include "bandtrack/TrackPanel.h"
#include "bandobj/BandDirector.h"
#include "meta_band/MetaPerformer.h"
#include "platform/Rnd_Wgpu.h"
#include "gfx/Screenshot.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

extern GLFWwindow *gNativeWindow;

namespace {
const char *directory;
FILE *timeline;
int requestedFrame;
std::string framePath;

void CaptureFrame() {
    if (!requestedFrame) return;
    auto &gpu = gWgpuRnd->Gpu();
    const int w = gpu.OutputWidth(), h = gpu.OutputHeight();
    std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4);
    bool saved = false;
    if (gpu.ReadbackFrame(pixels.data(), pixels.size())) {
        for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;
        saved = WriteScreenshot(framePath.c_str(), pixels.data(), w, h);
    }
    fprintf(stderr, "rb3-playtest: frame %d screenshot %s: %s\n", requestedFrame,
            saved ? "saved" : "failed", framePath.c_str());
    requestedFrame = 0;
}

bool OpenTimeline() {
    directory = getenv("RB3_APP_PLAYTEST_DIR");
    if (!directory || !*directory) return false;
    const std::string path = std::string(directory) + "/timeline.tsv";
    timeline = fopen(path.c_str(), "w");
    if (!timeline) { perror(path.c_str()); return false; }
    fputs("frame\twall_s\tworst_frame_ms\tevent\tscreen\ttransition\tsong\tplayer\ttrack_type"
          "\tdifficulty\tmatcher_ms\tchart_tick\tscore\thits\tmisses\tenergy\tdeploying\taudio_ms\tlane_frame\tlane_units_per_s\tlane_view_s\tcamera_shot\n", timeline);
    fprintf(stderr, "rb3-playtest: %s (F8 marks the next rendered frame)\n", path.c_str());
    return true;
}
} // namespace

void RB3AppPlaytestFrame(int frame, double now, const char *screen, const char *next, bool changed) {
    static const bool enabled = OpenTimeline();
    if (!enabled) return;
    static const std::string request = std::string(directory) + "/capture.request";
    static double lastFrame = now, nextSample = 0.0, nextRequestCheck = 0.0, worstMs = 0.0;
    worstMs = std::max(worstMs, (now - lastFrame) * 1000.0);
    lastFrame = now;

    const bool down = gNativeWindow && glfwGetKey(gNativeWindow, GLFW_KEY_F8) == GLFW_PRESS;
    static bool held = false;
    bool mark = down && !held;
    held = down;
    if (now >= nextRequestCheck) {
        if (access(request.c_str(), F_OK) == 0 && unlink(request.c_str()) == 0) mark = true;
        nextRequestCheck = now + 0.25;
    }
    if (!changed && !mark && now < nextSample) return;
    nextSample = now + 1.0;
    if (mark && gWgpuRnd) {
        requestedFrame = frame;
        framePath = std::string(directory) + "/frame-" + std::to_string(frame) + ".png";
        gWgpuRnd->SetFrameReadyCallback(CaptureFrame);
    }

    const bool playing = TheGame && strcmp(screen, "game_screen") == 0 && strcmp(next, "none") == 0;
    const char *song = "-";
    if (playing && MetaPerformer::Current()) song = MetaPerformer::Current()->Song().Str();
    auto prefix = [&] {
        fprintf(timeline, "%d\t%.3f\t%.2f\t%s\t%s\t%s\t%s\t", frame, now, worstMs,
                mark ? "mark" : changed ? "screen" : "sample", screen, next, song);
    };
    bool wrotePlayer = false;
    if (playing) {
        WorldDir *world = TheBandDirector ? TheBandDirector->GetWorld() : nullptr;
        CamShot *shot = world ? world->GetCameraManager()->CurrentShot() : nullptr;
        if (shot && shot->CurrentShot()) shot = shot->CurrentShot();
        const auto &players = TheGame->GetActivePlayers();
        for (size_t i = 0; i < players.size(); ++i) {
            Player *p = players[i];
            if (!p) continue;
            GemPlayer *gem = dynamic_cast<GemPlayer *>(p);
            BeatMatcher *matcher = gem ? gem->mMatcher : nullptr;
            GemTrack *track = TheTrackPanel ? dynamic_cast<GemTrack *>(TheTrackPanel->GetTrack(p, false)) : nullptr;
            GemTrackDir *lane = track ? track->GetTrackDir() : nullptr;
            BeatMaster *master = TheGame->GetBeatMaster();
            prefix();
            fprintf(timeline, "%zu\t%d\t%d\t%.2f\t%.2f\t%d\t%d\t%d\t%.4f\t%d\t%.2f\t%.3f\t%.3f\t%.3f\t%s\n",
                    i, (int)p->GetTrackType(), p->GetUser() ? (int)p->GetUser()->GetDifficulty() : -1,
                    matcher ? matcher->GetNow() : -1.0f,
                    matcher ? gem->GetSongPos().GetTotalTick() : -1.0f, (int)p->mScore,
                    p->GetStats().GetHitCount(), p->GetStats().GetMissCount(),
                    p->GetBandEnergy(), (int)p->IsDeployingBandEnergy(),
                    master && master->GetAudio() ? TheGame->GetSongMs() : -1.0f,
                    lane && lane->mAnimGroup ? lane->mAnimGroup->GetFrame() : -1.0f,
                    lane ? lane->mYPerSecond : -1.0f, lane ? lane->ViewTimeSeconds() : -1.0f,
                    shot && shot->Name() ? shot->Name() : "-");
            wrotePlayer = true;
        }
    }
    if (!wrotePlayer) {
        prefix();
        fputs("-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\t-\n", timeline);
    }
    fflush(timeline);
    worstMs = 0.0;
}
