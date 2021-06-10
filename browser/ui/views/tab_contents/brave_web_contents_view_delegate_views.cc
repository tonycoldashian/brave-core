/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/ui/views/tab_contents/brave_web_contents_view_delegate_views.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views.h"
#include "components/renderer_context_menu/render_view_context_menu_base.h"

BraveWebContentsViewDelegateViews::BraveWebContentsViewDelegateViews(
    content::WebContents* web_contents)
    : ChromeWebContentsViewDelegateViews(web_contents) {}

BraveWebContentsViewDelegateViews::~BraveWebContentsViewDelegateViews() =
    default;

void BraveWebContentsViewDelegateViews::ShowMenu(
    std::unique_ptr<RenderViewContextMenuBase> menu) {
  menu->RemoveAdjacentSeparators();
  ChromeWebContentsViewDelegateViews::ShowMenu(std::move(menu));
}
