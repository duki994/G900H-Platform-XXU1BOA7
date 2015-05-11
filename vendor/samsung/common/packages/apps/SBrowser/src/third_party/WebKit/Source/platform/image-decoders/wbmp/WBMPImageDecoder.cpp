/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WBMPImageDecoder.h"

namespace WebCore {

static inline unsigned wbmpAlign8(unsigned x)
{
    return ((x + 7) & ~7);
}

WBMPImageDecoder::WBMPImageDecoder(ImageSource::AlphaOption alphaOption,
    ImageSource::GammaAndColorProfileOption gammaAndColorProfileOption,
    size_t maxDecodedBytes)
    :ImageDecoder(alphaOption, gammaAndColorProfileOption, maxDecodedBytes)
{
}

static void expandBitsToBytes(uint8_t dst[], const uint8_t src[], int bits)
{
    int bytes = bits >> 3;

    for (int i = 0; i < bytes; ++i) {
        unsigned mask = *src++;
        dst[0] = (mask >> 7) & 1;
        dst[1] = (mask >> 6) & 1;
        dst[2] = (mask >> 5) & 1;
        dst[3] = (mask >> 4) & 1;
        dst[4] = (mask >> 3) & 1;
        dst[5] = (mask >> 2) & 1;
        dst[6] = (mask >> 1) & 1;
        dst[7] = (mask >> 0) & 1;
        dst += 8;
    }

    bits &= 7;
    if (bits > 0) {
        unsigned mask = *src;
        do {
            *dst++ = (mask >> 7) & 1;
            mask <<= 1;
        } while (--bits);
    }
}

static IntSize readMbf(const char* contents)
{
    // Skip the header as it is of two bytes.
    const char* temp = contents + 2;

    // Calculate width
    unsigned width = 0;
    do {
        width = (width << 7) | (*temp & 0x7F);
    } while (*temp++ & 0x80);

    // Calculate height
    unsigned height = 0;
    do {
        height = (height << 7) | (*temp & 0x7F);
    } while (*temp++ & 0x80);

    return IntSize(width, height);
}


bool WBMPImageDecoder::decode(bool onlysize)
{
    const SharedBuffer& data = *m_data;

    if (onlysize) {
        IntSize sizeWbmp = readMbf(data.data());
        ImageDecoder::setSize(sizeWbmp.width(), sizeWbmp.height());
        return false;
    }

    //samsung changes:: start
    // with progressive image decoding we can observe corruption while loading the image first time
    //to avoid progressive image decoding
    if (!ImageDecoder::isAllDataReceived()) {
       return false;
    }
    //samsung changes::end

    if (!data.size())
        return setFailed();

    //samsung changes:: start
    //we should not blindly add 4 bytes. we may get less bytes images also
    //const char* src = data.data() + 4;
    //samsung changes::end
    unsigned width = ImageDecoder::size().width();
    size_t srcRB = wbmpAlign8(width) >> 3;

    ImageFrame& buffer = m_frameBufferCache[0];

    ASSERT(buffer.status() != ImageFrame::FrameComplete);

    unsigned height = ImageDecoder::size().height();
    if (buffer.status() == ImageFrame::FrameEmpty) {
        if (!buffer.setSizeForWBMP(width, height))
            return ImageDecoder::setFailed();
        buffer.setStatus(ImageFrame::FramePartial);
        buffer.setHasAlpha(false);
    }
    unsigned char* dest = static_cast<unsigned char*>(buffer.getAddr8(0, 0));

    //samsung changes::start
    const char* src = data.data() + data.size() - (srcRB * height);
    //samsung changes::end
    for (unsigned y = 0; y < height; ++y) {
        expandBitsToBytes(dest, reinterpret_cast<const unsigned char*>(src), width);
        dest += buffer.rowBytes();
        src += srcRB;
    }

    if (!m_frameBufferCache.isEmpty())
        m_frameBufferCache[0].setStatus(ImageFrame::FrameComplete);

    return true;
}

ImageFrame* WBMPImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index)
        return 0;

    if (m_frameBufferCache.isEmpty()) {
        m_frameBufferCache.resize(1);
        m_frameBufferCache[0].setPremultiplyAlpha(m_premultiplyAlpha);
    }

    ImageFrame& frame = m_frameBufferCache[0];
    if (frame.status() != ImageFrame::FrameComplete)
        decode(false);
    return &frame;
}
bool WBMPImageDecoder::isSizeAvailable()
{
    if (!ImageDecoder::isSizeAvailable())
        decode(true);

    return ImageDecoder::isSizeAvailable();
}

}
