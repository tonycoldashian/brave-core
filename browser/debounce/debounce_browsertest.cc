/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "base/base64url.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "brave/browser/brave_browser_process.h"
#include "brave/browser/extensions/brave_base_local_data_files_browsertest.h"
#include "brave/common/brave_paths.h"
#include "brave/components/brave_shields/browser/brave_shields_util.h"
#include "brave/components/debounce/browser/debounce_download_service.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"

using debounce::DebounceDownloadService;

const char kTestDataDirectory[] = "debounce-data";

class DebounceDownloadServiceWaiter : public DebounceDownloadService::Observer {
 public:
  explicit DebounceDownloadServiceWaiter(
      DebounceDownloadService* download_service)
      : download_service_(download_service), scoped_observer_(this) {
    scoped_observer_.Add(download_service_);
  }
  ~DebounceDownloadServiceWaiter() override = default;

  void Wait() { run_loop_.Run(); }

 private:
  // DebounceDownloadService::Observer:
  void OnRulesReady(DebounceDownloadService* download_service) override {
    run_loop_.QuitWhenIdle();
  }

  DebounceDownloadService* const download_service_;
  base::RunLoop run_loop_;
  ScopedObserver<DebounceDownloadService, DebounceDownloadService::Observer>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(DebounceDownloadServiceWaiter);
};

class DebounceBrowserTest : public BaseLocalDataFilesBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BaseLocalDataFilesBrowserTest::SetUpOnMainThread();

    simple_landing_url_ =
        embedded_test_server()->GetURL("a.com", "/simple.html");
    redirect_to_cross_site_landing_url_ = embedded_test_server()->GetURL(
        "redir.b.com", "/cross-site/a.com/simple.html");
    redirect_to_same_site_landing_url_ = embedded_test_server()->GetURL(
        "redir.a.com", "/cross-site/a.com/simple.html");

    cross_site_url_ =
        embedded_test_server()->GetURL("b.com", "/navigate-to-site.html");
    same_site_url_ =
        embedded_test_server()->GetURL("sub.a.com", "/navigate-to-site.html");
  }

  // BaseLocalDataFilesBrowserTest overrides
  const char* test_data_directory() override { return kTestDataDirectory; }
  const char* embedded_test_server_directory() override { return ""; }
  LocalDataFilesObserver* service() override {
    return g_brave_browser_process->debounce_download_service();
  }

  void WaitForService() override {
    // Wait for debounce download service to load and parse its
    // configuration file.
    debounce::DebounceDownloadService* download_service =
        g_brave_browser_process->debounce_download_service();
    DebounceDownloadServiceWaiter(download_service).Wait();
  }

  HostContentSettingsMap* content_settings() {
    return HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  GURL url(const GURL& destination_url, const GURL& navigation_url) {
    std::string encoded_destination;
    base::Base64UrlEncode(destination_url.spec(),
                          base::Base64UrlEncodePolicy::OMIT_PADDING,
                          &encoded_destination);
    const std::string query =
        base::StringPrintf("url=%s", encoded_destination.c_str());
    GURL::Replacements replacement;
    replacement.SetQueryStr(query);
    return navigation_url.ReplaceComponents(replacement);
  }

  GURL landing_url(const base::StringPiece& query, const GURL& landing_url) {
    GURL::Replacements replacement;
    if (!query.empty()) {
      replacement.SetQueryStr(query);
    }
    return landing_url.ReplaceComponents(replacement);
  }

  const GURL& redirect_to_cross_site_landing_url() {
    return redirect_to_cross_site_landing_url_;
  }
  const GURL& redirect_to_same_site_landing_url() {
    return redirect_to_same_site_landing_url_;
  }
  const GURL& simple_landing_url() { return simple_landing_url_; }

  const GURL& cross_site_url() { return cross_site_url_; }
  const GURL& redirect_to_cross_site_url() {
    return redirect_to_cross_site_url_;
  }
  const GURL& same_site_url() { return same_site_url_; }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void NavigateToURLAndWaitForRedirects(const GURL& original_url,
                                        const GURL& landing_url) {
    ui_test_utils::UrlLoadObserver load_complete(
        landing_url, content::NotificationService::AllSources());
    ui_test_utils::NavigateToURL(browser(), original_url);
    EXPECT_EQ(contents()->GetMainFrame()->GetLastCommittedURL(), original_url);
    load_complete.Wait();

    EXPECT_EQ(contents()->GetLastCommittedURL(), landing_url);
  }

 private:
  GURL cross_site_url_;
  GURL redirect_to_cross_site_landing_url_;
  GURL redirect_to_cross_site_url_;
  GURL redirect_to_same_site_landing_url_;
  GURL same_site_url_;
  GURL simple_landing_url_;
};

IN_PROC_BROWSER_TEST_F(DebounceBrowserTest, QueryStringFilterShieldsDown) {
  ASSERT_TRUE(InstallMockExtension());

  const std::string inputs[] = {
      "", "foo=bar", "fbclid=1", "fbclid=2&key=value", "key=value&fbclid=3",
  };

  constexpr size_t input_count = base::size(inputs);

  for (size_t i = 0; i < input_count; i++) {
    const GURL dest_url = landing_url(inputs[i], simple_landing_url());
    brave_shields::SetBraveShieldsEnabled(content_settings(), false, dest_url);
    NavigateToURLAndWaitForRedirects(url(dest_url, cross_site_url()), dest_url);
  }
}

IN_PROC_BROWSER_TEST_F(DebounceBrowserTest, QueryStringFilterDirectNavigation) {
  ASSERT_TRUE(InstallMockExtension());

  const std::string inputs[] = {
      "",
      "abc=1",
      "fbclid=1",
  };
  const std::string outputs[] = {
      // URLs without trackers should be untouched.
      "",
      "abc=1",
      // URLs with trackers should have those removed.
      "",
  };

  constexpr size_t input_count = base::size(inputs);
  static_assert(input_count == base::size(outputs),
                "Inputs and outputs must have the same number of elements.");

  for (size_t i = 0; i < input_count; i++) {
    // Direct navigations go through the query filter.
    GURL input = landing_url(inputs[i], simple_landing_url());
    GURL output = landing_url(outputs[i], simple_landing_url());
    ui_test_utils::NavigateToURL(browser(), input);
    EXPECT_EQ(contents()->GetLastCommittedURL(), output);
  }
}
