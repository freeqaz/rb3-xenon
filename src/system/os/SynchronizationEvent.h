#pragma once

class SynchronizationEvent {
private:
    void *mEvent;

public:
    SynchronizationEvent();
    ~SynchronizationEvent();
    void Set();
    bool Wait(int timeoutMs);
};
