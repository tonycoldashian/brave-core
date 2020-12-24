/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/ftx/browser/ftx_pref_utils.h"

#include "components/prefs/pref_registry_simple.h"
#include "brave/components/ftx/common/pref_names.h"

namespace ftx {

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kFTXNewTabPageShowFTX, true);
  registry->RegisterStringPref(kFTXAccessToken, "");
  registry->RegisterStringPref(kFTXOauthHost, "ftx.com");
}

}  // namespace ftx
