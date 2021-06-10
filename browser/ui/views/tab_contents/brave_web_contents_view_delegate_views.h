/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_UI_VIEWS_TAB_CONTENTS_BRAVE_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_H_
#define BRAVE_BROWSER_UI_VIEWS_TAB_CONTENTS_BRAVE_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_H_

#include <memory>

#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views.h"

class RenderViewContextMenuBase;

namespace content {
class WebContents;
}

class BraveWebContentsViewDelegateViews
    : public ChromeWebContentsViewDelegateViews {
 public:
  explicit BraveWebContentsViewDelegateViews(
      content::WebContents* web_contents);
  ~BraveWebContentsViewDelegateViews() override;

  void ShowMenu(std::unique_ptr<RenderViewContextMenuBase> menu) override;
};

#endif  // BRAVE_BROWSER_UI_VIEWS_TAB_CONTENTS_BRAVE_WEB_CONTENTS_VIEW_DELEGATE_VIEWS_H_
