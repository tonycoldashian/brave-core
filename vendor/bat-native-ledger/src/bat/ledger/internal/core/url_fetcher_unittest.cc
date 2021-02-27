/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/core/url_fetcher.h"

#include <string>
#include <utility>

#include "base/callback_helpers.h"
#include "bat/ledger/internal/core/bat_ledger_test.h"

namespace ledger {

class URLFetcherTest : public BATLedgerTest {};

TEST_F(URLFetcherTest, FetchDelegatesToLedgerClient) {
  auto test_response = mojom::UrlResponse::New();
  test_response->status_code = 200;
  test_response->body = "hello_world";
  AddNetworkResultForTesting("test-url", mojom::UrlMethod::GET,
                             std::move(test_response));

  auto request = mojom::UrlRequest::New();
  request->url = "test-url";

  mojom::UrlResponsePtr response;

  context()
      .Get<URLFetcher>()
      .Fetch(std::move(request))
      .Then(base::BindLambdaForTesting(
          [&response](mojom::UrlResponsePtr r) { response = r->Clone(); }));

  task_environment()->RunUntilIdle();
  ASSERT_TRUE(response);
  EXPECT_EQ(response->status_code, 200);
  EXPECT_EQ(response->body, "hello_world");
}

TEST_F(URLFetcherTest, OnlyWhitelistedRequestHeadersAreLogged) {
  std::string log_output;
  SetLogCallbackForTesting(base::BindLambdaForTesting(
      [&log_output](const std::string& message) { log_output += message; }));

  auto request = mojom::UrlRequest::New();
  request->url = "test-url";

  // Whitelisted headers:
  request->headers.push_back("Digest=digest_value");
  request->headers.push_back("sIgnature=signature_value");
  request->headers.push_back("accepT=accept_value");
  request->headers.push_back("contenT-type=content_type_value");

  // Non-whitelisted headers:
  request->headers.push_back("secret=secret_value");
  request->headers.push_back("Cookie=cookie_value");
  request->headers.push_back("Authorization=auth_value");

  context().Get<URLFetcher>().Fetch(std::move(request)).Then(base::DoNothing());

  task_environment()->RunUntilIdle();

  EXPECT_NE(log_output.find("digest_value"), std::string::npos);
  EXPECT_NE(log_output.find("signature_value"), std::string::npos);
  EXPECT_NE(log_output.find("accept_value"), std::string::npos);
  EXPECT_NE(log_output.find("content_type_value"), std::string::npos);

  EXPECT_EQ(log_output.find("secret_value"), std::string::npos);
  EXPECT_EQ(log_output.find("cookie_value"), std::string::npos);
  EXPECT_EQ(log_output.find("auth_value"), std::string::npos);
}

TEST_F(URLFetcherTest, DisableLogging) {
  std::string log_output;
  SetLogCallbackForTesting(base::BindLambdaForTesting(
      [&log_output](const std::string& message) { log_output += message; }));

  auto test_response = mojom::UrlResponse::New();
  test_response->status_code = 200;
  test_response->body = "hello_world";
  AddNetworkResultForTesting("test-url", mojom::UrlMethod::GET,
                             std::move(test_response));

  auto request = mojom::UrlRequest::New();
  request->url = "test-url";

  context()
      .Get<URLFetcher>()
      .Fetch(std::move(request), {.disable_logging = true})
      .Then(base::DoNothing());

  task_environment()->RunUntilIdle();
  EXPECT_EQ(log_output, "");
}

TEST_F(URLFetcherTest, LogResponseBody) {
  std::string log_output;
  SetLogCallbackForTesting(base::BindLambdaForTesting(
      [&log_output](const std::string& message) { log_output += message; }));

  auto test_response = mojom::UrlResponse::New();
  test_response->status_code = 200;
  test_response->body = "hello_world";
  AddNetworkResultForTesting("test-url", mojom::UrlMethod::GET,
                             std::move(test_response));

  auto request = mojom::UrlRequest::New();
  request->url = "test-url";

  context()
      .Get<URLFetcher>()
      .Fetch(std::move(request), {.log_response_body = true})
      .Then(base::DoNothing());

  task_environment()->RunUntilIdle();
  EXPECT_NE(log_output.find("hello_world"), std::string::npos);
}

TEST_F(URLFetcherTest, ErrorLogging) {
  std::string log_output;
  SetLogCallbackForTesting(base::BindLambdaForTesting(
      [&log_output](const std::string& message) { log_output += message; }));

  auto test_response = mojom::UrlResponse::New();
  test_response->status_code = 500;
  test_response->body = "error!!";
  AddNetworkResultForTesting("test-url", mojom::UrlMethod::GET,
                             std::move(test_response));

  auto request = mojom::UrlRequest::New();
  request->url = "test-url";

  context().Get<URLFetcher>().Fetch(std::move(request)).Then(base::DoNothing());

  task_environment()->RunUntilIdle();
  EXPECT_NE(log_output.find("error!!"), std::string::npos);
}

}  // namespace ledger
