#include "jpeg/Jpeg.h"
#include "jpeg/jpeglib.h"
#include "os/Debug.h"

namespace {
    // Extended destination manager with additional fields for buffer tracking
    struct ExtendedDestMgr {
        jpeg_destination_mgr pub;  // Standard manager (0x0-0x13)
        JOCTET *bufferStart;       // Original buffer pointer (0x14)
        size_t bufferSize;         // Total buffer size (0x18)
        int bytesWritten;          // Result: bytes written (0x1c)
    };

    void JpegInitDestination(jpeg_compress_struct *s) {
        ExtendedDestMgr *dest = (ExtendedDestMgr *)s->dest;
        MILO_ASSERT(dest, 0x8b);
        dest->pub.next_output_byte = dest->bufferStart;
        dest->pub.free_in_buffer = dest->bufferSize;
    }
    unsigned char JpegEmptyOutputBuffer(jpeg_compress_struct *s) {
        MILO_ASSERT(false, 0x94);
        return 0;
    }
    void JpegTermDestination(jpeg_compress_struct *s) {
        ExtendedDestMgr *dest = (ExtendedDestMgr *)s->dest;
        MILO_ASSERT(dest, 0x9c);
        dest->bytesWritten = dest->bufferSize - dest->pub.free_in_buffer;
    }
};

bool LoadBitmapIntoJpeg(char *data, int width, int height, int depth, void *destBuffer, int &outSize) {
    JSAMPROW rowPtr;
    ExtendedDestMgr destMgr;
    jpeg_compress_struct cinfo;
    jpeg_error_mgr errorMgr;

    cinfo.err = jpeg_std_error(&errorMgr);
    jpeg_CreateCompress(&cinfo, JPEG_LIB_VERSION, sizeof(jpeg_compress_struct));

    // Zero the entire destMgr struct
    memset(&destMgr, 0, sizeof(destMgr));
    destMgr.bufferStart = (JOCTET *)destBuffer;
    destMgr.bufferSize = outSize;

    cinfo.image_height = height;
    cinfo.image_width = width;
    cinfo.input_components = depth;

    destMgr.pub.init_destination = JpegInitDestination;
    destMgr.pub.empty_output_buffer = JpegEmptyOutputBuffer;
    destMgr.pub.term_destination = JpegTermDestination;
    cinfo.dest = &destMgr.pub;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);

    int bytesPerRow = cinfo.input_components * width;
    while (cinfo.next_scanline < cinfo.image_height) {
        rowPtr = (JSAMPROW)(data + cinfo.next_scanline * bytesPerRow);
        jpeg_write_scanlines(&cinfo, (JSAMPARRAY)&rowPtr, 1);
    }

    jpeg_finish_compress(&cinfo);
    outSize = destMgr.bytesWritten;

    return true;
}
