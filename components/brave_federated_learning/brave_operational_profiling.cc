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
constexpr char kEphemeralIdPrefName[] = "brave.federated.ephemeral_id";
constexpr char kEphemeralIdExpirationPrefName[] =
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
  registry->RegisterStringPref(kEphemeralIdPrefName, {});
  registry->RegisterTimePref(kEphemeralIdExpirationPrefName, base::Time());
}

void BraveOperationalProfiling::Start() {
  DCHECK(!simulate_local_training_step_timer_);
  DCHECK(!collection_slot_periodic_timer_);

  LoadParams();

  simulate_local_training_step_timer_ =
      std::make_unique<base::RetainingOneShotTimer>();
  simulate_local_training_step_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(
          simulated_local_training_step_duration_in_minutes_ * 60),
      this, &BraveOperationalProfiling::OnSimulateLocalTrainingStepTimerFired);

  collection_slot_periodic_timer_ = std::make_unique<base::RepeatingTimer>();
  collection_slot_periodic_timer_->Start(
      FROM_HERE,
      base::TimeDelta::FromSeconds(collection_slot_size_in_minutes_ * 60 / 2),
      this, &BraveOperationalProfiling::OnCollectionSlotStartTimerFired);
}

void BraveOperationalProfiling::LoadParams() {
  collection_slot_size_in_minutes_ =
      operational_profiling::features::GetCollectionSlotSizeValue();
  simulated_local_training_step_duration_in_minutes_ = operational_profiling::
      features::GetSimulateLocalTrainingStepDurationValue();
  ephemeral_id_lifetime_in_days_ =
      operational_profiling::features::GetEphemeralIdLifetime();

  operational_profiling_endpoint_ = GURL(federatedLearningUrl);

  LoadPrefs();
  MaybeResetEphemeralId();
}

void BraveOperationalProfiling::LoadPrefs() {
  platform_ = GetPlatformIdentifier();
  last_checked_slot_ = pref_service_->GetInteger(kLastCheckedSlotPrefName);
  ephemeral_id_ = pref_service_->GetString(kEphemeralIdPrefName);
  ephemeral_id_expiration_time_ =
      pref_service_->GetTime(kEphemeralIdExpirationPrefName);
}

void BraveOperationalProfiling::SavePrefs() {
  pref_service_->SetInteger(kLastCheckedSlotPrefName, last_checked_slot_);
  pref_service_->SetString(kEphemeralIdPrefName, ephemeral_id_);
  pref_service_->SetTime(kEphemeralIdExpirationPrefName,
                         ephemeral_id_expiration_time_);
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

  MaybeResetEphemeralId();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = operational_profiling_endpoint_;
  resource_request->headers.SetHeader("X-Brave-FL-Operational-Profile", "?1");

  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";

  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), GetNetworkTrafficAnnotationTag());
  url_loader_->AttachStringForUpload(BuildPayload(), "application/base64");

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

std::string BraveOperationalProfiling::BuildPayload() const {
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

  return ((now.day_of_month - 1) * 24 * 60 + now.hour * 60 + now.minute) /
         collection_slot_size_in_minutes_;
}

std::string BraveOperationalProfiling::GetPlatformIdentifier() {
  return brave_stats::GetPlatformIdentifier();
}

void BraveOperationalProfiling::MaybeResetEphemeralId() {
  const base::Time now = base::Time::Now();
  if (ephemeral_id_.empty() || (!ephemeral_id_expiration_time_.is_null() &&
                                now > ephemeral_id_expiration_time_)) {
    ephemeral_id_ =
        base::ToUpperASCII(base::UnguessableToken::Create().ToString());
    ephemeral_id_expiration_time_ =
        now + base::TimeDelta::FromSeconds(ephemeral_id_lifetime_in_days_ * 24 *
                                           60 * 60);
    SavePrefs();
  }
}

}  // namespace brave
