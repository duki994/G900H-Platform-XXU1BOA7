/*
 * Implementation of ETC2 compression loader
 *
 * grep for GRAPHICS_COMPRESSION in source code to follow related changes.
 *
 * Copyright 2013 Samsung Electronics
 * @author s1.malhotra@partner.samsung.com
 */


#include "SkImageDecoder.h"
#include "SkStream.h"
#include "SkTRegistry.h"
#include "SkCompressedImageRef.h"
#include "SkColorTable.h"
#include <android/log.h>

class SkIndexedPaletteImageDecoder : public SkImageDecoder {
public:
    SkIndexedPaletteImageDecoder() {
        fInputStream = NULL;
        fOrigWidth = 0;
        fOrigHeight = 0;
        fHasAlpha = 0;
    }

    virtual ~SkIndexedPaletteImageDecoder() {
        SkSafeUnref(fInputStream);
    }

    virtual Format getFormat() const {
        return (Format)(kLastKnownFormat + 100);
    }

protected:
//    virtual bool onBuildTileIndex(SkStreamRewindable *stream, int *width, int *height) SK_OVERRIDE;
//    virtual bool onDecodeSubset(SkBitmap* bitmap, const SkIRect& rect) SK_OVERRIDE;
    virtual bool onDecode(SkStream* stream, SkBitmap* bm, Mode mode) SK_OVERRIDE;

private:
    SkStream* fInputStream;
    int fOrigWidth;
    int fOrigHeight;
    int fHasAlpha;
};

static SkImageDecoder* Factory(SkStreamRewindable* stream) {
    static const char kIPTStart[] = {'I', 'P', 'T', '0'};

    char buffer[sizeof(kIPTStart)];
    size_t startLen = sizeof(kIPTStart);

    if (stream->read(buffer, startLen) == startLen &&
            !memcmp(buffer, kIPTStart, startLen)) {
        char data[10];
        if (stream->read(data, sizeof(data)) != sizeof(data))
            return NULL;

        return SkNEW(SkIndexedPaletteImageDecoder);
    }
    return NULL;
}

/*bool SkIndexedPaletteImageDecoder::onBuildTileIndex(SkStreamRewindable* stream,
                                          int *width, int *height) {
    char data[16];
    if (stream->read(data, 16) != 16) {
        SkDebugf("Failed to read header from ETC2 stream!");
        return false;
    }

    int fullWidth   = ((int)(data[12] & 0xFF)<<8) | (int)(data[13] & 0xFF);
    int fullHeight  = ((int)(data[14] & 0xFF)<<8) | (int)(data[15] & 0xFF);

    if (!stream->rewind()) {
        SkDebugf("Failed to rewind ETC2 stream!");
        return false;
    }

    *width = fullWidth;
    *height = fullHeight;

    SkRefCnt_SafeAssign(this->fInputStream, stream);
    this->fOrigWidth = fullWidth;
    this->fOrigHeight = fullHeight;
    this->fHasAlpha = data[7] == 0x03;

    return true;
}

bool SkIndexedPaletteImageDecoder::onDecodeSubset(SkBitmap* decodedBitmap,
        const SkIRect& region) {
    SkIRect rect = SkIRect::MakeWH(fOrigWidth, fOrigHeight);

    if (!rect.intersect(region)) {
        // If the requested region is entirely outsides the image, return false
        return false;
    }

    // Note: sampling ignored
    int width = fOrigWidth;
    int height = fOrigHeight;


    SkBitmap *bitmap = decodedBitmap;
    int byteLength = 0;
    unsigned int rowByteLength = 0;
    unsigned int rowBytesToRead = 0;
    unsigned int leftBytesToSkip = 0;
    unsigned int rightBytesToSkip = 0;
    // based on region size, calculate needed length
    if (!fHasAlpha) {// GL_COMPRESSED_RGB8_ETC2
        byteLength = ((region.width() + 3) / 4 * 4) * ((region.height() + 3) / 4 * 4) / 2; // 4 bits per pixel
        rowByteLength = (fOrigWidth + 3) / 4 * 4 * 4 / 2;
        rowBytesToRead = (region.width() + 3) / 4 * 4 * 4/ 2;
        leftBytesToSkip = region.left() / 4 * 4 * 4 / 2;
        rightBytesToSkip = rowByteLength - leftBytesToSkip - rowBytesToRead;
    }
    else // 0x9278: // GL_COMPRESSED_RGBA8_ETC2_EAC
    {
        byteLength = ((region.width() + 3) / 4 * 4) * ((region.height() + 3) / 4 * 4);
        rowByteLength = (fOrigWidth + 3) / 4 * 4 * 4;
        rowBytesToRead = (region.width() + 3) / 4 * 4 * 4;
        leftBytesToSkip = region.left() / 4 * 4 * 4;
        rightBytesToSkip = rowByteLength - leftBytesToSkip - rowBytesToRead;
    }
    SkAutoMalloc storage(byteLength);
    unsigned char *texture = (unsigned char*) storage.get();
    unsigned char *currentRow = texture;
    // top rows to skip
    unsigned int topRowsToSkip = region.top() / 4;
    unsigned int rowsToRead = (region.height() + 3) / 4;

    if (fInputStream->skip(16) != 16) // skip header
        return false;
    if (topRowsToSkip * rowByteLength != 0 && fInputStream->skip(topRowsToSkip * rowByteLength) != topRowsToSkip * rowByteLength) // skip top part
        return false;
    // now read content
    for (unsigned int i = 0; i < rowsToRead; i++) {
        if (leftBytesToSkip && leftBytesToSkip != fInputStream->skip(leftBytesToSkip)) {
            return false;
        }
        if (fInputStream->read(currentRow, rowBytesToRead) != rowBytesToRead) {
            return false;
        }
        currentRow += rowBytesToRead;
        if (rightBytesToSkip && rightBytesToSkip != fInputStream->skip(rightBytesToSkip)) {
            return false;
        }
    }
    // after having it cropped, create texture ref
    unsigned int internalFormat = 0x9278; // GL_COMPRESSED_RGBA8_ETC2_EAC
    if (fHasAlpha) //it has alpha
    {
        bitmap->setConfig(SkBitmap::kETC2_Alpha_Config, region.width(), region.height(), 0, kPremul_SkAlphaType);
    }
    else //data[7] == 0x01), it has no alpha
    {
        internalFormat = 0x9274; // GL_COMPRESSED_RGB8_ETC2
        bitmap->setConfig(SkBitmap::kETC2_Config,region.width(), region.height(), 0, kOpaque_SkAlphaType);
    }
    SkCompressedImageRef *ref = new SkCompressedImageRef(storage.detach(), byteLength, 0, region.width(), region.height(), internalFormat);
    bitmap->setPixelRef(ref)->unref();
    SkAutoLockPixels alp(*bitmap);
    __android_log_print(ANDROID_LOG_INFO, "GFX_ETC2","ETC2 region decoder returned OK for left:%d top:%d width:%d height:%d", region.left(), region.top(), region.width(), region.height());
    return true;
}*/

