/*
 * Implementation of PVRTC compression loader: this is dummy decoder as
 * no decoding is happening, just loading content.
 *
 * grep for GRAPHICS_COMPRESSION in source code to follow related changes.
 *
 * Copyright 2013 Samsung Electronics
 * @author aleksandar.s@samsung.com
 */


#include "SkImageDecoder.h"
#include "SkStream.h"
#include "SkColorPriv.h"
#include "SkTDArray.h"
#include "SkTRegistry.h"
#include "SkCompressedImageRef.h"
#include <android/log.h>

class SkPVRImageDecoder : public SkImageDecoder {
public:
	SkPVRImageDecoder() {
	}

    virtual Format getFormat() const {
        return kPVRTC_Format;
    }

protected:
    virtual bool onDecode(SkStream* stream, SkBitmap* bm, Mode mode);
};

static SkImageDecoder* Factory(SkStreamRewindable* stream) {
	// first 4 bytes 0x50565203
    static const char kPvrStart[] = {0x50, 0x56, 0x52, 0x03};

    size_t len = stream->getLength();

    char buffer[sizeof(kPvrStart)];

    if (len > 0x20 &&
            stream->read(buffer, sizeof(kPvrStart)) == sizeof(kPvrStart) &&
            !memcmp(buffer, kPvrStart, sizeof(kPvrStart))) {
            char data[0x1C];
            if (stream->read(data, 0x1C) != 0x1C)
            return NULL;
            //PMD ERROR
        /*int height = ((int)(data[0x14+3] & 0x7F)<<24) | ((int)(data[0x14+2] & 0xFF)<<16) | ((int)(data[0x14+1] & 0xFF)<<8) | (int)(data[0x14] & 0xFF);
        int width = ((int)(data[0x18+3] & 0x7F)<<24) | ((int)(data[0x18+2] & 0xFF)<<16) | ((int)(data[0x18+1] & 0xFF)<<8) | (int)(data[0x18] & 0xFF);*/

        return SkNEW(SkPVRImageDecoder);
    }
    return NULL;
}

bool SkPVRImageDecoder::onDecode(SkStream* stream, SkBitmap* bm, Mode mode) {

    size_t length = stream->getLength();
    if (SkImageDecoder::kDecodeBounds_Mode == mode) {
        char data[0x20];
        if (stream->read(data, 0x20) != 0x20) {
            return false;
        }
        // assign these directly, in case we return kDimensions_Result
        int imageHeight = ((int)(data[0x18+3] & 0x7F)<<24) | ((int)(data[0x18+2] & 0xFF)<<16) | ((int)(data[0x18+1] & 0xFF)<<8) | (int)(data[0x18] & 0xFF);
        int imageWidth = ((int)(data[0x1C+3] & 0x7F)<<24) | ((int)(data[0x1C+2] & 0xFF)<<16) | ((int)(data[0x1C+1] & 0xFF)<<8) | (int)(data[0x1C] & 0xFF);
        bm->setConfig(SkBitmap::kPVRTC2_2,
                imageWidth,
                imageHeight, kPremul_SkAlphaType);
        return true;
    }

    // read all and set as bitmap's pixels
    SkAutoMalloc storage(length);

    if (stream->read(storage.get(), length) != length) {
        return false;
    }
    char *data = (char*)storage.get();
    int height = ((int)(data[0x18+3] & 0x7F)<<24) | ((int)(data[0x18+2] & 0xFF)<<16) | ((int)(data[0x18+1] & 0xFF)<<8) | (int)(data[0x18] & 0xFF);
    int width = ((int)(data[0x1C+3] & 0x7F)<<24) | ((int)(data[0x1C+2] & 0xFF)<<16) | ((int)(data[0x1C+1] & 0xFF)<<8) | (int)(data[0x1C] & 0xFF);


    int bitsPerPixel = 2;
    switch (data[8]) {
    case 0x00:
    case 0x01:
    case 0x04:
        bitsPerPixel = 2;
        break;
    case 0x02:
    case 0x03:
    case 0x05:
        bitsPerPixel = 4;
        break;
    default:
        __android_log_print(ANDROID_LOG_WARN, "GFX_COMPRSkPVRImageDecoder", "onDecode - unsupported PVR format: %x", data[8]);
        break;
    }

    int paddedWidth = ((width + 7)>>3)<<3;
    int paddedHeight = ((height + 3)>>2)<<2;

    int byteSize = paddedWidth * paddedHeight * bitsPerPixel / 8;
    if (byteSize < 32)
        byteSize = 32;

    bm->setConfig(SkBitmap::kPVRTC2_2, width, height, 0, kPremul_SkAlphaType);
    // FIXME reusing bitmap

    // PVRTC requires paddedWidth and paddedHeight to be supplied to glCompressedTexImage2D.
    // texture coordinates need to get scaled from [0-1] -> [0-width/paddedWidth] when renderring
    // TODO parse and support additional formats
    SkCompressedImageRef *ref = new SkCompressedImageRef(storage.detach(), byteSize, length - byteSize, paddedWidth, paddedHeight, 0x9137/*GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG*/);
    bm->setPixelRef(ref)->unref();
    SkAutoLockPixels alp(*bm);
    __android_log_print(ANDROID_LOG_INFO, "GFX_COMPR","SkPVRImageDecoder::onDecode %dx%d %d=%d+%d - mode:%d config: %d end", width, height, length, byteSize,  length - byteSize, mode, bm->config());
    return true;
}

static SkImageDecoder_DecodeReg gReg(Factory);


