#pragma once

struct XTEABlock {
    unsigned long long mData[2];
};

class XTEABlockEncrypter {
private:
    unsigned long long mNonce[2];
    unsigned int mKey[4];

    unsigned long long Encipher(unsigned long long, unsigned int *);

public:
    XTEABlockEncrypter();
    void SetKey(const unsigned char *);
    void SetNonce(const unsigned long long *, unsigned int);
    void Encrypt(const XTEABlock *, XTEABlock *);
};
