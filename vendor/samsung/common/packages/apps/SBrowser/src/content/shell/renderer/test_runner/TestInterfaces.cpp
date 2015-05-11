// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/TestInterfaces.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "content/shell/renderer/test_runner/AccessibilityController.h"
#include "content/shell/renderer/test_runner/EventSender.h"
#include "content/shell/renderer/test_runner/TestRunner.h"
#include "content/shell/renderer/test_runner/WebTestProxy.h"
#include "content/shell/renderer/test_runner/gamepad_controller.h"
#include "content/shell/renderer/test_runner/text_input_controller.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebCache.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebView.h"

using namespace blink;
using namespace std;

namespace WebTestRunner {

TestInterfaces::TestInterfaces()
    : m_accessibilityController(new AccessibilityController())
    , m_eventSender(new EventSender(this))
    , m_gamepadController(new content::GamepadController())
    , m_textInputController(new content::TextInputController())
    , m_testRunner(new TestRunner(this))
    , m_delegate(0)
{
    blink::setLayoutTestMode(true);

    // NOTE: please don't put feature specific enable flags here,
    // instead add them to RuntimeEnabledFeatures.in

    resetAll();
}

TestInterfaces::~TestInterfaces()
{
    m_accessibilityController->setWebView(0);
    m_eventSender->setWebView(0);
    // m_gamepadController doesn't depend on WebView.
    m_textInputController->SetWebView(NULL);
    m_testRunner->setWebView(0, 0);

    m_accessibilityController->setDelegate(0);
    m_eventSender->setDelegate(0);
    m_gamepadController->SetDelegate(0);
    // m_textInputController doesn't depend on WebTestDelegate.
    m_testRunner->setDelegate(0);
}

void TestInterfaces::setWebView(WebView* webView, WebTestProxyBase* proxy)
{
    m_proxy = proxy;
    m_accessibilityController->setWebView(webView);
    m_eventSender->setWebView(webView);
    // m_gamepadController doesn't depend on WebView.
    m_textInputController->SetWebView(webView);
    m_testRunner->setWebView(webView, proxy);
}

void TestInterfaces::setDelegate(WebTestDelegate* delegate)
{
    m_accessibilityController->setDelegate(delegate);
    m_eventSender->setDelegate(delegate);
    m_gamepadController->SetDelegate(delegate);
    // m_textInputController doesn't depend on WebTestDelegate.
    m_testRunner->setDelegate(delegate);
    m_delegate = delegate;
}

void TestInterfaces::bindTo(WebFrame* frame)
{
    m_accessibilityController->bindToJavascript(frame, WebString::fromUTF8("accessibilityController"));
    m_eventSender->bindToJavascript(frame, WebString::fromUTF8("eventSender"));
    m_gamepadController->Install(frame);
    m_textInputController->Install(frame);
    m_testRunner->bindToJavascript(frame, WebString::fromUTF8("testRunner"));
    m_testRunner->bindToJavascript(frame, WebString::fromUTF8("layoutTestController"));
}

void TestInterfaces::resetTestHelperControllers()
{
    m_accessibilityController->reset();
    m_eventSender->reset();
    m_gamepadController->Reset();
    // m_textInputController doesn't have any state to reset.
    WebCache::clear();
}

void TestInterfaces::resetAll()
{
    resetTestHelperControllers();
    m_testRunner->reset();
}

void TestInterfaces::setTestIsRunning(bool running)
{
    m_testRunner->setTestIsRunning(running);
}

void TestInterfaces::configureForTestWithURL(const WebURL& testURL, bool generatePixels)
{
    string spec = GURL(testURL).spec();
    m_testRunner->setShouldGeneratePixelResults(generatePixels);
    if (spec.find("loading/") != string::npos)
        m_testRunner->setShouldDumpFrameLoadCallbacks(true);
    if (spec.find("/dumpAsText/") != string::npos) {
        m_testRunner->setShouldDumpAsText(true);
        m_testRunner->setShouldGeneratePixelResults(false);
    }
    if (spec.find("/inspector/") != string::npos
        || spec.find("/inspector-enabled/") != string::npos)
        m_testRunner->clearDevToolsLocalStorage();
    if (spec.find("/inspector/") != string::npos) {
        // Subfolder name determines default panel to open.
        string settings = "";
        string test_path = spec.substr(spec.find("/inspector/") + 11);
        size_t slash_index = test_path.find("/");
        if (slash_index != string::npos) {
            settings = base::StringPrintf(
                "{\"lastActivePanel\":\"\\\"%s\\\"\"}",
                test_path.substr(0, slash_index).c_str());
        }
        m_testRunner->showDevTools(settings);
    }
    if (spec.find("/viewsource/") != string::npos) {
        m_testRunner->setShouldEnableViewSource(true);
        m_testRunner->setShouldGeneratePixelResults(false);
        m_testRunner->setShouldDumpAsMarkup(true);
    }
}

void TestInterfaces::windowOpened(WebTestProxyBase* proxy)
{
    m_windowList.push_back(proxy);
}

void TestInterfaces::windowClosed(WebTestProxyBase* proxy)
{
    vector<WebTestProxyBase*>::iterator pos = find(m_windowList.begin(), m_windowList.end(), proxy);
    if (pos == m_windowList.end()) {
        BLINK_ASSERT_NOT_REACHED();
        return;
    }
    m_windowList.erase(pos);
}

AccessibilityController* TestInterfaces::accessibilityController()
{
    return m_accessibilityController.get();
}

EventSender* TestInterfaces::eventSender()
{
    return m_eventSender.get();
}

TestRunner* TestInterfaces::testRunner()
{
    return m_testRunner.get();
}

WebTestDelegate* TestInterfaces::delegate()
{
    return m_delegate;
}

WebTestProxyBase* TestInterfaces::proxy()
{
    return m_proxy;
}

const vector<WebTestProxyBase*>& TestInterfaces::windowList()
{
    return m_windowList;
}

WebThemeEngine* TestInterfaces::themeEngine()
{
    if (!m_testRunner->useMockTheme())
        return 0;
#if defined(__APPLE__)
    if (!m_themeEngine.get())
        m_themeEngine.reset(new WebTestThemeEngineMac());
#else
    if (!m_themeEngine.get())
        m_themeEngine.reset(new WebTestThemeEngineMock());
#endif
    return m_themeEngine.get();
}

}
