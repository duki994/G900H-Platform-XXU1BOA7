// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/editing/InputMethodController.h"

#include "HTMLNames.h"
#include "core/frame/LocalFrame.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLInputElement.h"
#include "core/testing/DummyPageHolder.h"
#include "public/web/WebDocument.h"
#include "web/WebLocalFrameImpl.h"
#include "web/tests/FrameTestHelpers.h"
#include "web/tests/URLTestHelpers.h"
#include <gtest/gtest.h>

using namespace blink;
using namespace WebCore;

namespace {

class InputMethodControllerTest : public ::testing::Test {
public:
    InputMethodControllerTest() : m_baseURL("http://www.test.com/") { }

protected:
    std::string m_baseURL;
    FrameTestHelpers::WebViewHelper m_webViewHelper;
};

TEST_F(InputMethodControllerTest, BackspaceFromEndOfInput)
{
    char const* const filename = "input_field_populated.html";
    URLTestHelpers::registerMockedURLFromBaseURL(WebString::fromUTF8(m_baseURL.c_str()), WebString::fromUTF8(filename));
    WebView* view = m_webViewHelper.initializeAndLoad(m_baseURL + filename);
    WebLocalFrameImpl* frame = toWebLocalFrameImpl(view->mainFrame());
    HTMLDocument* document = toHTMLDocument(frame->frame()->document());
    InputMethodController& controller = frame->frame()->inputMethodController();
    HTMLInputElement* input = toHTMLInputElement(document->getElementById("sample"));
    ASSERT_TRUE(input);
    view->setInitialFocus(false);
    ASSERT_EQ(input, document->focusedElement());

    input->setValue("fooX");
    frame->setEditableSelectionOffsets(4, 4);
    EXPECT_STREQ("fooX", input->value().utf8().data());
    controller.extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());

    input->setValue(String::fromUTF8("foo\xE2\x98\x85")); // U+2605 == "black star"
    frame->setEditableSelectionOffsets(4, 4);
    EXPECT_STREQ("foo\xE2\x98\x85", input->value().utf8().data());
    controller.extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());

    input->setValue(String::fromUTF8("foo\xF0\x9F\x8F\x86")); // U+1F3C6 == "trophy"
    frame->setEditableSelectionOffsets(4, 4);
    EXPECT_STREQ("foo\xF0\x9F\x8F\x86", input->value().utf8().data());
    controller.extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());

    input->setValue(String::fromUTF8("foo\xE0\xB8\x81\xE0\xB9\x89")); // composed U+0E01 "ka kai" + U+0E49 "mai tho"
    frame->setEditableSelectionOffsets(4, 4);
    EXPECT_STREQ("foo\xE0\xB8\x81\xE0\xB9\x89", input->value().utf8().data());
    controller.extendSelectionAndDelete(1, 0);
    EXPECT_STREQ("foo", input->value().utf8().data());
}

} // namespace
