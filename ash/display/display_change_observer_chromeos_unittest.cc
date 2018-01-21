// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_change_observer_chromeos.h"

#include "ash/display/display_info.h"
#include "base/memory/scoped_vector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/chromeos/test/test_display_snapshot.h"
#include "ui/display/types/chromeos/display_mode.h"

using ui::DisplayConfigurator;

typedef testing::Test DisplayChangeObserverTest;

namespace ash {

TEST_F(DisplayChangeObserverTest, GetDisplayModeList) {
  ScopedVector<const ui::DisplayMode> modes;
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1200), false, 60));

  // All non-interlaced (as would be seen with different refresh rates).
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1080), false, 80));
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1080), false, 70));
  modes.push_back(new ui::DisplayMode(gfx::Size(1920, 1080), false, 60));

  // Interlaced vs non-interlaced.
  modes.push_back(new ui::DisplayMode(gfx::Size(1280, 720), true, 60));
  modes.push_back(new ui::DisplayMode(gfx::Size(1280, 720), false, 60));

  // Interlaced only.
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 768), true, 70));
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 768), true, 60));

  // Mixed.
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 600), true, 60));
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 600), false, 70));
  modes.push_back(new ui::DisplayMode(gfx::Size(1024, 600), false, 60));

  // Just one interlaced mode.
  modes.push_back(new ui::DisplayMode(gfx::Size(640, 480), true, 60));

  ui::TestDisplaySnapshot display_snapshot;
  display_snapshot.set_modes(modes.get());
  DisplayConfigurator::DisplayState output;
  output.display = &display_snapshot;

  std::vector<DisplayMode> display_modes =
      DisplayChangeObserver::GetDisplayModeList(output);
  ASSERT_EQ(6u, display_modes.size());
  EXPECT_EQ("1920x1200", display_modes[0].size.ToString());
  EXPECT_FALSE(display_modes[0].interlaced);
  EXPECT_EQ(display_modes[0].refresh_rate, 60);

  EXPECT_EQ("1920x1080", display_modes[1].size.ToString());
  EXPECT_FALSE(display_modes[1].interlaced);
  EXPECT_EQ(display_modes[1].refresh_rate, 80);

  EXPECT_EQ("1280x720", display_modes[2].size.ToString());
  EXPECT_FALSE(display_modes[2].interlaced);
  EXPECT_EQ(display_modes[2].refresh_rate, 60);

  EXPECT_EQ("1024x768", display_modes[3].size.ToString());
  EXPECT_TRUE(display_modes[3].interlaced);
  EXPECT_EQ(display_modes[3].refresh_rate, 70);

  EXPECT_EQ("1024x600", display_modes[4].size.ToString());
  EXPECT_FALSE(display_modes[4].interlaced);
  EXPECT_EQ(display_modes[4].refresh_rate, 70);

  EXPECT_EQ("640x480", display_modes[5].size.ToString());
  EXPECT_TRUE(display_modes[5].interlaced);
  EXPECT_EQ(display_modes[5].refresh_rate, 60);

  // Outputs without any modes shouldn't cause a crash.
  modes.clear();
  display_snapshot.set_modes(modes.get());

  display_modes = DisplayChangeObserver::GetDisplayModeList(output);
  EXPECT_EQ(0u, display_modes.size());
}

}  // namespace ash
