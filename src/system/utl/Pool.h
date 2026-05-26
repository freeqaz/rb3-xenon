#pragma once

class Pool {
public:
    Pool(int, void *, int);
    void *Alloc();
    void Free(void *);

private:
    char *mFree; // 0x0
};
