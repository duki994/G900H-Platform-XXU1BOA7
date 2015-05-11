/*
 * Implementation of pixel ref holding allocation (and/or texture) for compressed images.
 * Optionally, once the texture is uploaded to GPU, free memory on CPU side.
 *
 * grep for GRAPHICS_COMPRESSION_START in source code to follow related changes.
 *
 * Copyright 2013 Samsung Electronics
 * @author aleksandar.s@samsung.com
 */

#ifndef SkCompressedImageRef_DEFINED
#define SkCompressedImageRef_DEFINED

#include "SkPixelRef.h"
#include "SkBitmap.h"
#include "SkImageDecoder.h"
#include "SkString.h"

class SkCompressedImageRef : public SkPixelRef {
public:
    /*
     * byteLength is size of compressed testure information starting
     * from byteOffset position within storage.
     * byteOffset + byteLength == size of storage in bytes
     */
    SkCompressedImageRef(void *storage, size_t byteLength, unsigned int byteOffset, size_t width, size_t height, unsigned int internalFormat);
    virtual ~SkCompressedImageRef();

    size_t getPixelsByteSize() const;

    /*
     * This is width that is supplied to glCompressedTexImage2D. Often different
     * from bitmap width - e.g. alpha in texture atlas, scaling etc.
     */
    size_t getWidth() const;
    /*
     * This is height that is supplied to glCompressedTexImage2D. Often different
     * from bitmap width - e.g. alpha in texture atlas, scaling etc.
     * For ETC1 with alpha in atlas this is 2x of bitmap height as alpha is below the RGB.
     * texture
     */
    size_t getHeight() const;

    unsigned int getInternalFormat() const;

    SK_DECLARE_UNFLATTENABLE_OBJECT()

protected:
    /*  Overrides from SkPixelRef
     */
    virtual void* onLockPixels(SkColorTable**);
    // override this in your subclass to clean up when we're unlocking pixels
    virtual void onUnlockPixels();

private:
    void*           fStorage;
    size_t          fContentSize;
    unsigned int    fContentOffset;
    // \sa getWidth
    size_t          width;
    // \sa getHeight
    size_t          height;
    
    // \sa glCompressedTexImage2D
    unsigned int internalFormat;

    typedef SkPixelRef INHERITED;
};

#endif
