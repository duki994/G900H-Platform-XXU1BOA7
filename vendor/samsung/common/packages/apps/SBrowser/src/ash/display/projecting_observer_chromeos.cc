// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/projecting_observer_chromeos.h"

#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"

namespace ash {

namespace internal {

ProjectingObserver::ProjectingObserver()
    : has_internal_output_(false),
      output_count_(0),
      casting_session_count_(0) {}

ProjectingObserver::~ProjectingObserver() {}

void ProjectingObserver::OnDisplayModeChanged(
    const std::vector<chromeos::OutputConfigurator::OutputSnapshot>& outputs) {
  has_internal_output_ = false;
  output_count_ = outputs.size();

  for (size_t i = 0; i < outputs.size(); ++i) {
    if (outputs[i].type == ui::OUTPUT_TYPE_INTERNAL) {
      has_internal_output_ = true;
      break;
    }
  }

  SetIsProjecting();
}

void ProjectingObserver::OnCastingSessionStartedOrStopped(bool started) {
  if (started) {
    ++casting_session_count_;
  } else {
    DCHECK_GT(casting_session_count_, 0);
    --casting_session_count_;
    if (casting_session_count_ < 0)
      casting_session_count_ = 0;
  }

  SetIsProjecting();
}

void ProjectingObserver::SetIsProjecting() {
  // "Projecting" is defined as having more than 1 output connected while at
  // least one of them is an internal output.
  bool projecting = has_internal_output_ &&
      (output_count_ + casting_session_count_ > 1);

  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->SetIsProjecting(
      projecting);
}

}  // namespace internal

}  // namespace ash
