#ifndef XDK_XJSON_JSONWRITER_H
#define XDK_XJSON_JSONWRITER_H

// JsonWriter class for Xbox JSON support
class JsonWriter {
public:
    long GetBuffer(unsigned short* pBuffer, unsigned long* pSize);

private:
    void* mBuffer;           // Offset 0x08 - UTF-16 buffer
    unsigned long mBufferSize; // Offset 0x0C - number of characters
};

#endif // XDK_XJSON_JSONWRITER_H
