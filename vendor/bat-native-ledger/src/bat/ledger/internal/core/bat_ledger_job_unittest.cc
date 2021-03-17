/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ledger/internal/core/bat_ledger_job.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "bat/ledger/internal/core/bat_ledger_test.h"

namespace ledger {

class BATLedgerJobTest : public BATLedgerTest {};

TEST_F(BATLedgerJobTest, StartJob) {
  class Job : public BATLedgerJob<bool> {
   public:
    void Start(int n) {
      AsyncResult<int>::Resolver r;
      r.result().Then(ContinueWith(&Job::OnDone));
      r.Complete(n);
    }

   private:
    void OnDone(int n) { Complete(static_cast<bool>(n)); }
  };

  bool value = false;
  context().StartJob<Job>(10).Then(
      base::BindLambdaForTesting([&value](bool v) { value = v; }));

  task_environment()->RunUntilIdle();
  EXPECT_TRUE(value);
}

}  // namespace ledger
