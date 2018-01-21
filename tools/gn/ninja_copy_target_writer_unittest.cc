// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/file_template.h"
#include "tools/gn/ninja_copy_target_writer.h"
#include "tools/gn/test_with_scope.h"

// Tests mutliple files with an output pattern and no toolchain dependency.
TEST(NinjaCopyTargetWriter, Run) {
  TestWithScope setup;
  setup.settings()->set_target_os(Settings::LINUX);
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);

  target.sources().push_back(SourceFile("//foo/input1.txt"));
  target.sources().push_back(SourceFile("//foo/input2.txt"));

  target.action_values().outputs().push_back(
      "//out/Debug/{{source_name_part}}.out");

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, setup.toolchain(), out);
  writer.Run();

  const char expected_linux[] =
      "build input1.out: copy ../../foo/input1.txt\n"
      "build input2.out: copy ../../foo/input2.txt\n"
      "\n"
      "build obj/foo/bar.stamp: stamp input1.out input2.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}

// Tests a single file with no output pattern and a toolchain dependency.
TEST(NinjaCopyTargetWriter, ToolchainDeps) {
  TestWithScope setup;
  setup.settings()->set_target_os(Settings::LINUX);
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  target.set_output_type(Target::COPY_FILES);

  // Toolchain dependency. Here we make a target in the same toolchain for
  // simplicity, but in real life (using the Builder) this would be rejected
  // because it would be a circular dependency (the target depends on its
  // toolchain, and the toolchain depends on this target).
  Target toolchain_dep_target(setup.settings(),
                              Label(SourceDir("//foo/"), "setup"));
  toolchain_dep_target.set_output_type(Target::ACTION);
  setup.toolchain()->deps().push_back(LabelTargetPair(&toolchain_dep_target));

  target.sources().push_back(SourceFile("//foo/input1.txt"));

  target.action_values().outputs().push_back("//out/Debug/output.out");

  std::ostringstream out;
  NinjaCopyTargetWriter writer(&target, setup.toolchain(), out);
  writer.Run();

  const char expected_linux[] =
      "build obj/foo/bar.inputdeps.stamp: stamp obj/foo/setup.stamp\n"
      "build output.out: copy ../../foo/input1.txt | "
          "obj/foo/bar.inputdeps.stamp\n"
      "\n"
      "build obj/foo/bar.stamp: stamp output.out\n";
  std::string out_str = out.str();
  EXPECT_EQ(expected_linux, out_str);
}
