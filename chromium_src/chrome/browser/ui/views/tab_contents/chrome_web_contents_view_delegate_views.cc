/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/tab_contents/brave_web_contents_view_delegate_views.h"

#define CreateWebContentsViewDelegate CreateWebContentsViewDelegate_ChromiumImpl
#include "../../../../../../../chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views.cc"
#undef CreateWebContentsViewDelegate

content::WebContentsViewDelegate* CreateWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return new BraveWebContentsViewDelegateViews(web_contents);
}
