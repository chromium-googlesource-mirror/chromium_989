// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/network_configuration_updater.h"

#include "chrome/browser/chromeos/cros/mock_network_library.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Return;
using testing::_;

namespace policy {

static const char kFakeONC[] = "{ \"GUID\": \"1234\" }";

class NetworkConfigurationUpdaterTest
    : public testing::TestWithParam<ConfigurationPolicyType> {
 protected:
  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(network_library_, LoadOncNetworks(_, "", _, _))
        .WillRepeatedly(Return(true));
  }

  // Maps configuration policy type to corresponding ONC source.
  static chromeos::NetworkUIData::ONCSource TypeToONCSource(
      ConfigurationPolicyType type) {
    switch (type) {
      case kPolicyDeviceOpenNetworkConfiguration:
        return chromeos::NetworkUIData::ONC_SOURCE_DEVICE_POLICY;
      case kPolicyOpenNetworkConfiguration:
        return chromeos::NetworkUIData::ONC_SOURCE_USER_POLICY;
      default:
        return chromeos::NetworkUIData::ONC_SOURCE_NONE;
    }
  }

  chromeos::MockNetworkLibrary network_library_;
  MockConfigurationPolicyProvider provider_;
};

TEST_P(NetworkConfigurationUpdaterTest, InitialUpdate) {
  provider_.AddPolicy(GetParam(), Value::CreateStringValue(kFakeONC));

  EXPECT_CALL(network_library_,
              LoadOncNetworks(kFakeONC, "", TypeToONCSource(GetParam()), _))
      .WillOnce(Return(true));

  NetworkConfigurationUpdater updater(&provider_, &network_library_);
  Mock::VerifyAndClearExpectations(&network_library_);
}

TEST_P(NetworkConfigurationUpdaterTest, PolicyChange) {
  NetworkConfigurationUpdater updater(&provider_, &network_library_);

  // We should update if policy changes.
  EXPECT_CALL(network_library_,
              LoadOncNetworks(kFakeONC, "", TypeToONCSource(GetParam()), _))
      .WillOnce(Return(true));
  provider_.AddPolicy(GetParam(), Value::CreateStringValue(kFakeONC));
  provider_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(&network_library_);

  // No update if the set the same value again.
  EXPECT_CALL(network_library_,
              LoadOncNetworks(kFakeONC, "", TypeToONCSource(GetParam()), _))
      .Times(0);
  provider_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(&network_library_);

  // Another update is expected if the policy goes away.
  EXPECT_CALL(network_library_,
              LoadOncNetworks(NetworkConfigurationUpdater::kEmptyConfiguration,
                              "", TypeToONCSource(GetParam()), _))
      .WillOnce(Return(true));
  provider_.RemovePolicy(GetParam());
  provider_.NotifyPolicyUpdated();
  Mock::VerifyAndClearExpectations(&network_library_);
}

INSTANTIATE_TEST_CASE_P(
    NetworkConfigurationUpdaterTestInstance,
    NetworkConfigurationUpdaterTest,
    testing::Values(kPolicyDeviceOpenNetworkConfiguration,
                    kPolicyOpenNetworkConfiguration));

}  // namespace policy
