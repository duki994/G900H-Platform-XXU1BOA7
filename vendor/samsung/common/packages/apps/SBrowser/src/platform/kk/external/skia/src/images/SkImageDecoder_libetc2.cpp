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
#include <android/log.h>

class SkETC2ImageDecoder : public SkImageDecoder {
public:
    SkETC2ImageDecoder() {
        fInputStream = NULL;
        fOrigWidth = 0;
        fOrigHeight = 0;
        fHasAlpha = 0;
    }

    virtual ~SkETC2ImageDecoder() {
        SkSafeUnref(fInputStream);
    }

#ifdef TEXTURE_COMPRESSION_SUPPORT_DEBUG_WRITE_TO_FILE
    void writeToFile(char*,size_t);
#endif

    virtual Format getFormat() const {
        return (Format)(kLastKnownFormat + 100);
    }

protected:
    virtual bool onBuildTileIndex(SkStreamRewindable* stream, int *width, int *height) SK_OVERRIDE;
    virtual bool onDecodeSubset(SkBitmap* bitmap, const SkIRect& rect) SK_OVERRIDE;
    virtual bool onDecode(SkStream* stream, SkBitmap* bm, Mode mode) SK_OVERRIDE;

private:
    SkStream* fInputStream;
    int fOrigWidth;
    int fOrigHeight;
    int fHasAlpha;
};

static SkImageDecoder* Factory(SkStreamRewindable* stream) {
    static const char kETC2Start[] = {'P', 'K', 'M', ' ', '2', '0'};

    char buffer[sizeof(kETC2Start)];
    size_t startLen = sizeof(kETC2Start);

    if (stream->read(buffer, startLen) == startLen &&
            !memcmp(buffer, kETC2Start, startLen)) {
        char data[10];
        if (stream->read(data, sizeof(data)) != sizeof(data))
            return NULL;

        //int width        = ((int)(data[6] & 0xFF)<<8) | (int)(data[7] & 0xFF);
        //int height       = ((int)(data[8] & 0xFF)<<8) | (int)(data[9] & 0xFF);

        return SkNEW(SkETC2ImageDecoder);
    }
    return NULL;
}

#ifdef TEXTURE_COMPRESSION_SUPPORT_DEBUG_WRITE_TO_FILE
void SkETC2ImageDecoder::writeToFile(char* stream , size_t length)
{
    char pExtSdCardPath[256];
    //static int nFileIdentifer=1;
    static int temp=1;

    snprintf(pExtSdCardPath, 254, "/sdcard/pkmfiledest/test%d.pkm",temp++);

        SkFILEWStream *fileWStream = new  SkFILEWStream(pExtSdCardPath);
        //if( 1 )
        if(fileWStream->isValid())
        {
            SkDebugf("Writing pkm:%d",temp);
            //int nBufferSize = stream->getLength();
            int nBufferSize=length;
            char *pBuffer = stream;

            if ( pBuffer )
            {
                /* read the content from the stream & write to the fileWStream */
                //stream->read( pBuffer, nBufferSize );
                fileWStream->write( pBuffer, nBufferSize );

            }
            else
                SkDebugf("%s : %s ( Error: Could not allocate buffer to store stream data )",__FILE__,__FUNCTION__);
        }
        else
        {
            /* We were unable to open the file path */
            SkDebugf("\n %s : %s ( Error: Could not open the file path )",__FILE__,__FUNCTION__);
        }

        /* Delete teh new fileWStream object and also rewind the incoming stream */
        delete fileWStream;
        //stream->rewind();
}
#endif

bool SkETC2ImageDecoder::onBuildTileIndex(SkStreamRewindable* stream,
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

bool SkETC2ImageDecoder::onDecodeSubset(SkBitmap* decodedBitmap,
        const SkIRect& region) {
    SkIRect rect = SkIRect::MakeWH(fOrigWidth, fOrigHeight);

    if (!rect.intersect(region)) {
        // If the requested region is entirely outsides the image, return false
        return false;
    }

    // Note: sampling ignored
    /*int width = fOrigWidth;
    int height = fOrigHeight;*/


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
    SkCompressedImageRef *ref = new SkCompressedImageRef(storage.detach(), byteLength, 0, region.width(), region.height(),internalFormat);
    
    bitmap->setPixelRef(ref)->unref();
    SkAutoLockPixels alp(*bitmap);
#ifdef TEXTURE_COMPRESSION_SUPPORT_DEBUG
    __android_log_print(ANDROID_LOG_INFO, "GFX_ETC2","ETC2 region decoder returned OK for left:%d top:%d width:%d height:%d", region.left(), region.top(), region.width(), region.height());
#endif
    return true;
}

bool SkETC2ImageDecoder::onDecode(SkStream* stream, SkBitmap* bm, Mode mode) {
    char data[16];
    if (stream->read(data, 16) != 16) {
        return false;
    }

    int width   = ((int)(data[12] & 0xFF)<<8) | (int)(data[13] & 0xFF);
    int height  = ((int)(data[14] & 0xFF)<<8) | (int)(data[15] & 0xFF);
    unsigned int internalFormat = 0x9278; // GL_COMPRESSED_RGBA8_ETC2_EAC
    if (data[7] == 0x03) //it has alpha
    {
        bm->setConfig(SkBitmap::kETC2_Alpha_Config, width, height , 0, kPremul_SkAlphaType);
    }
    else //data[7] == 0x01), it has no alpha
    {
        internalFormat = 0x9274; // GL_COMPRESSED_RGB8_ETC2
        bm->setConfig(SkBitmap::kETC2_Config, width, height, 0, kOpaque_SkAlphaType);
    }

    if (SkImageDecoder::kDecodeBounds_Mode == mode) {
        // assign these directly, in case we return kDimensions_Result
        return true;
    }

    size_t length = 0; // texture length is data after header
    if (stream->hasLength()) {
        length = stream->getLength() - 16;
    } else {
        if (internalFormat == 0x9274) // GL_COMPRESSED_RGB8_ETC2
            length = ((width + 3) / 4 * 4) * ((height + 3) / 4 * 4) / 2; // 4 bits per pixel
        else // 0x9278: // GL_COMPRESSED_RGBA8_ETC2_EAC
            length = ((width + 3) / 4 * 4) * ((height + 3) / 4 * 4);
    }

    // read all the rest of bitmap pixels
    SkAutoMalloc storage(length);

    if (stream->read(storage.get(), length) != length) {
        return false;
    }

    // FIXME reusing bitmap
    SkCompressedImageRef *ref = new SkCompressedImageRef(storage.detach(), length, 0, width, height,internalFormat);
    bm->setPixelRef(ref)->unref();
    SkAutoLockPixels alp(*bm);

#ifdef TEXTURE_COMPRESSION_SUPPORT_DEBUG
    SkDebugf("GFX etc2 bitmap created width:%d height:%d bitmap id is %d \n",bm->width(),bm->height(),bm->getGenerationID());
#ifdef TEXTURE_COMPRESSION_SUPPORT_DEBUG_WRITE_TO_FILE
    writeToFile(data,length);
#endif
#endif
    return true;
}

static SkImageDecoder_DecodeReg gReg(Factory);
