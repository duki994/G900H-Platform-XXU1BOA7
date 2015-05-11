// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISPLAY_TOUCHSCREEN_DELEGATE_X11_H_
#define CHROMEOS_DISPLAY_TOUCHSCREEN_DELEGATE_X11_H_

#include "chromeos/display/output_configurator.h"

struct _XDisplay;
typedef struct _XDisplay Display;

namespace chromeos {

class TouchscreenDelegateX11 : public OutputConfigurator::TouchscreenDelegate {
 public:
  TouchscreenDelegateX11();
  virtual ~TouchscreenDelegateX11();

  // OutputConfigurator::TouchscreenDelegate implementation:
  virtual void AssociateTouchscreens(
      std::vector<OutputConfigurator::OutputSnapshot>* outputs) OVERRIDE;
  virtual void ConfigureCTM(
      int touch_device_id,
      const OutputConfigurator::CoordinateTransformation& ctm) OVERRIDE;

 private:
  Display* display_;

  DISALLOW_COPY_AND_ASSIGN(TouchscreenDelegateX11);
};

}  // namespace chromeos

#endif  // CHROMEOS_DISPLAY_TOUCHSCREEN_DELEGATE_X11_H_
