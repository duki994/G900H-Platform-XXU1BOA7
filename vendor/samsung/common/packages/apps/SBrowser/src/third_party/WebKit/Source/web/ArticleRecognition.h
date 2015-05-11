/// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>

#ifndef ArticleRecognition_h
#define ArticleRecognition_h

namespace WebCore { class Frame; }

namespace blink {

class ArticleRecognition {
public:
    static std::string recognizeArticleSimpleNativeRecognitionMode(WebCore::Frame*);
    static std::string recognizeArticleNativeRecognitionMode(WebCore::Frame*);
};

} // namespace blink

#endif // ArticleRecognition_h
