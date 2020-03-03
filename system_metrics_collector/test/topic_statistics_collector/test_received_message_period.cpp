// Copyright 2020 Amazon.com, Inc. or its affiliates. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>

#include "moving_average_statistics/types.hpp"
#include "topic_statistics_collector/received_message_period.hpp"

#include "rclcpp/clock.hpp"
#include "rcl/time.h"


namespace
{
constexpr const std::chrono::seconds kDefaultDurationSeconds{1};
constexpr const int kDefaultMessage{42};
constexpr const double kExpectedAverageMilliseconds{1000.0};
constexpr const double kExpectedMinMilliseconds{1000.0};
constexpr const double kExpectedMaxMilliseconds{1000.0};
constexpr const double kExpectedStandardDeviation{0.0};
const rclcpp::Time kDefaultSteadyTime{0, 0, RCL_STEADY_TIME};
const rclcpp::Time kDefaultROSTime{0, 0, RCL_ROS_TIME};
const rclcpp::Time kDefaultSysTime{0, 0, RCL_SYSTEM_TIME};
}  // namespace

/**
 * Class used to provide a mock time when testing received message period measurements.
 */
class TestReceivedMessagePeriodCollector
  : public topic_statistics_collector::ReceivedMessagePeriodCollector<int>
{
public:
  /**
   * Construct a TestReceivedMessagePeriod object. Initializes the
   * fake_now_nanos_ member.
=  */
  TestReceivedMessagePeriodCollector()
  {
    fake_now_nanos_ = ReceivedMessagePeriodCollector::GetCurrentTime().nanoseconds();
  }
  virtual ~TestReceivedMessagePeriodCollector() = default;

  /**
   * Overridden in order to mock the clock for measurement testing.
   * @return
   */
  rclcpp::Time GetCurrentTime() override
  {
    return rclcpp::Time{fake_now_nanos_, RCL_STEADY_TIME};
  }

  /**
   * Advance time by a specified duration, in seconds.
   * @param seconds duration which to advance time
   */
  void AdvanceTime(std::chrono::seconds seconds)
  {
    fake_now_nanos_ += std::chrono::duration_cast<std::chrono::nanoseconds>(seconds).count();
  }

  int64_t fake_now_nanos_;
};

TEST(ReceivedMessagePeriodTest, GetCurrentTimeDefaultClock) {
  topic_statistics_collector::ReceivedMessagePeriodCollector<int> test{};
  auto now = test.GetCurrentTime();
  EXPECT_NE(kDefaultSteadyTime, now);
}

TEST(ReceivedMessagePeriodTest, GetCurrentTimeCustomClock) {
  const rclcpp::Clock asdf{RCL_ROS_TIME};
  topic_statistics_collector::ReceivedMessagePeriodCollector<int> test{asdf};
  auto now = test.GetCurrentTime();
  EXPECT_NE(kDefaultROSTime, now);
}

TEST(ReceivedMessagePeriodTest, TestPeriodMeasurement) {
  TestReceivedMessagePeriodCollector test{};
  EXPECT_NE(kDefaultSteadyTime, test.GetCurrentTime());

  EXPECT_FALSE(test.IsStarted()) << "Expected to be not started after constructed";

  EXPECT_TRUE(test.Start()) << "Expected Start() to be successful";
  EXPECT_TRUE(test.IsStarted()) << "Expected to be started";

  test.OnMessageReceived(kDefaultMessage);
  auto stats = test.GetStatisticsResults();
  EXPECT_EQ(0, stats.sample_count) << "Expected 0 samples to be collected";

  test.AdvanceTime(kDefaultDurationSeconds);
  test.OnMessageReceived(kDefaultMessage);
  stats = test.GetStatisticsResults();
  EXPECT_EQ(1, stats.sample_count) << "Expected 1 sample to be collected";

  test.AdvanceTime(kDefaultDurationSeconds);
  test.OnMessageReceived(kDefaultMessage);
  stats = test.GetStatisticsResults();
  EXPECT_EQ(2, stats.sample_count) << "Expected 2 samples to be collected";

  test.AdvanceTime(kDefaultDurationSeconds);
  test.OnMessageReceived(kDefaultMessage);
  stats = test.GetStatisticsResults();
  EXPECT_EQ(3, stats.sample_count);
  EXPECT_EQ(kExpectedAverageMilliseconds, stats.average);
  EXPECT_EQ(kExpectedMinMilliseconds, stats.min);
  EXPECT_EQ(kExpectedMaxMilliseconds, stats.max);
  EXPECT_EQ(kExpectedStandardDeviation, stats.standard_deviation);
}