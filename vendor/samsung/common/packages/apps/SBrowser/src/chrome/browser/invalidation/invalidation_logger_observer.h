// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_INVALIDATION_LOGGER_OBSERVER_H_
#define CHROME_BROWSER_INVALIDATION_INVALIDATION_LOGGER_OBSERVER_H_

#include "base/memory/scoped_ptr.h"
#include "sync/notifier/invalidator_state.h"
#include "sync/notifier/object_id_invalidation_map.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace syncer {
class ObjectIdInvalidationMap;
}  // namespace syncer

namespace invalidation {
// This class provides the possibilty to register as an observer for the
// InvalidationLogger notifications whenever an InvalidatorService changes
// its internal state.
// (i.e. A new registration, a new invalidation, a TICL/GCM state change)

class InvalidationLoggerObserver {
 public:
  virtual void OnRegistration(const base::DictionaryValue& details) = 0;
  virtual void OnUnregistration(const base::DictionaryValue& details) = 0;
  virtual void OnStateChange(const syncer::InvalidatorState& newState) = 0;
  virtual void OnUpdateIds(const base::DictionaryValue& details) = 0;
  virtual void OnDebugMessage(const base::DictionaryValue& details) = 0;
  virtual void OnInvalidation(
      const syncer::ObjectIdInvalidationMap& details) = 0;

 protected:
  virtual ~InvalidationLoggerObserver() {}
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_INVALIDATION_LOGGER_OBSERVER_H_
