/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/core/user_encryption.h"

#include "base/base64.h"

namespace ledger {

const size_t UserEncryption::kComponentKey =
    BATLedgerContext::ReserveComponentKey();

AsyncResult<std::string> UserEncryption::EncryptString(
    const std::string& plain_text) {
  AsyncResult<std::string>::Resolver resolver;
  context().GetLedgerClient()->EncryptString(
      plain_text, [resolver](const std::string& output) mutable {
        resolver.Complete(output);
      });
  return resolver.result();
}

AsyncResult<std::string> UserEncryption::Base64EncryptString(
    const std::string& plain_text) {
  return EncryptString(plain_text).Then(base::BindOnce([](std::string output) {
    std::string encoded;
    base::Base64Encode(output, &encoded);
    return encoded;
  }));
}

AsyncResult<std::string> UserEncryption::DecryptString(
    const std::string& encrypted) {
  AsyncResult<std::string>::Resolver resolver;
  context().GetLedgerClient()->DecryptString(
      encrypted, [resolver](const std::string& output) mutable {
        resolver.Complete(output);
      });
  return resolver.result();
}

AsyncResult<std::string> UserEncryption::Base64DecryptString(
    const std::string& encrypted) {
  std::string decoded;
  if (!base::Base64Decode(encrypted, &decoded)) {
    AsyncResult<std::string>::Resolver resolver;
    resolver.Complete("");
    return resolver.result();
  }

  return DecryptString(decoded);
}

}  // namespace ledger
