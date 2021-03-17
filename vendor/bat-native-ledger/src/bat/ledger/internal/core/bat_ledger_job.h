/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_BAT_LEDGER_JOB_H_
#define BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_BAT_LEDGER_JOB_H_

#include <utility>

#include "base/memory/weak_ptr.h"
#include "bat/ledger/internal/core/async_result.h"
#include "bat/ledger/internal/core/bat_ledger_context.h"

namespace ledger {

// Convenience class for defining job classes that can be started by calling
// |BATLedgerContext::StartJob|. Subclasses must implement a |Start| method
// that begins the asynchronous operation.
//
// Example:
//   class MyJob : public BATLedgerJob<int> {
//    public:
//     void Start() {
//       Complete(42);
//     }
//   };
template <typename T>
class BATLedgerJob : public BATLedgerContext::Object,
                     public base::SupportsWeakPtr<BATLedgerJob<T>> {
 public:
  using Result = AsyncResult<T>;

  // Returns the AsyncResult for the job.
  Result result() const { return resolver_.result(); }

 protected:
  // Completes the job with the specified value
  void Complete(typename Result::CompleteType value) {
    resolver_.Complete(std::move(value));
  }

  // Returns a base::OnceCallback that wraps the specified member function. The
  // resulting callback is bound with a WeakPtr for the receiver. It is not
  // bound with any additional arguments.
  template <typename Derived, typename... Args>
  auto ContinueWith(void (Derived::*fn)(Args...)) {
    return base::BindOnce(fn, base::AsWeakPtr(static_cast<Derived*>(this)));
  }

 private:
  typename Result::Resolver resolver_;
};

}  // namespace ledger

#endif  // BRAVE_VENDOR_BAT_NATIVE_LEDGER_SRC_BAT_LEDGER_INTERNAL_CORE_BAT_LEDGER_JOB_H_
