/*
 * Implementation of pixel ref holding allocation (and/or texture) for compressed images.
 * Optionally, once the texture is uploaded to GPU, free memory on CPU side.
 *
 * grep for GRAPHICS_COMPRESSION_START in source code to follow related changes.
 *
 * Copyright 2013 Samsung Electronics
 * @author aleksandar.s@samsung.com
 */

#include "SkImageRef.h"
#include "SkBitmap.h"
#include "SkCompressedImageRef.h"

//GRAPHICS_COMPRESSION_START
#include <android/log.h>
//GRAPHICS_COMPRESSION_END

///////////////////////////////////////////////////////////////////////////////

#include "SkBitmap.h"
#include <android/log.h>

SkCompressedImageRef::SkCompressedImageRef(void* storage, size_t byteLength, unsigned int byteOffset, size_t width, size_t height, unsigned int internalFormat) {
    if (NULL == storage) {
        storage = sk_malloc_throw(byteLength);

    }
    fStorage = storage;
    fContentSize = byteLength;
    fContentOffset = byteOffset;
    setImmutable();
    this->width = width;
    this->height = height;
    this->internalFormat = internalFormat;
    this->setPreLocked((void*)(((char*)fStorage) + fContentOffset), NULL);
}

SkCompressedImageRef::~SkCompressedImageRef() {
    sk_free(fStorage);
}

void* SkCompressedImageRef::onLockPixels(SkColorTable** ct) {
    // doesn't matter as set prelocked
    return (void*)(((char*)fStorage) + fContentOffset);
}

void SkCompressedImageRef::onUnlockPixels() {
    // nothing to do
}

size_t SkCompressedImageRef::getPixelsByteSize() const
{
    return fContentSize;
}

size_t SkCompressedImageRef::getHeight() const
{
    return height;
}

size_t SkCompressedImageRef::getWidth() const
{
    return width;
}

unsigned int SkCompressedImageRef::getInternalFormat() const
{
    return internalFormat;
}


