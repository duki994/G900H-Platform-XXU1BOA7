/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkStream.h"
#include "SkStreamHelpers.h"
#include "SkTypes.h"

size_t CopyStreamToStorage(SkAutoMalloc* storage, SkStream* stream) {
    SkASSERT(storage != NULL);
    SkASSERT(stream != NULL);

    if (stream->hasLength()) {
        size_t nReadSize = 0;
        const size_t length = stream->getLength();

        /* Skia: Some of the streams return true for hasLength but
         * the actual length value is undefined. Such a scenario
         * results in no allocation done at all or a crash within
         * sk_malloc_flags. Hence we check for the length of the
         * stream.
         */
        if (length) {

            /* Skia: Allocate the required bytes. We deviate from
             * default skia behavior by calilng reset_nothrow. The
             * _nothrow doesn't cause application to abort when memory
             * allocation fails. Hence the caller is responsible for
             * checking whether the returned memory's validity. 
             */
            void* dst = storage->reset_nothrow(length);
            if ( dst != NULL ) {
                /* Skia: Memory allocation has succeeded. We proceed
                 * to read required bytes and check if the stream
                 * interface provided the required bytes. 
                 */
                nReadSize = stream->read(dst, length);
                if (nReadSize != length) {
                    /* Skia: The stream did not provide sufficient data */
                    nReadSize = 0;
                }
            }else {
                /* Skia: We also print the log to identify root cause */
                SkDebugf("%s : Memory allocation failed to provide required size.",__FUNCTION__);
            }
        }

        /* Skia: Return bytes read from the stream */
        return nReadSize;
    }

    SkDynamicMemoryWStream tempStream;
    // Arbitrary buffer size.
    const size_t bufferSize = 256 * 1024; // 256KB
    char buffer[bufferSize];
    SkDEBUGCODE(size_t debugLength = 0;)
    do {
        size_t bytesRead = stream->read(buffer, bufferSize);
        tempStream.write(buffer, bytesRead);
        SkDEBUGCODE(debugLength += bytesRead);
        SkASSERT(tempStream.bytesWritten() == debugLength);
    } while (!stream->isAtEnd());
    const size_t length = tempStream.bytesWritten();
    void* dst = storage->reset(length);
    tempStream.copyTo(dst);
    return length;
}
