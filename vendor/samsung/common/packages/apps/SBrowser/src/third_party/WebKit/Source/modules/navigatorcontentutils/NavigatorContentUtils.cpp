/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 * Copyright (C) 2012, Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "config.h"
#include "modules/navigatorcontentutils/NavigatorContentUtils.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/Frame.h"
#include "core/frame/Navigator.h"
#include "core/page/Page.h"
#include "wtf/HashSet.h"

namespace WebCore {

static HashSet<String>* protocolWhitelist;
//S_HTML5_CUSTOM_HANDLER_SUPPORT - START
static HashSet<String>* ContentBlacklist;
//S_HTML5_CUSTOM_HANDLER_SUPPORT - END

static void initProtocolHandlerWhitelist()
{
    protocolWhitelist = new HashSet<String>;
    static const char* const protocols[] = {
        "bitcoin",
        "geo",
        "im",
        "irc",
        "ircs",
        "magnet",
        "mailto",
        "mms",
        "news",
        "nntp",
        "sip",
        "sms",
        "smsto",
        "ssh",
        "tel",
        "urn",
        "webcal",
        "wtai",
        "xmpp",
    };
    for (size_t i = 0; i < WTF_ARRAY_LENGTH(protocols); ++i)
        protocolWhitelist->add(protocols[i]);
}

//S_HTML5_CUSTOM_HANDLER_SUPPORT - START
static void initContentHandlerBlacklist()
{
    ContentBlacklist = new HashSet<String>;
    static const char* mimeType[] = {
        "application/x-www-form-urlencoded",
        "application/xhtml+xml",
        "application/xml",
        "image/gif",
        "image/jpeg",
        "image/png",
        "image/svg+xml",
        "multipart/x-mixed-replace",
        "text/cache-manifest",
        "text/css",
        "text/html",
        "text/ping",
        "text/plain",
        "text/xml"
    };

    for (size_t i = 0; i < WTF_ARRAY_LENGTH(mimeType); ++i)
        ContentBlacklist->add(mimeType[i]);
}
//S_HTML5_CUSTOM_HANDLER_SUPPORT - END

static bool verifyCustomHandlerURL(const KURL& baseURL, const String& url, ExceptionState& exceptionState)
{
    // The specification requires that it is a SyntaxError if the "%s" token is
    // not present.
    static const char token[] = "%s";
    int index = url.find(token);
    if (-1 == index) {
        exceptionState.throwDOMException(SyntaxError, "The url provided ('" + url + "') does not contain '%s'.");
        return false;
    }

    // It is also a SyntaxError if the custom handler URL, as created by removing
    // the "%s" token and prepending the base url, does not resolve.
    String newURL = url;
    newURL.remove(index, WTF_ARRAY_LENGTH(token) - 1);

    KURL kurl(baseURL, newURL);

    if (kurl.isEmpty() || !kurl.isValid()) {
        exceptionState.throwDOMException(SyntaxError, "The custom handler URL created by removing '%s' and prepending '" + baseURL.string() + "' is invalid.");
        return false;
    }

#if defined(S_HTML5_CUSTOM_HANDLER_SUPPORT)
	RefPtr<SecurityOrigin>baseUrlOrigin = SecurityOrigin::create(baseURL);
	RefPtr<SecurityOrigin>protoUrlOrigin = SecurityOrigin::create(kurl);
	if(!protoUrlOrigin.get()->canAccess(baseUrlOrigin.get())){
		exceptionState.throwDOMException(SyntaxError, "The custom handler URL created by removing '%s' and prepending '" + baseURL.string() + "' is invalid(origin does not match).");
        return false;
	}
#endif	

    return true;
}

static bool isProtocolWhitelisted(const String& scheme)
{
    if (!protocolWhitelist)
        initProtocolHandlerWhitelist();
    return protocolWhitelist->contains(scheme);
}

//S_HTML5_CUSTOM_HANDLER_SUPPORT - START
static bool isContentBlacklisted(const String& mimeType)
{
    if (!ContentBlacklist)
        initContentHandlerBlacklist();
    return ContentBlacklist->contains(mimeType);
}
//S_HTML5_CUSTOM_HANDLER_SUPPORT - END

static bool verifyProtocolHandlerScheme(const String& scheme, const String& method, ExceptionState& exceptionState)
{
    if (scheme.startsWith("web+")) {
        // The specification requires that the length of scheme is at least five characteres (including 'web+' prefix).
        if (scheme.length() >= 5 && isValidProtocol(scheme))
            return true;
        if (!isValidProtocol(scheme))
            exceptionState.throwSecurityError("The scheme '" + scheme + "' is not a valid protocol.");
        else
            exceptionState.throwSecurityError("The scheme '" + scheme + "' is less than five characters long.");
        return false;
    }

    if (isProtocolWhitelisted(scheme))
        return true;
    exceptionState.throwSecurityError("The scheme '" + scheme + "' doesn't belong to the protocol whitelist. Please prefix non-whitelisted schemes with the string 'web+'.");
    return false;
}

//S_HTML5_CUSTOM_HANDLER_SUPPORT - START
static bool verifyContentHandlerMimeType(const String& mimeType, ExceptionState& exceptionState)
{
    if (!isContentBlacklisted(mimeType))
        return true;
    
    exceptionState.throwSecurityError("The mimeType '" + mimeType + "' belong to the content blacklist. Please prefix non-blacklisted mimeType");
    return false;
}
//S_HTML5_CUSTOM_HANDLER_SUPPORT - END

NavigatorContentUtils* NavigatorContentUtils::from(Page* page)
{
    return static_cast<NavigatorContentUtils*>(RefCountedSupplement<Page, NavigatorContentUtils>::from(page, NavigatorContentUtils::supplementName()));
}

NavigatorContentUtils::~NavigatorContentUtils()
{
}

PassRefPtr<NavigatorContentUtils> NavigatorContentUtils::create(NavigatorContentUtilsClient* client)
{
    return adoptRef(new NavigatorContentUtils(client));
}

void NavigatorContentUtils::registerProtocolHandler(Navigator* navigator, const String& scheme, const String& url, const String& title, ExceptionState& exceptionState)
{
    if (!navigator->frame())
        return;

    Document* document = navigator->frame()->document();
    if (!document)
        return;

    KURL baseURL = document->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url, exceptionState))
        return;

    if (!verifyProtocolHandlerScheme(scheme, "registerProtocolHandler", exceptionState))
        return;

    //Resolve with base url- S_HTML5_CUSTOM_HANDLER_SUPPORT
	NavigatorContentUtils::from(navigator->frame()->page())->client()->registerContentHandler(scheme, baseURL, KURL(baseURL, url), title);
	
}

