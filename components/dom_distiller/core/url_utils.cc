// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/url_utils.h"

#include <string>

#include "base/guid.h"
#include "components/dom_distiller/core/url_constants.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace dom_distiller {

namespace url_utils {

namespace {

const char kDummyInternalUrlPrefix[] = "chrome-distiller-internal://dummy/";

}  // namespace

const GURL GetDistillerViewUrlFromEntryId(const std::string& scheme,
                                          const std::string& entry_id) {
  GURL url(scheme + "://" + base::GenerateGUID());
  return net::AppendOrReplaceQueryParameter(url, kEntryIdKey, entry_id);
}

const GURL GetDistillerViewUrlFromUrl(const std::string& scheme,
                                      const GURL& view_url) {
  GURL url(scheme + "://" + base::GenerateGUID());
  return net::AppendOrReplaceQueryParameter(url, kUrlKey, view_url.spec());
}

std::string GetValueForKeyInUrlPathQuery(const std::string& path,
                                         const std::string& key) {
  // Tools for retrieving a value in a query only works with full GURLs, so
  // using a dummy scheme and host to create a fake URL which can be parsed.
  GURL dummy_url(kDummyInternalUrlPrefix + path);
  std::string value;
  net::GetValueForKeyInQuery(dummy_url, key, &value);
  return value;
}

bool IsUrlDistillable(const GURL& url) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS();
}

bool IsUrlReportable(const std::string& scheme, const GURL& url) {
  return url.is_valid() && url.scheme() == scheme;
}

}  // namespace url_utils

}  // namespace dom_distiller
