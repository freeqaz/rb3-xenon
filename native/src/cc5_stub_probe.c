/* cc5_stub_probe.c -- native-only diagnostic: which link STUBS actually execute?
 *
 * WHY THIS EXISTS
 * ---------------
 * The native build satisfies off-path symbols with hand-written stubs (empty
 * bodies / neutral returns).  Each stub file asserts, in a comment, that its
 * stubs are never reached on the demo's live path -- e.g. m10_leaf_stubs.cpp:
 * "NONE is reached on the scoring path (verified: ...)".  Nothing ever tested
 * those claims.  A stub that IS reached silently turns real engine behaviour
 * into a no-op, which makes a demo's output partly fictional -- and the X360
 * match build cannot see it, because it never links or runs.
 *
 * HOW IT WORKS
 * ------------
 * The stub translation units (and only those) are compiled with clang's
 * -finstrument-functions, so every function they define calls
 * __cyg_profile_func_enter on entry.  We record the distinct function
 * addresses.  At exit we dump them, together with a reference address, so the
 * addresses can be resolved to symbol names OFFLINE against the binary's
 * symbol table (this avoids -rdynamic, which would perturb --gc-sections on
 * six of the targets and could change what links at all).
 *
 * Enabled only by the RB3_STUB_PROBE CMake option; nothing here is compiled
 * into a normal build, and no shared decomp source is touched.
 */
#include <stdio.h>
#include <stdlib.h>

#define CC5_MAX_HITS 16384

static void *g_hits[CC5_MAX_HITS];
static long g_counts[CC5_MAX_HITS];
static int g_n;
static int g_overflow;

/* Not instrumented itself: this TU is compiled without -finstrument-functions. */
void rb3_stub_record(void *fn) {
    int i;
    for (i = 0; i < g_n; i++) {
        if (g_hits[i] == fn) { g_counts[i]++; return; }
    }
    if (g_n < CC5_MAX_HITS) {
        g_hits[g_n] = fn;
        g_counts[g_n] = 1;
        g_n++;
    } else {
        g_overflow = 1;
    }
}

void __cyg_profile_func_enter(void *fn, void *call_site) {
    (void)call_site;
    rb3_stub_record(fn);
}

void __cyg_profile_func_exit(void *fn, void *call_site) {
    (void)fn; (void)call_site;
}

__attribute__((destructor)) static void cc5_dump(void) {
    const char *path = getenv("RB3_STUB_PROBE_OUT");
    FILE *f = path ? fopen(path, "w") : stderr;
    int i;
    if (!f) f = stderr;
    /* Reference point: lets the reader compute the PIE load bias offline as
     * (runtime REF) - (nm address of rb3_stub_record). */
    fprintf(f, "#REF rb3_stub_record %p\n", (void *)&rb3_stub_record);
    fprintf(f, "#DISTINCT %d%s\n", g_n, g_overflow ? " OVERFLOW" : "");
    for (i = 0; i < g_n; i++) {
        fprintf(f, "HIT %p %ld\n", g_hits[i], g_counts[i]);
    }
    if (path) fclose(f);
}