//S_HTML5_CUSTOM_HANDLER_SUPPORT - START
void NavigatorContentUtils::registerContentHandler(Navigator* navigator, const String& mimeType, const String& url, const String& title, ExceptionState& exceptionState)
{
    if (!navigator->frame())
        return;

    Document* document = navigator->frame()->document();
    if (!document)
        return;

    KURL baseURL = document->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url, exceptionState))
        return;

    if (!verifyContentHandlerMimeType(mimeType, exceptionState))
        return;

   //resolve using baseURL 
    NavigatorContentUtils::from(navigator->frame()->page())->client()->registerContentHandler(mimeType, baseURL, KURL(baseURL, url), title);
}
//S_HTML5_CUSTOM_HANDLER_SUPPORT - END

static String customHandlersStateString(const NavigatorContentUtilsClient::CustomHandlersState state)
{
    DEFINE_STATIC_LOCAL(const String, newHandler, ("new"));
    DEFINE_STATIC_LOCAL(const String, registeredHandler, ("registered"));
    DEFINE_STATIC_LOCAL(const String, declinedHandler, ("declined"));

    switch (state) {
    case NavigatorContentUtilsClient::CustomHandlersNew:
        return newHandler;
    case NavigatorContentUtilsClient::CustomHandlersRegistered:
        return registeredHandler;
    case NavigatorContentUtilsClient::CustomHandlersDeclined:
        return declinedHandler;
    }

    ASSERT_NOT_REACHED();
    return String();
}

String NavigatorContentUtils::isProtocolHandlerRegistered(Navigator* navigator, const String& scheme, const String& url, ExceptionState& exceptionState)
{
    DEFINE_STATIC_LOCAL(const String, declined, ("declined"));

    if (!navigator->frame())
        return declined;

    Document* document = navigator->frame()->document();
    KURL baseURL = document->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url, exceptionState))
        return declined;

    if (!verifyProtocolHandlerScheme(scheme, "isProtocolHandlerRegistered", exceptionState))
        return declined;

    return customHandlersStateString(NavigatorContentUtils::from(navigator->frame()->page())->client()->isProtocolHandlerRegistered(scheme, baseURL, KURL(ParsedURLString, url)));
}

void NavigatorContentUtils::unregisterProtocolHandler(Navigator* navigator, const String& scheme, const String& url, ExceptionState& exceptionState)
{
    if (!navigator->frame())
        return;

    Document* document = navigator->frame()->document();
    KURL baseURL = document->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url, exceptionState))
        return;

    if (!verifyProtocolHandlerScheme(scheme, "unregisterProtocolHandler", exceptionState))
        return;

    NavigatorContentUtils::from(navigator->frame()->page())->client()->unregisterProtocolHandler(scheme, baseURL, KURL(ParsedURLString, url));
}

//S_HTML5_CUSTOM_HANDLER_SUPPORT - START
String NavigatorContentUtils::isContentHandlerRegistered(Navigator* navigator, const String& mimeType, const String& url, ExceptionState& exceptionState)
{
    DEFINE_STATIC_LOCAL(const String, declined, ("declined"));

    if (!navigator->frame())
        return declined;

    Document* document = navigator->frame()->document();
    KURL baseURL = document->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url, exceptionState))
        return declined;

    if (!verifyContentHandlerMimeType(mimeType, exceptionState))
        return declined;

    return customHandlersStateString(NavigatorContentUtils::from(navigator->frame()->page())->client()->isContentHandlerRegistered(mimeType, baseURL, KURL(ParsedURLString, url)));
}

void NavigatorContentUtils::unregisterContentHandler(Navigator* navigator, const String& mimeType, const String& url, ExceptionState& exceptionState)
{
    if (!navigator->frame())
        return;

    Document* document = navigator->frame()->document();
    KURL baseURL = document->baseURL();

    if (!verifyCustomHandlerURL(baseURL, url, exceptionState))
        return;

    if (!verifyContentHandlerMimeType(mimeType, exceptionState))
        return;

    NavigatorContentUtils::from(navigator->frame()->page())->client()->unregisterContentHandler(mimeType, baseURL, KURL(ParsedURLString, url));
}
//S_HTML5_CUSTOM_HANDLER_SUPPORT - END

const char* NavigatorContentUtils::supplementName()
{
    return "NavigatorContentUtils";
}

void provideNavigatorContentUtilsTo(Page* page, NavigatorContentUtilsClient* client)
{
    RefCountedSupplement<Page, NavigatorContentUtils>::provideTo(page, NavigatorContentUtils::supplementName(), NavigatorContentUtils::create(client));
}

} // namespace WebCore
