#pragma once

#include "types.h"

class App {
private:
    int _pad;

protected:
    void CDECL DrawRegular(void);
    void CDECL CaptureHiRes(void);

public:
    CDECL App(int, char **);
    CDECL ~App(void);

    // Never returns: the frame loop has no exit, and the unhandled-exception
    // filter in App.cpp calls it last, with no epilogue after the call.
#ifdef _MSC_VER
    __declspec(noreturn)
#endif
    void CDECL RunWithoutDebugging(void);
    void CDECL Run(void);
};
