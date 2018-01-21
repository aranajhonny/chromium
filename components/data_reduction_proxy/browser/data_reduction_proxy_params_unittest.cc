// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {
class DataReductionProxyParamsTest : public testing::Test {
 public:
  void CheckParams(const TestDataReductionProxyParams& params,
                   bool expected_init_result,
                   bool expected_allowed,
                   bool expected_fallback_allowed,
                   bool expected_alternative_allowed,
                   bool expected_promo_allowed) {
    EXPECT_EQ(expected_init_result, params.init_result());
    EXPECT_EQ(expected_allowed, params.allowed());
    EXPECT_EQ(expected_fallback_allowed, params.fallback_allowed());
    EXPECT_EQ(expected_alternative_allowed, params.alternative_allowed());
    EXPECT_EQ(expected_promo_allowed, params.promo_allowed());
  }
  void CheckValues(const TestDataReductionProxyParams& params,
                   const std::string& expected_origin,
                   const std::string& expected_fallback_origin,
                   const std::string& expected_ssl_origin,
                   const std::string& expected_alt_origin,
                   const std::string& expected_alt_fallback_origin,
                   const std::string& expected_probe_url) {
    EXPECT_EQ(GURL(expected_origin), params.origin());
    EXPECT_EQ(GURL(expected_fallback_origin), params.fallback_origin());
    EXPECT_EQ(GURL(expected_ssl_origin), params.ssl_origin());
    EXPECT_EQ(GURL(expected_alt_origin), params.alt_origin());
    EXPECT_EQ(GURL(expected_alt_fallback_origin), params.alt_fallback_origin());
    EXPECT_EQ(GURL(expected_probe_url), params.probe_url());
  }
};

TEST_F(DataReductionProxyParamsTest, EverythingDefined) {
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kPromoAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING);
  CheckParams(params, true, true, true, false, true);
  CheckValues(params,
              TestDataReductionProxyParams::DefaultDevOrigin(),
              TestDataReductionProxyParams::DefaultFallbackOrigin(),
              TestDataReductionProxyParams::DefaultSSLOrigin(),
              TestDataReductionProxyParams::DefaultAltOrigin(),
              TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
              TestDataReductionProxyParams::DefaultProbeURL());
}

TEST_F(DataReductionProxyParamsTest, NoDevOrigin) {
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kPromoAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING &
      ~TestDataReductionProxyParams::HAS_DEV_ORIGIN);
  CheckParams(params, true, true, true, false, true);
  CheckValues(params,
              TestDataReductionProxyParams::DefaultOrigin(),
              TestDataReductionProxyParams::DefaultFallbackOrigin(),
              TestDataReductionProxyParams::DefaultSSLOrigin(),
              TestDataReductionProxyParams::DefaultAltOrigin(),
              TestDataReductionProxyParams::DefaultAltFallbackOrigin(),
              TestDataReductionProxyParams::DefaultProbeURL());
}

TEST_F(DataReductionProxyParamsTest, Flags) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxy,
      TestDataReductionProxyParams::FlagOrigin());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyFallback,
      TestDataReductionProxyParams::FlagFallbackOrigin());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionSSLProxy,
      TestDataReductionProxyParams::FlagSSLOrigin());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAlt,
      TestDataReductionProxyParams::FlagAltOrigin());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyAltFallback,
      TestDataReductionProxyParams::FlagAltFallbackOrigin());
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyProbeURL,
      TestDataReductionProxyParams::FlagProbeURL());
  TestDataReductionProxyParams params(
      DataReductionProxyParams::kAllowed |
      DataReductionProxyParams::kFallbackAllowed |
      DataReductionProxyParams::kAlternativeAllowed |
      DataReductionProxyParams::kPromoAllowed,
      TestDataReductionProxyParams::HAS_EVERYTHING);
  CheckParams(params, true, true, true, true, true);
  CheckValues(params,
              TestDataReductionProxyParams::FlagOrigin(),
              TestDataReductionProxyParams::FlagFallbackOrigin(),
              TestDataReductionProxyParams::FlagSSLOrigin(),
              TestDataReductionProxyParams::FlagAltOrigin(),
              TestDataReductionProxyParams::FlagAltFallbackOrigin(),
              TestDataReductionProxyParams::FlagProbeURL());
}

