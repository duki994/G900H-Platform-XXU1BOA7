/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef WTF_RawPtr_h
#define WTF_RawPtr_h

// Ptr is a simple wrapper for a raw pointer that provides the
// interface (get, clear) of other pointer types such as RefPtr,
// Persistent and Member. This is used for the Blink garbage
// collection work in order to be able to write shared code that will
// use reference counting or garbage collection based on a
// compile-time flag.

namespace WTF {

template<typename T>
class RawPtr {
public:
    RawPtr() : m_ptr(0) { }
    RawPtr(T* ptr) : m_ptr(ptr) { }
    RawPtr(const RawPtr& other)
        : m_ptr(other.get())
    {
    }
    template<typename U>
    RawPtr(const RawPtr<U>& other)
        : m_ptr(other.get())
    {
    }

    T* get() const { return m_ptr; }
    void clear() { m_ptr = 0; }
    // FIXME: oilpan: Remove release and leakRef once we remove RefPtrWillBeRawPtr.
    RawPtr<T> release()
    {
        RawPtr<T> tmp = m_ptr;
        m_ptr = 0;
        return tmp;
    }
    T* leakRef()
    {
        T* ptr = m_ptr;
        m_ptr = 0;
        return ptr;
    }

    template<typename U>
    RawPtr& operator=(U* ptr)
    {
        m_ptr = ptr;
        return *this;
    }

    template<typename U>
    RawPtr& operator=(RawPtr<U> ptr)
    {
        m_ptr = ptr.get();
        return *this;
    }

    operator T*() const { return m_ptr; }
    T& operator*() const { return *m_ptr; }
    T* operator->() const { return m_ptr; }

private:
    T* m_ptr;
};

} // namespace WTF

using WTF::RawPtr;

#endif
