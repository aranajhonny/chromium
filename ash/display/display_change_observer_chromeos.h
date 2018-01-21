// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CHANGE_OBSERVER_CHROMEOS_H
#define ASH_DISPLAY_DISPLAY_CHANGE_OBSERVER_CHROMEOS_H

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/basictypes.h"
#include "ui/display/chromeos/display_configurator.h"

namespace ash {

struct DisplayMode;

// An object that observes changes in display configuration and
// update DisplayManagers.
class DisplayChangeObserver : public ui::DisplayConfigurator::StateController,
                              public ui::DisplayConfigurator::Observer,
                              public ShellObserver {
 public:
  // Returns the resolution list.
  ASH_EXPORT static std::vector<DisplayMode> GetDisplayModeList(
      const ui::DisplayConfigurator::DisplayState& output);

  DisplayChangeObserver();
  virtual ~DisplayChangeObserver();

  // ui::DisplayConfigurator::StateController overrides:
  virtual ui::MultipleDisplayState GetStateForDisplayIds(
      const std::vector<int64>& outputs) const OVERRIDE;
  virtual bool GetResolutionForDisplayId(int64 display_id,
                                         gfx::Size* size) const OVERRIDE;

  // Overriden from ui::DisplayConfigurator::Observer:
  virtual void OnDisplayModeChanged(
      const ui::DisplayConfigurator::DisplayStateList& outputs) OVERRIDE;

  // Overriden from ShellObserver:
  virtual void OnAppTerminating() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayChangeObserver);
};

}  // namespace ash

#endif  // ASH_DISPLAY_AURA_DISPLAY_CHANGE_OBSERVER_CHROMEOS_H