bool SkIndexedPaletteImageDecoder::onDecode(SkStream* stream, SkBitmap* bm, Mode mode) {
    char data[16];
    if (stream->read(data, 16) != 16) {
        return false;
    }

    uint32_t width   = ((uint32_t)(data[4] & 0xFF)<<8) | (uint32_t)(data[5] & 0xFF);
    uint32_t height  = ((uint32_t)(data[6] & 0xFF)<<8) | (uint32_t)(data[7] & 0xFF);
    uint32_t paletteCount = data[8];
    if (paletteCount == 0) {
        paletteCount = 256;
    }
    int hasAlpha = data[9];
    if (paletteCount > 256)
        return false;

    //unsigned int internalFormat = 0x8B96; // GL_PALETTE8_RGBA8_OES
    if (hasAlpha) //it has alpha
    {
        bm->setConfig(SkBitmap::kIndex8_Config, width, height , 0, kPremul_SkAlphaType);
    }
    else //data[7] == 0x01), it has no alpha
    {
        bm->setConfig(SkBitmap::kIndex8_Config, width, height, 0, kOpaque_SkAlphaType);
    }

    bm->setIsIPT(true) ; //To fix the GIF issue

    if (SkImageDecoder::kDecodeBounds_Mode == mode) {
        // assign these directly, in case we return kDimensions_Result
        return true;
    }

    SkPMColor colorStorage[256];    // worst-case storage
    SkPMColor* colorPtr = colorStorage;

    if (stream->read(colorPtr, paletteCount * 4) != paletteCount * 4) {
        return false;
    }

    SkColorTable* ct = SkNEW_ARGS(SkColorTable, (colorStorage, paletteCount, kPremul_SkAlphaType));
    SkAutoUnref   aur(ct);

    if (!this->allocPixelRef(bm, ct)) {
        return false;
    }

    SkAutoLockPixels alp(*bm);

    uint8_t* dst = bm->getAddr8(0, 0);
    if (stream->read(dst, width * height) != width * height) {
        return false;
    }
#ifdef TEXTURE_COMPRESSION_SUPPORT_DEBUG
    __android_log_print(ANDROID_LOG_INFO, "GFX_Indexed palette","width:%d height:%d",bm->width(), bm->height());
#endif
    return true;
}

static  SkImageDecoder_DecodeReg gReg(Factory);
