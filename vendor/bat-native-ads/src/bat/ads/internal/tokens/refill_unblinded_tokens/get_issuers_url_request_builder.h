/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_TOKENS_REFILL_UNBLINDED_TOKENS_GET_ISSUERS_URL_REQUEST_BUILDER_H_
#define BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_TOKENS_REFILL_UNBLINDED_TOKENS_GET_ISSUERS_URL_REQUEST_BUILDER_H_

#include <string>
#include <vector>

#include "bat/ads/internal/server/url_request_builder.h"
#include "bat/ads/mojom.h"
#include "wrapper.hpp"

namespace ads {

const char kGetIssuersUrlPath[] = "/v1/issuers/";

class GetIssuersUrlRequestBuilder : UrlRequestBuilder {
 public:
  GetIssuersUrlRequestBuilder();

  ~GetIssuersUrlRequestBuilder() override;

  UrlRequestPtr Build() override;

 private:
  std::string BuildUrl() const;

  std::vector<std::string> BuildHeaders(const std::string& body) const;

  std::string BuildBody() const;
};

}  // namespace ads

#endif  // BRAVE_VENDOR_BAT_NATIVE_ADS_SRC_BAT_ADS_INTERNAL_TOKENS_REFILL_UNBLINDED_TOKENS_GET_ISSUERS_URL_REQUEST_BUILDER_H_
