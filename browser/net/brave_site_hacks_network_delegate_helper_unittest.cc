/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/net/brave_site_hacks_network_delegate_helper.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "brave/browser/net/url_context.h"
#include "brave/common/network_constants.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

using brave::ResponseCallback;

TEST(BraveSiteHacksNetworkDelegateHelperTest, UAWhitelistedTest) {
  const std::vector<const GURL> urls(
      {GURL("https://duckduckgo.com"), GURL("https://duckduckgo.com/something"),
       GURL("https://netflix.com"), GURL("https://netflix.com/something"),
       GURL("https://a.duckduckgo.com"),
       GURL("https://a.netflix.com"),
       GURL("https://a.duckduckgo.com/something"),
       GURL("https://a.netflix.com/something")});
  for (const auto& url : urls) {
    net::HttpRequestHeaders headers;
    headers.SetHeader(kUserAgentHeader,
                      "Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 "
                      "(KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
    auto brave_request_info = std::make_shared<brave::BraveRequestInfo>(url);
    int rc = brave::OnBeforeStartTransaction_SiteHacksWork(
        &headers, ResponseCallback(), brave_request_info);
    std::string user_agent;
    headers.GetHeader(kUserAgentHeader, &user_agent);
    EXPECT_EQ(rc, net::OK);
    EXPECT_EQ(user_agent,
              "Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 "
              "(KHTML, like Gecko) Brave Chrome/33.0.1750.117 Safari/537.36");
  }
}

TEST(BraveSiteHacksNetworkDelegateHelperTest, ChangeUAOnlyOnce) {
  const GURL whitelisted_url("https://netflix.com/");
  net::HttpRequestHeaders headers;
  headers.SetHeader(kUserAgentHeader,
                    "Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 "
                    "(KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
  auto brave_request_info =
      std::make_shared<brave::BraveRequestInfo>(whitelisted_url);

  // Check once.
  int rc = brave::OnBeforeStartTransaction_SiteHacksWork(
      &headers, ResponseCallback(), brave_request_info);
  std::string user_agent;
  headers.GetHeader(kUserAgentHeader, &user_agent);
  EXPECT_EQ(rc, net::OK);
  EXPECT_EQ(user_agent,
            "Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Brave Chrome/33.0.1750.117 Safari/537.36");

  // Check twice.
  rc = brave::OnBeforeStartTransaction_SiteHacksWork(
      &headers, ResponseCallback(), brave_request_info);
  headers.GetHeader(kUserAgentHeader, &user_agent);
  EXPECT_EQ(rc, net::OK);
  EXPECT_EQ(user_agent,
            "Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Brave Chrome/33.0.1750.117 Safari/537.36");
}

TEST(BraveSiteHacksNetworkDelegateHelperTest, NOTUAWhitelistedTest) {
  const std::vector<const GURL> urls({GURL("https://brianbondy.com"),
                                      GURL("https://bravecombo.com"),
                                      GURL("https://brave.example.com")});
  for (const auto& url : urls) {
    net::HttpRequestHeaders headers;
    headers.SetHeader(kUserAgentHeader,
                      "Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 "
                      "(KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
    auto brave_request_info = std::make_shared<brave::BraveRequestInfo>(url);
    int rc = brave::OnBeforeStartTransaction_SiteHacksWork(
        &headers, ResponseCallback(), brave_request_info);
    std::string user_agent;
    headers.GetHeader(kUserAgentHeader, &user_agent);
    EXPECT_EQ(rc, net::OK);
    EXPECT_EQ(user_agent,
              "Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 "
              "(KHTML, like Gecko) Chrome/33.0.1750.117 Safari/537.36");
  }
}

TEST(BraveSiteHacksNetworkDelegateHelperTest, ReferrerPreserved) {
  const std::vector<const GURL> urls(
      {GURL("https://brianbondy.com/7"), GURL("https://www.brianbondy.com/5"),
       GURL("https://brian.bondy.brianbondy.com")});
  for (const auto& url : urls) {
    net::HttpRequestHeaders headers;
    const GURL original_referrer("https://hello.brianbondy.com/about");

    auto brave_request_info = std::make_shared<brave::BraveRequestInfo>(url);
    brave_request_info->referrer = original_referrer;
    int rc = brave::OnBeforeURLRequest_SiteHacksWork(ResponseCallback(),
                                                     brave_request_info);
    EXPECT_EQ(rc, net::OK);
    // new_url should not be set.
    EXPECT_TRUE(brave_request_info->new_url_spec.empty());
    EXPECT_EQ(brave_request_info->referrer, original_referrer);
  }
}

TEST(BraveSiteHacksNetworkDelegateHelperTest, ReferrerTruncated) {
  const std::vector<const GURL> urls({GURL("https://digg.com/7"),
                                      GURL("https://slashdot.org/5"),
                                      GURL("https://bondy.brian.org")});
  for (const auto& url : urls) {
    const GURL original_referrer("https://hello.brianbondy.com/about");

    auto brave_request_info = std::make_shared<brave::BraveRequestInfo>(url);
    brave_request_info->referrer = original_referrer;
    int rc = brave::OnBeforeURLRequest_SiteHacksWork(ResponseCallback(),
                                                     brave_request_info);
    EXPECT_EQ(rc, net::OK);
    // new_url should not be set.
    EXPECT_TRUE(brave_request_info->new_url_spec.empty());
    EXPECT_TRUE(brave_request_info->new_referrer.has_value());
    EXPECT_EQ(brave_request_info->new_referrer.value(),
              original_referrer.GetOrigin().spec());
  }
}

TEST(BraveSiteHacksNetworkDelegateHelperTest,
     ReferrerWouldBeClearedButExtensionSite) {
  const std::vector<const GURL> urls({GURL("https://digg.com/7"),
                                      GURL("https://slashdot.org/5"),
                                      GURL("https://bondy.brian.org")});
  for (const auto& url : urls) {
    auto brave_request_info = std::make_shared<brave::BraveRequestInfo>(url);
    brave_request_info->tab_origin =
        GURL("chrome-extension://aemmndcbldboiebfnladdacbdfmadadm/");
    const GURL original_referrer("https://hello.brianbondy.com/about");
    brave_request_info->referrer = original_referrer;

    int rc = brave::OnBeforeURLRequest_SiteHacksWork(ResponseCallback(),
                                                     brave_request_info);
    EXPECT_EQ(rc, net::OK);
    // new_url should not be set
    EXPECT_TRUE(brave_request_info->new_url_spec.empty());
    EXPECT_EQ(brave_request_info->referrer, original_referrer);
  }
}
