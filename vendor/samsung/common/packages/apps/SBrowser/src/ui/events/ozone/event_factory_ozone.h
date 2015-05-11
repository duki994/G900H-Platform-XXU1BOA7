// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVENT_FACTORY_OZONE_H_
#define UI_EVENTS_OZONE_EVENT_FACTORY_OZONE_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_pump_libevent.h"
#include "base/task_runner.h"
#include "ui/events/events_export.h"

namespace ui {

class Event;

// Creates and dispatches |ui.Event|'s. Ozone assumes that events arrive on file
// descriptors with one  |EventConverterOzone| instance for each descriptor.
// Ozone presumes that the set of file desctiprtors can vary at runtime so this
// class supports dynamically adding and removing |EventConverterOzone|
// instances as necessary.
class EVENTS_EXPORT EventFactoryOzone {
 public:
  EventFactoryOzone();
  virtual ~EventFactoryOzone();

  // Called from WindowTreeHostOzone to initialize and start processing events.
  // This should create the initial set of converters, and potentially arrange
  // for more converters to be created as new event sources become available.
  // No events processing should happen until this is called. All processes have
  // an EventFactoryOzone but not all of them should process events. In chrome,
  // events are dispatched in the browser process on the UI thread.
  virtual void StartProcessingEvents();

  // Sets the TaskRunner to use for file I/O. The thread that calls
  // StartProcessingEvents() should only be used for I/O that is critical
  // to event dispatching.
  virtual void SetFileTaskRunner(scoped_refptr<base::TaskRunner> task_runner);

  // Returns the static instance last set using SetInstance().
  static EventFactoryOzone* GetInstance();

  // Sets the implementation delegate. Ownership is retained by the caller.
  static void SetInstance(EventFactoryOzone*);

  // Subclasses should use this method to post a task that will dispatch
  // |event| from the UI message loop. This method takes ownership of
  // |event|. |event| will be deleted at the end of the posted task.
  static void DispatchEvent(scoped_ptr<ui::Event> event);

 private:
  static EventFactoryOzone* impl_;  // not owned

  DISALLOW_COPY_AND_ASSIGN(EventFactoryOzone);
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVENT_FACTORY_OZONE_H_
