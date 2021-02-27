/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/core/url_fetcher.h"

#include <array>
#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "bat/ledger/internal/ledger_impl.h"
#include "bat/ledger/ledger_client.h"

namespace {

constexpr std::array<const char*, 4> kRequestHeadersForLogging = {
    "digest", "signature", "accept", "content-type"};

constexpr size_t kMaxResponseBodySizeForLogging = 1024;

base::StringPiece GetResponseBodyForLogging(const std::string& body) {
  base::StringPiece result(body);
  return result.substr(0, kMaxResponseBodySizeForLogging);
}

}  // namespace

namespace ledger {

const size_t URLFetcher::kComponentKey =
    BATLedgerContext::ReserveComponentKey();

URLFetcher::URLFetcher() = default;
URLFetcher::~URLFetcher() = default;

URLFetcher::FetchResult URLFetcher::Fetch(mojom::UrlRequestPtr request) {
  return FetchImpl(std::move(request), {});
}

URLFetcher::FetchResult URLFetcher::Fetch(mojom::UrlRequestPtr request,
                                          FetchOptions options) {
  return FetchImpl(std::move(request), options);
}

URLFetcher::FetchResult URLFetcher::FetchImpl(mojom::UrlRequestPtr request,
                                              FetchOptions options) {
  DCHECK(request);
  LogURLRequest(*request, options);

  FetchResult::Resolver resolver;

  context().GetLedgerClient()->LoadURL(
      std::move(request),
      [weak_self = weak_factory_.GetWeakPtr(), resolver,
       options](const mojom::UrlResponse& response) mutable {
        if (weak_self)
          weak_self->LogURLResponse(response, options);

        resolver.Complete(response.Clone());
      });

  return resolver.result();
}

void URLFetcher::LogURLRequest(const mojom::UrlRequest& request,
                               FetchOptions options) {
  if (options.disable_logging)
    return;

  auto stream = context().LogVerbose(FROM_HERE);

  stream << "\n[ REQUEST ]";
  stream << "\n> URL: " << request.url;
  stream << "\n> Method: " << request.method;

  if (!request.content.empty())
    stream << "\n> Content: " << request.content;

  if (!request.content_type.empty())
    stream << "\n> Content-Type: " << request.content_type;

  for (const std::string& header : request.headers) {
    for (const char* name : kRequestHeadersForLogging) {
      if (base::StartsWith(header, name, base::CompareCase::INSENSITIVE_ASCII))
        stream << "\n> Header " << header;
    }
  }
}

void URLFetcher::LogURLResponse(const mojom::UrlResponse& response,
                                FetchOptions options) {
  if (options.disable_logging)
    return;

  auto stream = context().LogVerbose(FROM_HERE);

  std::string result;
  bool log_body = options.log_response_body;

  if (!response.error.empty()) {
    result = "Error (" + response.error + ")";
  } else if (response.status_code >= 200 && response.status_code < 300) {
    result = "Success";
  } else {
    log_body = true;
    result = "Failure";
  }

  stream << "\n[ RESPONSE ]";
  stream << "\n> URL: " << response.url;
  stream << "\n> Result: " << result;
  stream << "\n> HTTP Status: " << response.status_code;

  if (log_body && !response.body.empty())
    stream << "\n> Body:\n" << GetResponseBodyForLogging(response.body);
}

}  // namespace ledger
