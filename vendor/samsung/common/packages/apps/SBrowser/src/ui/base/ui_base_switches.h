// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by ui/base.

#ifndef UI_BASE_UI_BASE_SWITCHES_H_
#define UI_BASE_UI_BASE_SWITCHES_H_

#include "base/compiler_specific.h"
#include "ui/base/ui_base_export.h"

namespace switches {

UI_BASE_EXPORT extern const char kDisableDwmComposition[];
UI_BASE_EXPORT extern const char kDisableTouchAdjustment[];
UI_BASE_EXPORT extern const char kDisableTouchDragDrop[];
UI_BASE_EXPORT extern const char kDisableTouchEditing[];
UI_BASE_EXPORT extern const char kEnableTouchDragDrop[];
UI_BASE_EXPORT extern const char kEnableTouchEditing[];
UI_BASE_EXPORT extern const char kHighlightMissingScaledResources[];
UI_BASE_EXPORT extern const char kLang[];
UI_BASE_EXPORT extern const char kLocalePak[];
UI_BASE_EXPORT extern const char kNoMessageBox[];
UI_BASE_EXPORT extern const char kTouchOptimizedUI[];
UI_BASE_EXPORT extern const char kTouchOptimizedUIAuto[];
UI_BASE_EXPORT extern const char kTouchOptimizedUIDisabled[];
UI_BASE_EXPORT extern const char kTouchOptimizedUIEnabled[];
UI_BASE_EXPORT extern const char kTouchSideBezels[];

#if defined(OS_ANDROID)
UI_BASE_EXPORT extern const char kTabletUI[];
#endif

#if defined(USE_XI2_MT)
UI_BASE_EXPORT extern const char kTouchCalibration[];
#endif

}  // namespace switches

#endif  // UI_BASE_UI_BASE_SWITCHES_H_
