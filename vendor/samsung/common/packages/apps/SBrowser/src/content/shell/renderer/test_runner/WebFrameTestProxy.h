// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBFRAMETESTPROXY_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_WEBFRAMETESTPROXY_H_

#include "base/basictypes.h"
#include "content/shell/renderer/test_runner/WebTestProxy.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace WebTestRunner {

// Templetized wrapper around RenderFrameImpl objects, which implement
// the WebFrameClient interface.
template<class Base, typename P, typename R>
class WebFrameTestProxy : public Base {
public:
    WebFrameTestProxy(P p, R r)
        : Base(p, r)
        , m_baseProxy(0)
        , m_version(0) { }

    virtual ~WebFrameTestProxy() { }

    void setBaseProxy(WebTestProxyBase* proxy)
    {
        m_baseProxy = proxy;
    }

    void setVersion(int version)
    {
        m_version = version;
    }

    blink::WebPlugin* createPlugin(blink::WebFrame* frame, const blink::WebPluginParams& params)
    {
        blink::WebPlugin* plugin = m_baseProxy->createPlugin(frame, params);
        if (plugin)
            return plugin;
        return Base::createPlugin(frame, params);
    }

    // WebFrameClient implementation.
    virtual bool canCreatePluginWithoutRenderer(const blink::WebString& mimeType)
    {
        using blink::WebString;

        const CR_DEFINE_STATIC_LOCAL(WebString, suffix, ("-can-create-without-renderer"));
        return mimeType.utf8().find(suffix.utf8()) != std::string::npos;
    }
    virtual void didStartProvisionalLoad(blink::WebFrame* frame)
    {
        if (m_version > 2)
            m_baseProxy->didStartProvisionalLoad(frame);
        Base::didStartProvisionalLoad(frame);
    }
    virtual void didReceiveServerRedirectForProvisionalLoad(blink::WebFrame* frame)
    {
        m_baseProxy->didReceiveServerRedirectForProvisionalLoad(frame);
        Base::didReceiveServerRedirectForProvisionalLoad(frame);
    }
    virtual void didFailProvisionalLoad(blink::WebFrame* frame, const blink::WebURLError& error)
    {
        Base::didFailProvisionalLoad(frame, error);
    }
    virtual void didCommitProvisionalLoad(blink::WebFrame* frame, bool isNewNavigation)
    {
        Base::didCommitProvisionalLoad(frame, isNewNavigation);
    }
    virtual void didReceiveTitle(blink::WebFrame* frame, const blink::WebString& title, blink::WebTextDirection direction)
    {
        Base::didReceiveTitle(frame, title, direction);
    }
    virtual void didChangeIcon(blink::WebFrame* frame, blink::WebIconURL::Type iconType)
    {
        Base::didChangeIcon(frame, iconType);
    }
    virtual void didFinishDocumentLoad(blink::WebFrame* frame)
    {
        Base::didFinishDocumentLoad(frame);
    }
    virtual void didHandleOnloadEvents(blink::WebFrame* frame)
    {
        Base::didHandleOnloadEvents(frame);
    }
    virtual void didFailLoad(blink::WebFrame* frame, const blink::WebURLError& error)
    {
        Base::didFailLoad(frame, error);
    }
    virtual void didFinishLoad(blink::WebFrame* frame)
    {
        Base::didFinishLoad(frame);
    }
    virtual void didDetectXSS(blink::WebFrame* frame, const blink::WebURL& insecureURL, bool didBlockEntirePage)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->didDetectXSS(frame, insecureURL, didBlockEntirePage);
        Base::didDetectXSS(frame, insecureURL, didBlockEntirePage);
    }
    virtual void didDispatchPingLoader(blink::WebFrame* frame, const blink::WebURL& url)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->didDispatchPingLoader(frame, url);
        Base::didDispatchPingLoader(frame, url);
    }
    virtual void willRequestResource(blink::WebFrame* frame, const blink::WebCachedURLRequest& request)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->willRequestResource(frame, request);
        Base::willRequestResource(frame, request);
    }
    virtual void didCreateDataSource(blink::WebFrame* frame, blink::WebDataSource* ds)
    {
        Base::didCreateDataSource(frame, ds);
    }
    virtual void willSendRequest(blink::WebFrame* frame, unsigned identifier, blink::WebURLRequest& request, const blink::WebURLResponse& redirectResponse)
    {
        m_baseProxy->willSendRequest(frame, identifier, request, redirectResponse);
        Base::willSendRequest(frame, identifier, request, redirectResponse);
    }
    virtual void didReceiveResponse(blink::WebFrame* frame, unsigned identifier, const blink::WebURLResponse& response)
    {
        m_baseProxy->didReceiveResponse(frame, identifier, response);
        Base::didReceiveResponse(frame, identifier, response);
    }
    virtual void didChangeResourcePriority(blink::WebFrame* frame, unsigned identifier, const blink::WebURLRequest::Priority& priority)
    {
        // This is not implemented in RenderFrameImpl, so need to explicitly call
        // into the base proxy.
        m_baseProxy->didChangeResourcePriority(frame, identifier, priority);
        Base::didChangeResourcePriority(frame, identifier, priority);
    }
    virtual void didFinishResourceLoad(blink::WebFrame* frame, unsigned identifier)
    {
        Base::didFinishResourceLoad(frame, identifier);
    }
    virtual blink::WebNavigationPolicy decidePolicyForNavigation(blink::WebFrame* frame, blink::WebDataSource::ExtraData* extraData, const blink::WebURLRequest& request, blink::WebNavigationType type, blink::WebNavigationPolicy defaultPolicy, bool isRedirect)
    {
        return Base::decidePolicyForNavigation(frame, extraData, request, type, defaultPolicy, isRedirect);
    }
    virtual bool willCheckAndDispatchMessageEvent(blink::WebFrame* sourceFrame, blink::WebFrame* targetFrame, blink::WebSecurityOrigin target, blink::WebDOMMessageEvent event)
    {
        if (m_baseProxy->willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event))
            return true;
        return Base::willCheckAndDispatchMessageEvent(sourceFrame, targetFrame, target, event);
    }

private:
    WebTestProxyBase* m_baseProxy;

    // This is used to incrementally change code between Blink and Chromium.
    // It is used instead of a #define and is set by layouttest_support when
    // creating this object.
    int m_version;

    DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxy);
};

}

#endif // WebTestProxy_h
