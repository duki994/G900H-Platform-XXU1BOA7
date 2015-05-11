/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkImagePriv.h"
#include "SkCanvas.h"
#include "SkPicture.h"

SkBitmap::Config SkColorTypeToBitmapConfig(SkColorType colorType) {
    switch (colorType) {
        case kAlpha_8_SkColorType:
            return SkBitmap::kA8_Config;

        case kARGB_4444_SkColorType:
            return SkBitmap::kARGB_4444_Config;

        case kRGB_565_SkColorType:
            return SkBitmap::kRGB_565_Config;

        case kN32_SkColorType:
            return SkBitmap::kARGB_8888_Config;

        case kIndex_8_SkColorType:
            return SkBitmap::kIndex8_Config;

        default:
            // break for unsupported colortypes
            break;
    }
    return SkBitmap::kNo_Config;
}

SkBitmap::Config SkImageInfoToBitmapConfig(const SkImageInfo& info) {
    return SkColorTypeToBitmapConfig(info.fColorType);
}

SkColorType SkBitmapConfigToColorType(SkBitmap::Config config) {
    static const SkColorType gCT[] = {
        kUnknown_SkColorType,   // kNo_Config
        kAlpha_8_SkColorType,   // kA8_Config
        kIndex_8_SkColorType,   // kIndex8_Config
        kRGB_565_SkColorType,   // kRGB_565_Config
        kARGB_4444_SkColorType, // kARGB_4444_Config
        kN32_SkColorType,   // kARGB_8888_Config
    };
    SkASSERT((unsigned)config < SK_ARRAY_COUNT(gCT));
    return gCT[config];
}

SkImage* SkNewImageFromBitmap(const SkBitmap& bm, bool canSharePixelRef) {
    SkImageInfo info;
    if (!bm.asImageInfo(&info)) {
        return NULL;
    }

    SkImage* image = NULL;
    if (canSharePixelRef || bm.isImmutable()) {
        image = SkNewImageFromPixelRef(info, bm.pixelRef(), bm.rowBytes());
    } else {
        bm.lockPixels();
        if (bm.getPixels()) {
            image = SkImage::NewRasterCopy(info, bm.getPixels(), bm.rowBytes());
        }
        bm.unlockPixels();
    }
    return image;
}
