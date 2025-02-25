/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/frequency_capping/permission_rules/inline_content_ads_per_day_frequency_cap.h"

#include <vector>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "bat/ads/internal/features/ad_serving/ad_serving_features.h"
#include "bat/ads/internal/frequency_capping/frequency_capping_unittest_util.h"
#include "bat/ads/internal/unittest_base.h"
#include "bat/ads/internal/unittest_util.h"

// npm run test -- brave_unit_tests --filter=BatAds*

namespace ads {

class BatAdsInlineContentAdsPerDayFrequencyCapTest : public UnitTestBase {
 protected:
  BatAdsInlineContentAdsPerDayFrequencyCapTest() = default;

  ~BatAdsInlineContentAdsPerDayFrequencyCapTest() override = default;

  void SetUp() override {
    UnitTestBase::SetUp();

    const std::vector<base::test::ScopedFeatureList::FeatureAndParams>
        enabled_features;

    const std::vector<base::Feature> disabled_features;

    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeaturesAndParameters(enabled_features,
                                                      disabled_features);
  }
};

TEST_F(BatAdsInlineContentAdsPerDayFrequencyCapTest,
       AllowAdIfThereIsNoAdsHistory) {
  // Arrange

  // Act
  InlineContentAdsPerDayFrequencyCap frequency_cap;
  const bool is_allowed = frequency_cap.ShouldAllow();

  // Assert
  EXPECT_TRUE(is_allowed);
}

TEST_F(BatAdsInlineContentAdsPerDayFrequencyCapTest,
       AllowAdIfDoesNotExceedCap) {
  // Arrange
  const int count = features::GetMaximumInlineContentAdsPerDay() - 1;
  RecordAdEvents(AdType::kInlineContentAd, ConfirmationType::kServed, count);

  // Act
  InlineContentAdsPerDayFrequencyCap frequency_cap;
  const bool is_allowed = frequency_cap.ShouldAllow();

  // Assert
  EXPECT_TRUE(is_allowed);
}

TEST_F(BatAdsInlineContentAdsPerDayFrequencyCapTest,
       AllowAdIfDoesNotExceedCapAfter1Day) {
  // Arrange
  const int count = features::GetMaximumInlineContentAdsPerDay();
  RecordAdEvents(AdType::kInlineContentAd, ConfirmationType::kServed, count);

  FastForwardClockBy(base::TimeDelta::FromDays(1));

  // Act
  InlineContentAdsPerDayFrequencyCap frequency_cap;
  const bool is_allowed = frequency_cap.ShouldAllow();

  // Assert
  EXPECT_TRUE(is_allowed);
}

TEST_F(BatAdsInlineContentAdsPerDayFrequencyCapTest,
       DoNotAllowAdIfExceedsCapWithin1Day) {
  // Arrange
  const int count = features::GetMaximumInlineContentAdsPerDay();
  RecordAdEvents(AdType::kInlineContentAd, ConfirmationType::kServed, count);

  FastForwardClockBy(base::TimeDelta::FromHours(23));

  // Act
  InlineContentAdsPerDayFrequencyCap frequency_cap;
  const bool is_allowed = frequency_cap.ShouldAllow();

  // Assert
  EXPECT_FALSE(is_allowed);
}

}  // namespace ads
