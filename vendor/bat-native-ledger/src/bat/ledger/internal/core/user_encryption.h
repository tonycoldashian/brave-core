/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_USER_ENCRYPTION_H_
#define BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_USER_ENCRYPTION_H_

#include <string>

#include "bat/ledger/internal/core/async_result.h"
#include "bat/ledger/internal/core/bat_ledger_context.h"

namespace ledger {

class UserEncryption : public BATLedgerContext::Object {
 public:
  static const size_t kComponentKey;

  AsyncResult<std::string> EncryptString(const std::string& plain_text);
  AsyncResult<std::string> Base64EncryptString(const std::string& plain_text);

  AsyncResult<std::string> DecryptString(const std::string& encrypted);
  AsyncResult<std::string> Base64DecryptString(const std::string& encrypted);
};

}  // namespace ledger

#endif  // BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_USER_ENCRYPTION_H_
