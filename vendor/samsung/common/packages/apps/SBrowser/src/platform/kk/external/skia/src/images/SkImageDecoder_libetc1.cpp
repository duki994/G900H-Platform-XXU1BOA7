/*
 * Implementation of ETC1 compression loader
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



class SkETC1ImageDecoder : public SkImageDecoder {
public:
	SkETC1ImageDecoder() {
	}

#ifdef TEXTURE_COMPRESSION_SUPPORT_DEBUG_WRITE_TO_FILE
    void writeToFile(char*,size_t);
#endif

    virtual Format getFormat() const {
        return (Format)(kLastKnownFormat + 100);
    }

protected:
    virtual bool onDecode(SkStream* stream, SkBitmap* bm, Mode mode);
};

static SkImageDecoder* Factory(SkStreamRewindable* stream) {
    static const char kETC1Start[] = {'P', 'K', 'M', ' ', '1', '0'};

    size_t len = stream->getLength();

    char buffer[sizeof(kETC1Start)];
    size_t startLen = sizeof(kETC1Start);

    if (len > 16 &&
            stream->read(buffer, startLen) == startLen &&
            !memcmp(buffer, kETC1Start, startLen)) {
        char data[10];
        if (stream->read(data, sizeof(data)) != sizeof(data))
            return NULL;

        // FIXME temporary limit size here - no need later for size check
        //int width        = ((int)(data[6] & 0xFF)<<8) | (int)(data[7] & 0xFF);
        //int height       = ((int)(data[8] & 0xFF)<<8) | (int)(data[9] & 0xFF);

        return SkNEW(SkETC1ImageDecoder);
    }
    return NULL;
}

#ifdef TEXTURE_COMPRESSION_SUPPORT_DEBUG_WRITE_TO_FILE
void SkETC1ImageDecoder::writeToFile(char* stream , size_t length)
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

bool SkETC1ImageDecoder::onDecode(SkStream* stream, SkBitmap* bm, Mode mode) {
    size_t length = stream->getLength();

    if (SkImageDecoder::kDecodeBounds_Mode == mode) {
        char data[16];
        if (stream->read(data, 16) != 16) {
            return false;
        }
        // assign these directly, in case we return kDimensions_Result
        bm->setConfig(SkBitmap::kETC1_AlphaAtlas_Config,
                ((int)(data[12] & 0xFF)<<8) | (int)(data[13] & 0xFF),
                ((int)(data[14] & 0xFF)<<8) | (int)(data[15] & 0xFF), kPremul_SkAlphaType);
        return true;
    }

    // read all and set as bitmap's pixels
    SkAutoMalloc storage(length);

    if (stream->read(storage.get(), length) != length) {
        return false;
    }
    char *data = (char*)storage.get();
    unsigned int width        = ((unsigned int)(data[12] & 0xFF)<<8) | (unsigned int)(data[13] & 0xFF);
    unsigned int height       = ((unsigned int)(data[14] & 0xFF)<<8) | (unsigned int)(data[15] & 0xFF);
    unsigned int paddedWidth  = ((unsigned int)(data[ 8] & 0xFF)<<8) | (unsigned int)(data[ 9] & 0xFF);
    unsigned int paddedHeight = ((unsigned int)(data[10] & 0xFF)<<8) | (unsigned int)(data[11] & 0xFF);


    unsigned int byteSize = paddedWidth * paddedHeight >> 1;
    if (byteSize < 32)
        byteSize = 32;

    // misusing byte on offset 6 (type) for holding information about whether there is alpha in atlas or not.
    // check build scripts for details
    if (data[6] == 0x0F)
    {
        // by default, it has alpha packaged under in the atlas
        bm->setConfig(SkBitmap::kETC1_AlphaAtlas_Config, width, height >> 1, 0, kPremul_SkAlphaType);
    }
    else
    {
        // doesn't have alpha packaged in the atlas.
        bm->setConfig(SkBitmap::kETC1_Config, width, height, 0, kOpaque_SkAlphaType);
    }
    // FIXME reusing bitmap
    if (byteSize != length -16) {
        __android_log_print(ANDROID_LOG_WARN, "GFX ETC1 image decoder size PKM file mismatch for:", "%d stream size, %d width, %d height, %d type, %d byteSize", length - 16, width, height, data[6], byteSize);
        byteSize = (byteSize < length - 16) ? byteSize : length - 16;
    }
    SkCompressedImageRef *ref = new SkCompressedImageRef(storage.detach(), byteSize, 16, width, height, 0x8D64);
    bm->setPixelRef(ref)->unref();
    SkAutoLockPixels alp(*bm);

#ifdef TEXTURE_COMPRESSION_SUPPORT_DEBUG
    SkDebugf("etc1 bitmap created the %d \n",bm->getGenerationID());
#ifdef TEXTURE_COMPRESSION_SUPPORT_DEBUG_WRITE_TO_FILE
    writeToFile(data,length);
#endif
#endif
    return true;
}

static SkImageDecoder_DecodeReg gReg(Factory);


