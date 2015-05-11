/*
 * Implementation of ASTC compression loader
 *
 * grep for GRAPHICS_COMPRESSION in source code to follow related changes.
 *
 * Copyright 2013 Samsung Electronics
 * @author s1.malhotra@partner.samsung.com
 */


#include "SkImageDecoder.h"
#include "SkStream.h"
#include "SkCompressedImageRef.h"
#include "SkTRegistry.h"
#include <android/log.h>

class SkASTCImageDecoder : public SkImageDecoder {
public:
	SkASTCImageDecoder() {
	}

    virtual Format getFormat() const {
        return (Format)(kLastKnownFormat + 100);
    }

protected:
    virtual bool onDecode(SkStream* stream, SkBitmap* bm, Mode mode);
};


static SkImageDecoder* Factory(SkStreamRewindable* stream) {
    //First 4 bytes are 0x5CA1AB13
	//Low endian byte order - [0x13, 0xAB, 0xA1, 0x5C]

    static const char kASTC[] = {0x13, 0xAB, 0xA1, 0x5C};

    size_t len = stream->getLength();
    char buffer[sizeof(kASTC)];
    size_t startLen = sizeof(kASTC);

    if (len > 16 &&
            stream->read(buffer, startLen) == startLen &&
            !memcmp(buffer, kASTC, startLen)) {
        char data[12];
        if (stream->read(data, 12) != sizeof(data))
            return NULL;
        
        //int width = (((int)data[4]) << 8) + ((int)data[3] & 0xFF);
        //int height = (((int)data[7]) << 8) + ((int)data[6] & 0xFF);
        return SkNEW(SkASTCImageDecoder);
    }

   return NULL;
}

bool SkASTCImageDecoder::onDecode(SkStream* stream, SkBitmap* bm, Mode mode) {
    size_t length = stream->getLength();

    if (SkImageDecoder::kDecodeBounds_Mode == mode) {
        char data[16];
        if (stream->read(data, 16) != 16) {
            return false;
        }
        // assign these directly, in case we return kDimensions_Result
        bm->setConfig(SkBitmap::kASTC,
                (((int)data[8]) << 8) + ((int)data[7] & 0xFF),
                (((int)data[11]) << 8) + ((int)data[10] & 0xFF), kPremul_SkAlphaType);
        return true;
    }

    // read all and set as bitmap's pixels
    SkAutoMalloc storage(length);

    if (stream->read(storage.get(), length) != length) {
        return false;
    }
    char *data = (char*)storage.get();
    unsigned int width  = (((unsigned int)data[8]) << 8) + ((unsigned int)data[7] & 0xFF);
    unsigned int height = (((unsigned int)data[11]) << 8) + ((unsigned int)data[10] & 0xFF);

    unsigned int internalFormat = 0x93B4; // GL_COMPRESSED_RGBA_ASTC_6x6_KHR

    // Byte 4 is the X block dimension
    // Byte 5 is the Y block dimension
    switch (data[4]) {
        case 4:
            internalFormat = 0x93B0; // #define GL_COMPRESSED_RGBA_ASTC_4x4_KHR
        break;
        case 5:
            if (data[5] == 5)
                internalFormat = 0x93B2; // #define GL_COMPRESSED_RGBA_ASTC_5x5_KHR
            else
                internalFormat = 0x93B1; // #define GL_COMPRESSED_RGBA_ASTC_5x4_KHR
            break;
        case 6:
            if (data[5] == 5)
                internalFormat = 0x93B3; // #define GL_COMPRESSED_RGBA_ASTC_6x5_KHR
            break;
        case 8:
                internalFormat = 0x93B7; // #define GL_COMPRESSED_RGBA_ASTC_8x8_KHR
            break;
        default:
            __android_log_print(ANDROID_LOG_WARN, "GFX ASTC image decoder - invalid cell size", "%dx%d for %dx%d image", data[4], data[5], width, height);
            break;
    }

    bm->setConfig(SkBitmap::kASTC, width, height, 0, kPremul_SkAlphaType);
    SkCompressedImageRef *ref = new SkCompressedImageRef(storage.detach(), length - 16, 16, width, height, internalFormat);
    bm->setPixelRef(ref)->unref();
    SkAutoLockPixels alp(*bm);
    return true;
}

static SkImageDecoder_DecodeReg gReg(Factory);

