// JsonWriter::GetBuffer implementation
// Converts UTF-16 buffer to UTF-8

#include "xdk/xjson/jsonwriter.h"

// GetBuffer converts the internal UTF-16 buffer to UTF-8
// Returns HRESULT: S_OK on success, error codes for failures
long JsonWriter::GetBuffer(unsigned short* pBuffer, unsigned long* pSize) {
    unsigned long hr = 0;

    // Validate parameters
    if (!pSize || (!pBuffer && *pSize != 0)) {
        hr = 0x80070057; // E_INVALIDARG
    } else {
        unsigned long outputSize = 0;
        unsigned long index = 0;

        // First pass: calculate required UTF-8 buffer size
        if (mBufferSize > 0) {
            int offset = 0;
            do {
                index++;
                unsigned short wc = *(unsigned short*)((char*)mBuffer + offset);
                offset += 2;

                if (wc <= 0x7F) {
                    outputSize++;
                    if (outputSize < *pSize) {
                        *pBuffer = (unsigned char)(wc & 0x7F);
                        pBuffer++;
                        *pBuffer = 0;
                    }
                } else {
                    unsigned long maxSize = *pSize;
                    if (wc <= 0x7FF) {
                        outputSize += 2;
                        if (outputSize < maxSize) {
                            *pBuffer = (unsigned char)(0xC0 | ((wc >> 6) & 0x1F));
                            pBuffer++;
                            *pBuffer = (unsigned char)(0x80 | (wc & 0x3F));
                            pBuffer++;
                            *pBuffer = 0;
                        }
                    } else {
                        outputSize += 3;
                        if (outputSize < maxSize) {
                            *pBuffer = (unsigned char)(0xE0 | ((wc >> 12) & 0x0F));
                            pBuffer++;
                            *(pBuffer + 1) = (unsigned char)(0x80 | ((wc >> 6) & 0x3F));
                            pBuffer++;
                            *(pBuffer + 1) = (unsigned char)((wc & 0x3F) | 0x80);
                            pBuffer++;
                            *pBuffer = 0;
                        }
                    }
                }
            } while (index < mBufferSize);
        }

        // Check if buffer was too small
        if (outputSize >= *pSize) {
            hr = 0x803F0005; // Buffer too small error
        }

        *pSize = outputSize + 1; // Include null terminator
    }

    return hr;
}
