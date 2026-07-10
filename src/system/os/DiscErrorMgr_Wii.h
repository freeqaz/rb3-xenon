#pragma once
// Xbox 360 stub: Wii disc error manager is not applicable on X360.
// Game.h includes this but the Wii-specific paths are never taken.
class DiscErrorMgrWii {
public:
    class Callback {
    public:
        virtual void DiscErrorStart() {}
        virtual void DiscErrorEnd() {}
        virtual void DiscErrorDraw(void *) {}
    };
    DiscErrorMgrWii() {}
    void Init() {}
    void SetDiscError(bool) {}
    // Retail Xbox's Game does not inherit Callback (see Game.h), so these take
    // void* to accept a Game* without requiring the Callback base sub-object.
    void AddCallback(void *) {}
    void RemoveCallback(void *) {}
};

extern DiscErrorMgrWii TheDiscErrorMgrWii;
