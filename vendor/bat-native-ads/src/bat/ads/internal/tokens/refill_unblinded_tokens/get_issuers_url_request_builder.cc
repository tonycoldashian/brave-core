/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/tokens/refill_unblinded_tokens/get_issuers_url_request_builder.h"

#include <cstdint>
#include <utility>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "bat/ads/ads.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/security/crypto_util.h"
#include "bat/ads/internal/server/confirmations_server_util.h"
#include "bat/ads/internal/server/via_header_util.h"

namespace ads {

GetIssuersUrlRequestBuilder::GetIssuersUrlRequestBuilder() =
    default;

GetIssuersUrlRequestBuilder::~GetIssuersUrlRequestBuilder() =
    default;

// GET /v1/issuers/

UrlRequestPtr GetIssuersUrlRequestBuilder::Build() {
  UrlRequestPtr url_request = UrlRequest::New();
  url_request->url = BuildUrl();
  const std::string body = "";
  url_request->headers = BuildHeaders(body);
  url_request->content = body;
  url_request->content_type = "application/json";
  url_request->method = UrlRequestMethod::GET;

  return url_request;
}

///////////////////////////////////////////////////////////////////////////////

std::string GetIssuersUrlRequestBuilder::BuildUrl() const {
  const std::string kGetIssuersUrlMask =
      base::StringPrintf("%%s%s", kGetIssuersUrlPath);
  return base::StringPrintf(kGetIssuersUrlMask.c_str(),
                            confirmations::server::GetHost().c_str());
}

std::vector<std::string> GetIssuersUrlRequestBuilder::BuildHeaders(
    const std::string& body) const {
  std::vector<std::string> headers;

  const std::string content_type_header = "content-type: application/json";
  headers.push_back(content_type_header);

  const std::string via_header = server::BuildViaHeader();
  headers.push_back(via_header);

  const std::string accept_header = "accept: application/json";
  headers.push_back(accept_header);

  return headers;
}

std::string GetIssuersUrlRequestBuilder::BuildBody() const {
  return "";
}

}  // namespace ads
