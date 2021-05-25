/* Copyright 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include <string>
#include <utility>

#include "brave/components/brave_federated_learning/brave_operational_profiling.h"

#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "brave/components/brave_federated_learning/brave_operational_profiling_features.h"
#include "brave/components/brave_stats/browser/brave_stats_updater_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace brave {

namespace {

static constexpr char federatedLearningUrl[] = "https://fl.bravesoftware.com/";

constexpr char kLastCheckedSlotPrefName[] = "brave.federated.last_checked_slot";
constexpr char kEphemeralIDPrefName[] = "brave.federated.ephemeral_id";
constexpr char kEphemeralIDExpirationPrefName[] =
    "brave.federated.ephemeral_id_expiration";

net::NetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag() {
  return net::DefineNetworkTrafficAnnotation("brave_operational_profiling", R"(
        semantics {
          sender: "Operational Profiling Service"
          description:
            "Report of anonymized usage statistics. For more info see "
            "TODO: https://wikilink_here"
          trigger:
            "Reports are automatically generated on startup and at intervals "
            "while Brave is running."
          data:
            "Anonymized and encrypted usage data."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This service is enabled only when P3A is enabled and the user"
            "has opted-in to ads."
          policy_exception_justification:
            "Not implemented."
        }
    )");
}

}  // anonymous namespace

BraveOperationalProfiling::BraveOperationalProfiling(
    PrefService* pref_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pref_service_(pref_service), url_loader_factory_(url_loader_factory) {}

BraveOperationalProfiling::~BraveOperationalProfiling() {}

void BraveOperationalProfiling::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kLastCheckedSlotPrefName, -1);
  registry->RegisterStringPref(kEphemeralIDPrefName, {});
  registry->RegisterTimePref(kEphemeralIDExpirationPrefName, base::Time());
}

void BraveOperationalProfiling::Start() {
  DCHECK(!simulate_local_training_step_timer_);
  DCHECK(!collection_slot_periodic_timer_);

  LoadPrefs();

  simulate_local_training_step_timer_ =
      std::make_unique<base::RetainingOneShotTimer>();
  simulate_local_training_step_timer_->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(fake_update_duration_ * 60), this,
      &BraveOperationalProfiling::OnSimulateLocalTrainingStepTimerFired);

  collection_slot_periodic_timer_ = std::make_unique<base::RepeatingTimer>();
  collection_slot_periodic_timer_->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(collection_slot_size_ * 60 / 2),
      this, &BraveOperationalProfiling::OnCollectionSlotStartTimerFired);
}

void BraveOperationalProfiling::LoadParams() {
  collection_slot_size_ =
      operational_profiling::features::GetCollectionSlotSizeValue();
  fake_update_duration_ = operational_profiling::features::
      GetSimulateLocalTrainingStepDurationValue();
  ephemeral_id_lifetime_ =
      operational_profiling::features::GetEphemeralIdLifetime();

  operational_profiling_endpoint_ = GURL(federatedLearningUrl);

  LoadPrefs();
  reuseOrRefreshEphemeralId();
}

void BraveOperationalProfiling::LoadPrefs() {
  platform_ = GetPlatformIdentifier();
  last_checked_slot_ = pref_service_->GetInteger(kLastCheckedSlotPrefName);
  ephemeral_id_ = pref_service_->GetString(kEphemeralIDPrefName);
  ephemeral_id_expiration_ =
      pref_service_->GetTime(kEphemeralIDExpirationPrefName);
}

void BraveOperationalProfiling::SavePrefs() {
  pref_service_->SetInteger(kLastCheckedSlotPrefName, last_checked_slot_);
  pref_service_->SetString(kEphemeralIDPrefName, ephemeral_id_);
  pref_service_->SetTime(kEphemeralIDExpirationPrefName,
                         ephemeral_id_expiration_);
}

void BraveOperationalProfiling::OnCollectionSlotStartTimerFired() {
  simulate_local_training_step_timer_->Reset();
}

void BraveOperationalProfiling::OnSimulateLocalTrainingStepTimerFired() {
  SendCollectionSlot();
}

void BraveOperationalProfiling::SendCollectionSlot() {
  current_collected_slot_ = GetCurrentCollectionSlot();
  if (current_collected_slot_ == last_checked_slot_) {
    return;
  }

  reuseOrRefreshEphemeralId();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = operational_profiling_endpoint_;
  resource_request->headers.SetHeader("X-Brave-Trace", "?1");

  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetNetworkTrafficAnnotationTag());
  url_loader_->AttachStringForUpload(BuildTraceCollectionPayload(),
                                     "application/base64");

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&BraveOperationalProfiling::OnUploadComplete,
                     base::Unretained(this)));
}

void BraveOperationalProfiling::OnUploadComplete(
    std::unique_ptr<std::string> response_body) {
  if (url_loader_->ResponseInfo()->headers->response_code() == 200) {
    last_checked_slot_ = current_collected_slot_;
    SavePrefs();
  }
}

std::string BraveOperationalProfiling::BuildTraceCollectionPayload() const {
  base::Value root(base::Value::Type::DICTIONARY);

  root.SetKey("ephemeral_id", base::Value(ephemeral_id_));
  root.SetKey("platform", base::Value(platform_));
  root.SetKey("collection_slot", base::Value(current_collected_slot_));

  std::string result;
  base::JSONWriter::Write(root, &result);

  return result;
}

int BraveOperationalProfiling::GetCurrentCollectionSlot() const {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  return (now.hour * 60 + now.minute) / collection_slot_size_;
}

std::string BraveOperationalProfiling::GetPlatformIdentifier() {
  return brave_stats::GetPlatformIdentifier();
}

void BraveOperationalProfiling::reuseOrRefreshEphemeralId() {
  const base::Time now = base::Time::Now();
  if (ephemeral_id_.empty() ||
      (!ephemeral_id_expiration_.is_null() && now > ephemeral_id_expiration_)) {
    ephemeral_id_ =
        base::ToUpperASCII(base::UnguessableToken::Create().ToString());
    ephemeral_id_expiration_ = now + base::TimeDelta::FromSeconds(
                                         ephemeral_id_lifetime_ * 24 * 60 * 60);
    SavePrefs();
  }
}

}  // namespace brave
