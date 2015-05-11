// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_TEST_RUNNER_TESTINTERFACES_H_
#define CONTENT_SHELL_RENDERER_TEST_RUNNER_TESTINTERFACES_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/platform/WebNonCopyable.h"

#if defined(__APPLE__)
#include "content/shell/renderer/test_runner/WebTestThemeEngineMac.h"
#else
#include "content/shell/renderer/test_runner/WebTestThemeEngineMock.h"
#endif

namespace blink {
class WebFrame;
class WebThemeEngine;
class WebURL;
class WebView;
}

namespace content {
class GamepadController;
class TextInputController;
}

namespace WebTestRunner {

class AccessibilityController;
class EventSender;
class TestRunner;
class WebTestDelegate;
class WebTestProxyBase;

class TestInterfaces {
public:
    TestInterfaces();
    ~TestInterfaces();

    void setWebView(blink::WebView*, WebTestProxyBase*);
    void setDelegate(WebTestDelegate*);
    void bindTo(blink::WebFrame*);
    void resetTestHelperControllers();
    void resetAll();
    void setTestIsRunning(bool);
    void configureForTestWithURL(const blink::WebURL&, bool generatePixels);

    void windowOpened(WebTestProxyBase*);
    void windowClosed(WebTestProxyBase*);

    AccessibilityController* accessibilityController();
    EventSender* eventSender();
    TestRunner* testRunner();
    WebTestDelegate* delegate();
    WebTestProxyBase* proxy();
    const std::vector<WebTestProxyBase*>& windowList();
    blink::WebThemeEngine* themeEngine();

private:
    scoped_ptr<AccessibilityController> m_accessibilityController;
    scoped_ptr<EventSender> m_eventSender;
    scoped_ptr<content::GamepadController> m_gamepadController;
    scoped_ptr<content::TextInputController> m_textInputController;
    scoped_ptr<TestRunner> m_testRunner;
    WebTestDelegate* m_delegate;
    WebTestProxyBase* m_proxy;

    std::vector<WebTestProxyBase*> m_windowList;
#if defined(__APPLE__)
    scoped_ptr<WebTestThemeEngineMac> m_themeEngine;
#else
    scoped_ptr<WebTestThemeEngineMock> m_themeEngine;
#endif

    DISALLOW_COPY_AND_ASSIGN(TestInterfaces);
};

}

#endif  // CONTENT_SHELL_RENDERER_TEST_RUNNER_TESTINTERFACES_H_
