// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_PROJECTING_OBSERVER_CHROMEOS_H_
#define ASH_DISPLAY_PROJECTING_OBSERVER_CHROMEOS_H_

#include "ash/ash_export.h"
#include "chromeos/display/output_configurator.h"

namespace ash {

namespace internal {

class ASH_EXPORT ProjectingObserver
    : public chromeos::OutputConfigurator::Observer {
 public:
  ProjectingObserver();
  virtual ~ProjectingObserver();

  // Called when a casting session is started or stopped.
  void OnCastingSessionStartedOrStopped(bool started);

  // OutputConfigurator::Observer implementation:
  virtual void OnDisplayModeChanged(const std::vector<
      chromeos::OutputConfigurator::OutputSnapshot>& outputs) OVERRIDE;

 private:
  // Sends the current projecting state to power manager.
  void SetIsProjecting();

  // True if at least one output is internal. This value is updated when
  // |OnDisplayModeChanged| is called.
  bool has_internal_output_;

  // Keeps track of the number of connected outputs.
  int output_count_;

  // Number of outstanding casting sessions.
  int casting_session_count_;

  DISALLOW_COPY_AND_ASSIGN(ProjectingObserver);
};

}  // namespace internal

}  // namespace ash

#endif  // ASH_DISPLAY_PROJECTING_OBSERVER_CHROMEOS_H_
