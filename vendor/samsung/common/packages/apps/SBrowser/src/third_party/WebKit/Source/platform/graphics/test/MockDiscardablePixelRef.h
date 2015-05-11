/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MockDiscardablePixelRef_h

#include "SkBitmap.h"
#include "SkPixelRef.h"

namespace WebCore {

class MockDiscardablePixelRef : public SkPixelRef {
public:
    MockDiscardablePixelRef(const SkImageInfo& info, size_t rowbytes)
        : SkPixelRef(info)
        , m_rowBytes(rowbytes)
        , discarded(false)
    {
        setURI("discardable");
    }

    ~MockDiscardablePixelRef() { }

    void discard()
    {
        ASSERT(!m_lockedMemory);
        discarded = true;
    }

    class Allocator : public SkBitmap::Allocator {
    public:
        virtual bool allocPixelRef(SkBitmap* dst, SkColorTable* ct) SK_OVERRIDE {
            SkImageInfo info;
            if (!dst->asImageInfo(&info)) {
                return false;
            }
            SkAutoTUnref<SkPixelRef> pr(new MockDiscardablePixelRef(info, dst->rowBytes()));
            dst->setPixelRef(pr);
            return true;
        }
    };

    SK_DECLARE_UNFLATTENABLE_OBJECT()

protected:
    virtual bool onNewLockPixels(LockRec* rec)
    {
        if (discarded)
            return false;
        m_lockedMemory = &discarded;
        rec->fPixels = m_lockedMemory;
        rec->fColorTable = 0;
        rec->fRowBytes = m_rowBytes;
        return true;
    }

    virtual void onUnlockPixels()
    {
        m_lockedMemory = 0;
    }

private:
    void* m_lockedMemory;
    size_t m_rowBytes;
    bool discarded;
};

} // namespace WebCore

#endif // MockDiscardablePixelRef_h
