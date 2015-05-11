// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_converter_evdev.h"

#include "ui/events/event.h"
#include "ui/events/ozone/event_factory_ozone.h"

namespace ui {

EventConverterEvdev::EventConverterEvdev() {}

EventConverterEvdev::~EventConverterEvdev() {}

void EventConverterEvdev::DispatchEvent(scoped_ptr<ui::Event> event) {
  EventFactoryOzone::DispatchEvent(event.Pass());
}

}  // namespace ui
