// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.input;

import org.chromium.base.JNINamespace;

@JNINamespace("content")
class WebGamepadData {
    public String id;
    public String mapping;
    public float[] axes;
    public float[] buttons;
}