TEST_F(DataReductionProxyParamsTest, InvalidConfigurations) {
  const struct {
    bool allowed;
    bool fallback_allowed;
    bool alternative_allowed;
    bool promo_allowed;
    unsigned int missing_definitions;
    bool expected_result;
  } tests[]  = {
    {
      true,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_NOTHING,
      true
    },
    {
      true,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_DEV_ORIGIN,
      true
    },
    {
      true,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN,
      true
    },
    {
      true,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_ORIGIN,
      false
    },
    { true,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
      false
    },
    { true,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_SSL_ORIGIN,
      false
    },
    { true,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_ALT_ORIGIN,
      false
    },
    { true,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      false
    },
    { true,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_PROBE_URL,
      false
    },
    {
      true,
      false,
      true,
      true,
      TestDataReductionProxyParams::HAS_NOTHING,
      true
    },
    {
      true,
      false,
      true,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_ORIGIN,
      false
    },
    {
      true,
      false,
      true,
      true,
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      false,
      true,
      true,
      TestDataReductionProxyParams::HAS_SSL_ORIGIN,
      false
    },
    {
      true,
      false,
      true,
      true,
      TestDataReductionProxyParams::HAS_ALT_ORIGIN,
      false
    },
    {
      true,
      false,
      true,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      false,
      true,
      true,
      TestDataReductionProxyParams::HAS_PROBE_URL,
      false
    },

    {
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_NOTHING,
      true
    },
    {
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_ORIGIN,
      false
    },
    {
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
      false
    },
    {
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_SSL_ORIGIN,
      true
    },
    {
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_ORIGIN,
      true
    },
    {
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      true,
      false,
      true,
      TestDataReductionProxyParams::HAS_PROBE_URL,
      false
    },
    {
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_ORIGIN,
      false
    },
    {
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_SSL_ORIGIN,
      true
    },
    {
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_ORIGIN,
      true
    },
    {
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      true
    },
    {
      true,
      false,
      false,
      true,
      TestDataReductionProxyParams::HAS_PROBE_URL,
      false
    },
    {
      false,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_NOTHING,
      false
    },
    {
      false,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_ORIGIN |
          TestDataReductionProxyParams::HAS_DEV_ORIGIN,
      false
    },
    {
      false,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_FALLBACK_ORIGIN,
      false
    },
    {
      false,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_SSL_ORIGIN,
      false
    },
    {
      false,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_ALT_ORIGIN,
      false
    },
    {
      false,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_ALT_FALLBACK_ORIGIN,
      false
    },
    {
      false,
      true,
      true,
      true,
      TestDataReductionProxyParams::HAS_PROBE_URL,
      false
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    int flags = 0;
    if (tests[i].allowed)
      flags |= DataReductionProxyParams::kAllowed;
    if (tests[i].fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    if (tests[i].alternative_allowed)
      flags |= DataReductionProxyParams::kAlternativeAllowed;
    if (tests[i].promo_allowed)
      flags |= DataReductionProxyParams::kPromoAllowed;
    TestDataReductionProxyParams params(
        flags,
        TestDataReductionProxyParams::HAS_EVERYTHING &
            ~(tests[i].missing_definitions));
    EXPECT_EQ(tests[i].expected_result, params.init_result());
  }
}

TEST_F(DataReductionProxyParamsTest, IsDataReductionProxy) {
  const struct {
    net::HostPortPair host_port_pair;
    bool fallback_allowed;
    bool set_dev_origin;
    bool expected_result;
    net::HostPortPair expected_first;
    net::HostPortPair expected_second;
  } tests[]  = {
      { net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultOrigin())),
        true,
        false,
        true,
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultOrigin())),
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultFallbackOrigin()))
      },
      { net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultOrigin())),
        false,
        false,
        true,
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultOrigin())),
        net::HostPortPair::FromURL(GURL())
      },
      { net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultFallbackOrigin())),
        true,
        false,
        true,
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultFallbackOrigin())),
        net::HostPortPair::FromURL(GURL())
      },
      { net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultFallbackOrigin())),
        false,
        false,
        false,
        net::HostPortPair::FromURL(GURL()),
        net::HostPortPair::FromURL(GURL())
      },
      { net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultAltOrigin())),
        true,
        false,
        true,
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultAltOrigin())),
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultAltFallbackOrigin()))
      },
      { net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultAltOrigin())),
        false,
        false,
        true,
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultAltOrigin())),
        net::HostPortPair::FromURL(GURL())
      },
      { net::HostPortPair::FromURL(
            GURL(TestDataReductionProxyParams::DefaultAltFallbackOrigin())),
        true,
        false,
        true,
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultAltFallbackOrigin())),
        net::HostPortPair::FromURL(GURL())
      },
      { net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultAltFallbackOrigin())),
        false,
        false,
        false,
        net::HostPortPair::FromURL(GURL()),
        net::HostPortPair::FromURL(GURL())
      },
      { net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultSSLOrigin())),
        true,
        false,
        true,
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultSSLOrigin())),
        net::HostPortPair::FromURL(GURL())
      },
      { net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultDevOrigin())),
        true,
        true,
        true,
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultDevOrigin())),
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultFallbackOrigin()))
      },
      { net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultOrigin())),
        true,
        true,
        true,
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultOrigin())),
        net::HostPortPair::FromURL(GURL(
            TestDataReductionProxyParams::DefaultFallbackOrigin()))
      },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    int flags = DataReductionProxyParams::kAllowed |
                DataReductionProxyParams::kAlternativeAllowed;
    if (tests[i].fallback_allowed)
      flags |= DataReductionProxyParams::kFallbackAllowed;
    unsigned int has_definitions = TestDataReductionProxyParams::HAS_EVERYTHING;
    if (!tests[i].set_dev_origin) {
      has_definitions &= ~TestDataReductionProxyParams::HAS_DEV_ORIGIN;
    }
    TestDataReductionProxyParams params(flags, has_definitions);
    std::pair<GURL, GURL> proxy_servers;
    EXPECT_EQ(tests[i].expected_result,
              params.IsDataReductionProxy(
                  tests[i].host_port_pair, &proxy_servers));
    EXPECT_TRUE(tests[i].expected_first.Equals(
        net::HostPortPair::FromURL(proxy_servers.first)));
    EXPECT_TRUE(tests[i].expected_second.Equals(
        net::HostPortPair::FromURL(proxy_servers.second)));
  }
}
}  // namespace data_reduction_proxy
