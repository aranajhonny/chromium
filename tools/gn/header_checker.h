// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_HEADER_CHECKER_H_
#define TOOLS_GN_HEADER_CHECKER_H_

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/lock.h"
#include "tools/gn/err.h"

class BuildSettings;
class InputFile;
class Label;
class LocationRange;
class SourceFile;
class Target;

namespace base {
class MessageLoop;
}

class HeaderChecker : public base::RefCountedThreadSafe<HeaderChecker> {
 public:
  HeaderChecker(const BuildSettings* build_settings,
                const std::vector<const Target*>& targets);

  // This assumes that the current thread already has a message loop.  On
  // error, fills the given vector with the errors and returns false.  Returns
  // true on success.
  bool Run(std::vector<Err>* errors);

 private:
  friend class base::RefCountedThreadSafe<HeaderChecker>;
  FRIEND_TEST_ALL_PREFIXES(HeaderCheckerTest, IsDependencyOf);
  FRIEND_TEST_ALL_PREFIXES(HeaderCheckerTest, CheckInclude);
  FRIEND_TEST_ALL_PREFIXES(HeaderCheckerTest, DoDirectDependentConfigsApply);
  ~HeaderChecker();

  struct TargetInfo {
    TargetInfo() : target(NULL), is_public(false) {}
    TargetInfo(const Target* t, bool p) : target(t), is_public(p) {}

    const Target* target;
    bool is_public;
  };

  typedef std::vector<TargetInfo> TargetVector;

  void DoWork(const Target* target, const SourceFile& file);

  // Adds the sources and public files from the given target to the file_map_.
  // Not threadsafe! Called only during init.
  void AddTargetToFileMap(const Target* target);

  // Returns true if the given file is in the output directory.
  bool IsFileInOuputDir(const SourceFile& file) const;

  // Resolves the contents of an include to a SourceFile.
  SourceFile SourceFileForInclude(const base::StringPiece& input) const;

  // from_target is the target the file was defined from. It will be used in
  // error messages.
  bool CheckFile(const Target* from_target,
                 const SourceFile& file,
                 Err* err) const;

  // Checks that the given file in the given target can include the given
  // include file. If disallowed, returns false and sets the error. The
  // range indicates the location of the include in the file for error
  // reporting.
  bool CheckInclude(const Target* from_target,
                    const InputFile& source_file,
                    const SourceFile& include_file,
                    const LocationRange& range,
                    Err* err) const;

  // Returns true if the given search_for target is a dependency of
  // search_from. Many subtrees are duplicated so this function avoids
  // duplicate checking across recursive calls by keeping track of checked
  // targets in the given set. It should point to an empty set for the first
  // call. A target is not considered to be a dependency of itself.
  //
  // If found, the vector given in "chain" will be filled with the reverse
  // dependency chain from the dest target (chain[0] = search_for) to the src
  // target (chain[chain.size() - 1] = search_from).
  bool IsDependencyOf(const Target* search_for,
                      const Target* search_from,
                      std::vector<const Target*>* chain) const;
  bool IsDependencyOf(const Target* search_for,
                      const Target* search_from,
                      std::vector<const Target*>* chain,
                      std::set<const Target*>* checked) const;

  // Given a reverse dependency chain (chain[0] is the lower-level target,
  // chain[end] is the higher-level target), determines if all direct dependent
  // configs on the lower-level target would apply to the higher-level one.
  //
  // If configs do not apply, this function returns false and indicates the
  // index of the target that caused the config to not apply by putting it in
  // problematic_index.
  static bool DoDirectDependentConfigsApply(
      const std::vector<const Target*>& chain,
      size_t* problematic_index);

  // Non-locked variables ------------------------------------------------------
  //
  // These are initialized during construction (which happens on one thread)
  // and are not modified after, so any thread can read these without locking.

  base::MessageLoop* main_loop_;
  base::RunLoop main_thread_runner_;

  const BuildSettings* build_settings_;

  // Maps source files to targets it appears in (usually just one target).
  typedef std::map<SourceFile, TargetVector> FileMap;
  FileMap file_map_;

  // Locked variables ----------------------------------------------------------
  //
  // These are mutable during runtime and require locking.

  base::Lock lock_;

  std::vector<Err> errors_;

  DISALLOW_COPY_AND_ASSIGN(HeaderChecker);
};

#endif  // TOOLS_GN_HEADER_CHECKER_H_
