/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_EPHEMERAL_STORAGE_EPHEMERAL_STORAGE_BROWSERTEST_H_
#define BRAVE_BROWSER_EPHEMERAL_STORAGE_EPHEMERAL_STORAGE_BROWSERTEST_H_

#include <string>

#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

class EphemeralStorageBrowserTest : public InProcessBrowserTest {
 public:
  enum StorageType { Session, Local };

  struct ValuesFromFrame {
    content::EvalJsResult local_storage;
    content::EvalJsResult session_storage;
    content::EvalJsResult cookies;
  };

  struct ValuesFromFrames {
    ValuesFromFrame main_frame;
    ValuesFromFrame iframe_1;
    ValuesFromFrame iframe_2;
  };

  EphemeralStorageBrowserTest();
  EphemeralStorageBrowserTest(const EphemeralStorageBrowserTest&) = delete;
  EphemeralStorageBrowserTest& operator=(const EphemeralStorageBrowserTest&) =
      delete;
  ~EphemeralStorageBrowserTest() override;

  void SetUpOnMainThread() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  void SetValuesInFrame(content::RenderFrameHost* frame,
                        std::string storage_value,
                        std::string cookie_value);

  void SetValuesInFrames(content::WebContents* web_contents,
                         std::string storage_value,
                         std::string cookie_value);

  ValuesFromFrame GetValuesFromFrame(content::RenderFrameHost* frame);
  ValuesFromFrames GetValuesFromFrames(content::WebContents* web_contents);

  content::WebContents* LoadURLInNewTab(GURL url);

  void SetStorageValueInFrame(content::RenderFrameHost* host,
                              std::string value,
                              StorageType storage_type);
  content::EvalJsResult GetStorageValueInFrame(content::RenderFrameHost* host,
                                               StorageType storage_type);
  void SetCookieInFrame(content::RenderFrameHost* host, std::string cookie);
  content::EvalJsResult GetCookiesInFrame(content::RenderFrameHost* host);
  void WaitForCleanupAfterKeepAlive();
  void ExpectValuesFromFramesAreEmpty(const base::Location& location,
                                      const ValuesFromFrames& values);

 protected:
  net::test_server::EmbeddedTestServer https_server_;
  GURL a_site_ephemeral_storage_url_;
  GURL b_site_ephemeral_storage_url_;
  GURL c_site_ephemeral_storage_url_;
};

#endif  // BRAVE_BROWSER_EPHEMERAL_STORAGE_EPHEMERAL_STORAGE_BROWSERTEST_H_
