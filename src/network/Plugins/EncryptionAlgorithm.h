#pragma once
#include "PluginObject.h"
#include "Plugins/Buffer.h"
#include "Plugins/Key.h"

namespace Quazal {
    class EncryptionAlgorithm : public PluginObject {
    public:
        EncryptionAlgorithm(unsigned int minKeyLength, unsigned int maxKeyLength);
        virtual ~EncryptionAlgorithm();
        virtual bool Encrypt(const Buffer &, Buffer *) = 0;
        virtual bool Encrypt(Buffer *);
        virtual bool Decrypt(const Buffer &, Buffer *) = 0;
        virtual bool Decrypt(Buffer *);
        virtual bool GetErrorString(unsigned int, char *, unsigned int);
        virtual void KeyHasChanged();

        bool SetKey(const Key &);

        Key mKey; // 0x8
        unsigned int mMinKeyLength; // 0x18
        unsigned int mMaxKeyLength; // 0x1c
    };
}
