/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/core/bat_ledger_initializer.h"

#include <utility>

#include "bat/ledger/internal/core/bat_ledger_job.h"

namespace ledger {

namespace {

template <typename... Ts>
class InitializeJob : public BATLedgerJob<bool> {
 public:
  void Start() { StartNext<Ts...>(true); }

 private:
  template <typename T, typename... Rest>
  void StartNext(bool success) {
    if (!success) {
      Complete(false);
      return;
    }

    context().template Get<T>().Initialize().Then(
        ContinueWith(&InitializeJob::StartNext<Rest...>));
  }

  void StartNext(bool success) { Complete(success); }
};

class InitializeAllJob : public BATLedgerJob<bool> {
 public:
  void Start() { Complete(true); }
};

}  // namespace

const size_t BATLedgerInitializer::kComponentKey =
    BATLedgerContext::ReserveComponentKey();

BATLedgerInitializer::BATLedgerInitializer() = default;
BATLedgerInitializer::~BATLedgerInitializer() = default;

AsyncResult<bool> BATLedgerInitializer::Initialize() {
  return initialize_cache_.GetResult([this]() {
    return context().StartJob<InitializeAllJob>().Then(
        base::BindOnce([](bool success) {
          return std::make_pair(success, base::TimeDelta::Max());
        }));
  });
}

}  // namespace ledger
