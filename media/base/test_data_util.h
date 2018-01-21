// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEST_DATA_UTIL_H_
#define MEDIA_BASE_TEST_DATA_UTIL_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

namespace media {

class DecoderBuffer;

typedef std::vector<std::pair<std::string, std::string> > QueryParams;

// Returns a file path for a file in the media/test/data directory.
base::FilePath GetTestDataFilePath(const std::string& name);

// Returns relative path for test data folder: media/test/data.
base::FilePath GetTestDataPath();

// Starts an HTTP server serving files from media data path.
scoped_ptr<net::SpawnedTestServer> StartMediaHttpTestServer();

// Returns a string containing key value query params in the form of:
// "key_1=value_1&key_2=value2"
std::string GetURLQueryString(const QueryParams& query_params);

// Reads a test file from media/test/data directory and stores it in
// a DecoderBuffer.  Use DecoderBuffer vs DataBuffer to ensure no matter
// what a test does, it's safe to use FFmpeg methods.
//
//  |name| - The name of the file.
//  |buffer| - The contents of the file.
scoped_refptr<DecoderBuffer> ReadTestDataFile(const std::string& name);

}  // namespace media

#endif  // MEDIA_BASE_TEST_DATA_UTIL_H_
