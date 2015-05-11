/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd All Rights Reserved
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

#if ENABLE(BING_SEARCH_ENGINE_SETTING_FROM_JS)
#include "modules/bingsearchengine/DOMWindowBingSearchEngine.h"

#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"
#include "core/page/Chrome.h"
#include "core/page/Page.h"
#include "modules/bingsearchengine/DOMWindowBingSearchEngineClient.h"

namespace WebCore {

bool DOMWindowBingSearchEngine::isBingCurrentSearchDefault(DOMWindow* window)
{
    Frame* frame = window->frame();
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;

    return DOMWindowBingSearchEngine::from(page)->client()->isBingCurrentSearchDefault();
}

bool DOMWindowBingSearchEngine::setBingCurrentSearchDefault(DOMWindow* window)
{
    Frame* frame = window->frame();
    if (!frame)
        return false;

    Page* page = frame->page();
    if (!page)
        return false;

    if (DOMWindowBingSearchEngine::from(page)->client()->isBingCurrentSearchDefault()) {
        // Bing is already set as default search engine, the documentation says we should return
        // false in this case.
        return false;
    }

    return DOMWindowBingSearchEngine::from(page)->client()->setBingAsCurrentSearchDefault();
}

PassRefPtr<DOMWindowBingSearchEngine> DOMWindowBingSearchEngine::create(DOMWindowBingSearchEngineClient* client)
{
    return adoptRef(new DOMWindowBingSearchEngine(client));
}

const char* DOMWindowBingSearchEngine::supplementName()
{
    return "WindowBingSearchEngine";
}

DOMWindowBingSearchEngine* DOMWindowBingSearchEngine::from(Page* page)
{
    return static_cast<DOMWindowBingSearchEngine*>(RefCountedSupplement<Page, DOMWindowBingSearchEngine>::from(page, DOMWindowBingSearchEngine::supplementName()));
}

void provideDOMWindowBingSearchEngineTo(Page* page, DOMWindowBingSearchEngineClient* client)
{
    RefCountedSupplement<Page, DOMWindowBingSearchEngine>::provideTo(page, DOMWindowBingSearchEngine::supplementName(), DOMWindowBingSearchEngine::create(client));
}

} // namespace WebCore

#endif // ENABLE(BING_SEARCH_ENGINE_SETTING_FROM_JS)
