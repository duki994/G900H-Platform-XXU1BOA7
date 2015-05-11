// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_shell_window_delegate.h"

#import "chrome/browser/ui/cocoa/apps/native_app_window_cocoa.h"

// static
apps::NativeAppWindow* ChromeShellWindowDelegate::CreateNativeAppWindowImpl(
    apps::AppWindow* app_window,
    const apps::AppWindow::CreateParams& params) {
  return new NativeAppWindowCocoa(app_window, params);
}
