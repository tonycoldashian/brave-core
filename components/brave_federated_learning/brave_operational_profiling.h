/* Copyright 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_BRAVE_FEDERATED_LEARNING_BRAVE_OPERATIONAL_PROFILING_H_
#define BRAVE_COMPONENTS_BRAVE_FEDERATED_LEARNING_BRAVE_OPERATIONAL_PROFILING_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

class PrefRegistrySimple;

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace brave {

class BraveOperationalProfiling {
 public:
  explicit BraveOperationalProfiling(
      PrefService* pref_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~BraveOperationalProfiling();

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  void Start();

 private:
  void OnCollectionSlotStartTimerFired();
  void OnSimulateLocalTrainingStepTimerFired();
  void OnUploadComplete(std::unique_ptr<std::string> response_body);

  void SendCollectionSlot();

  void SavePrefs();
  void LoadPrefs();
  void LoadParams();

  std::string BuildTraceCollectionPayload() const;
  int GetCurrentCollectionSlot() const;
  std::string GetPlatformIdentifier();

  void reuseOrRefreshEphemeralId();

  PrefService* pref_service_;
  GURL operational_profiling_endpoint_;
  std::unique_ptr<base::RepeatingTimer> collection_slot_periodic_timer_;
  std::unique_ptr<base::RetainingOneShotTimer>
      simulate_local_training_step_timer_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  int ephemeral_id_lifetime_;
  base::Time ephemeral_id_expiration_;
  int current_collected_slot_;
  int last_checked_slot_;
  int collection_slot_size_;
  int fake_update_duration_;
  std::string ephemeral_id_;
  std::string platform_;
};

}  // namespace brave

#endif  // BRAVE_COMPONENTS_BRAVE_FEDERATED_LEARNING_BRAVE_OPERATIONAL_PROFILING_H_
