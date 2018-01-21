// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file intentionally does not have header guards, it's included
// inside a macro to generate enum values.
#ifndef DEFINE_CONTEXT_CONFIG
#error "DEFINE_CONTEXT_CONFIG should be defined before including this file"
#endif
// See HttpUrlRequestFactoryConfig.java for description of these parameters.
DEFINE_CONTEXT_CONFIG(STORAGE_PATH)
DEFINE_CONTEXT_CONFIG(ENABLE_LEGACY_MODE)
DEFINE_CONTEXT_CONFIG(ENABLE_QUIC)
DEFINE_CONTEXT_CONFIG(ENABLE_SPDY)
DEFINE_CONTEXT_CONFIG(HTTP_CACHE)
DEFINE_CONTEXT_CONFIG(HTTP_CACHE_MAX_SIZE)

DEFINE_CONTEXT_CONFIG(HTTP_CACHE_DISABLED)
DEFINE_CONTEXT_CONFIG(HTTP_CACHE_DISK)
DEFINE_CONTEXT_CONFIG(HTTP_CACHE_MEMORY)
