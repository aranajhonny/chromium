// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_CONFIGURATOR_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_CONFIGURATOR_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_configurator.h"

class PrefService;

class DataReductionProxyChromeConfigurator
    : public data_reduction_proxy::DataReductionProxyConfigurator {
 public:
  explicit DataReductionProxyChromeConfigurator(PrefService* prefs);
  virtual ~DataReductionProxyChromeConfigurator();

  virtual void Enable(bool primary_restricted,
                      bool fallback_restricted,
                      const std::string& primary_origin,
                      const std::string& fallback_origin,
                      const std::string& ssl_origin) OVERRIDE;
  virtual void Disable() OVERRIDE;

  // Add a host pattern to bypass. This should follow the same syntax used
  // in net::ProxyBypassRules; that is, a hostname pattern, a hostname suffix
  // pattern, an IP literal, a CIDR block, or the magic string '<local>'.
  // Bypass settings persist for the life of this object and are applied
  // each time the proxy is enabled, but are not updated while it is enabled.
  virtual void AddHostPatternToBypass(const std::string& pattern) OVERRIDE;

  // Add a URL pattern to bypass the proxy. The base implementation strips
  // everything in |pattern| after the first single slash and then treats it
  // as a hostname pattern. Subclasses may implement other semantics.
  virtual void AddURLPatternToBypass(const std::string& pattern) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest, TestBypassList);

  PrefService* prefs_;
  std::vector<std::string> bypass_rules_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyChromeConfigurator);
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_CHROME_CONFIGURATOR_H_
