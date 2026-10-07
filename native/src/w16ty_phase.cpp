// rb3-xenon native -- W16-TY: VIA-DC3 engine rows native linked but no target
// entered, and VIA-DC3 rows that run with no gate (PROBE).

#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "utl/FilePath.h"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <vector>
#include <string>

typedef void (*GateFn)(const char *, bool, const char *);

namespace {

GateFn gGate = nullptr;
int gRan = 0;

ObjectDir *LoadDir(ObjDirPtr<ObjectDir> &holder, const char *path) {
    auto t0 = std::chrono::steady_clock::now();
    holder.LoadFile(FilePath(path), false, true, kLoadFront, false);
    ObjectDir *dir = holder.Ptr();
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    std::map<std::string, int> cls;
    int n = 0;
    std::vector<ObjectDir *> todo;
    std::set<ObjectDir *> seen;
    if (dir)
        todo.push_back(dir);
    while (!todo.empty()) {
        ObjectDir *d = todo.back();
        todo.pop_back();
        if (!seen.insert(d).second)
            continue;
        for (ObjDirItr<Hmx::Object> it(d, true); it; ++it) {
            cls[it->ClassName().Str()]++;
            n++;
            ObjectDir *sub = dynamic_cast<ObjectDir *>(&*it);
            if (sub && sub != d)
                todo.push_back(sub);
        }
    }
    printf("  probe %s: %s, %d objects, %.0f ms\n", path, dir ? "loaded" : "NULL", n, ms);
    for (auto &kv : cls)
        printf("     %-24s %d\n", kv.first.c_str(), kv.second);
    return dir;
}

} // namespace

int RunW16TYPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== W16-TY phase: VIA-DC3 rows on shipped data (probe) ===\n");
    static ObjDirPtr<ObjectDir> a, b, c, d;
    LoadDir(a, "world/vignette/transition/gen/tv11_a.milo_xbox");
    LoadDir(b, "world/vignette/shell/gen/sv7_b.milo_xbox");
    LoadDir(c, "char/main/rigging/gen/keyboard.milo_xbox");
    LoadDir(d, "char/main/gen/main.milo_xbox");
    return gRan;
}
