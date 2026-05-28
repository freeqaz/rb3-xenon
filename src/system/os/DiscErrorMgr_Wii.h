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
    void AddCallback(Callback *) {}
    void RemoveCallback(Callback *) {}
};

extern DiscErrorMgrWii TheDiscErrorMgrWii;
