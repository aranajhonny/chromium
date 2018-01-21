// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/hid/hid_connection_resource.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "device/hid/hid_connection.h"

namespace extensions {

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ApiResourceManager<HidConnectionResource> > >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<HidConnectionResource> >*
ApiResourceManager<HidConnectionResource>::GetFactoryInstance() {
  return &g_factory.Get();
}

HidConnectionResource::HidConnectionResource(
    const std::string& owner_extension_id,
    scoped_refptr<device::HidConnection> connection)
    : ApiResource(owner_extension_id), connection_(connection) {}

HidConnectionResource::~HidConnectionResource() {}

}  // namespace extensions
