#include "App.h"

// NOT CLOSABLE FROM SOURCE -- do not re-hunt (lane DS-4/B).
//
// This body is already exact: 18 of 19 instructions match byte-for-byte,
// including the full prologue/epilogue, the App ctor call (bl 0x82270E68) and
// the App dtor call (bl 0x82270000). The single residual is instruction 10 at
// 0x82272E90, the `app.Run()` call:
//
//     retail : 4280D1F1  =  op16 B-form,  BO=20 BI=0 AA=0 LK=1  ("bcl 20,0,X")
//     ours   :            op18 I-form,  LK=1                  ("bl X")
//
// Both are unconditional calls to the same target (0x82270080 = App::Run, a
// real prologue in the App.cpp cluster next to App::~App at 0x82270000); only
// the ENCODING FORM differs. B-form reaches +-32 KB, I-form +-32 MB, so no
// compiler would ever prefer B-form here.
//
// Measured over the whole retail .text: 178,015 instructions are op18 with
// LK=1, and exactly ONE is op16 with LK=1 -- this one. A binary-unique encoding
// is not an MSVC codegen idiom; it is a post-link artifact. MSVC emits I-form
// for every call, so no spelling of C++ can produce it. Closing this would take
// an __emit()/inline-asm fabrication, which is metric-fitting and is refused.
//
// => default/Main is capped at 1/2 matched functions; `main` is capped at
//    96.84 mpn (75 of 76 bytes correct).
int main(int argc, char **argv) {
    App app(argc, argv);
    app.Run();
}
