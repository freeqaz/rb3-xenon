#pragma once

namespace KeyChain {
    void getMasher(unsigned char *);
    void getKey(unsigned int, unsigned char *, unsigned char *);
    unsigned int getNumKeys();
};
