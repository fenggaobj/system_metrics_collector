// Copyright 2019 Amazon.com, Inc. or its affiliates. All Rights Reserved.
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
#include <iostream>
#include <string>
#include <memory>
#include <mutex>

#include "../../src/system_metrics_collector/linux_memory_measurement_node.hpp"

namespace
{
constexpr const char EMPTY_SAMPLE[] = "";
constexpr const char GARBAGE_SAMPLE[] = "this is garbage\n";
constexpr const char INCOMPLETE_SAMPLE[] = "MemTotal:       16302048 kB\n"
  "MemFree:          443300 kB\n";
constexpr const char COMPLETE_SAMPLE[] = "MemTotal:       16302048 kB\n"
  "MemFree:          239124 kB\n"
  "MemAvailable:    9104952 kB\n";
constexpr const char FULL_SAMPLE[] = "MemTotal:       16302048 kB\n"
  "MemFree:          239124 kB\n"
  "MemAvailable:    9104952 kB\n"
  "Buffers:         2755028 kB\n"
  "Cached:          5351344 kB\n"
  "SwapCached:       202440 kB\n"
  "Active:          9743384 kB\n"
  "Inactive:        3662540 kB\n"
  "Active(anon):    5246708 kB\n"
  "Inactive(anon):  1084404 kB\n"
  "Active(file):    4496676 kB\n"
  "Inactive(file):  2578136 kB\n"
  "Unevictable:          68 kB\n"
  "Mlocked:              68 kB\n"
  "SwapTotal:       8003580 kB\n"
  "SwapFree:        6510332 kB\n"
  "Dirty:               436 kB\n"
  "Writeback:             0 kB\n"
  "AnonPages:       5294808 kB\n"
  "Mapped:           823420 kB\n"
  "Shmem:           1037804 kB\n"
  "Slab:            2371932 kB\n"
  "SReclaimable:    2118248 kB\n"
  "SUnreclaim:       253684 kB\n"
  "KernelStack:       21968 kB\n"
  "PageTables:       114360 kB\n"
  "NFS_Unstable:          0 kB\n"
  "Bounce:                0 kB\n"
  "WritebackTmp:          0 kB\n"
  "CommitLimit:    16154604 kB\n"
  "Committed_AS:   19520052 kB\n"
  "VmallocTotal:   34359738367 kB\n"
  "VmallocUsed:           0 kB\n"
  "VmallocChunk:          0 kB\n"
  "HardwareCorrupted:     0 kB\n"
  "AnonHugePages:         0 kB\n"
  "ShmemHugePages:        0 kB\n"
  "ShmemPmdMapped:        0 kB\n"
  "CmaTotal:              0 kB\n"
  "CmaFree:               0 kB\n"
  "HugePages_Total:       0\n"
  "HugePages_Free:        0\n"
  "HugePages_Rsvd:        0\n"
  "HugePages_Surp:        0\n"
  "Hugepagesize:       2048 kB\n"
  "DirectMap4k:     3993192 kB\n"
  "DirectMap2M:    12660736 kB\n"
  "DirectMap1G:     1048576 kB";
constexpr const auto TEST_PERIOD{std::chrono::milliseconds(50)};
constexpr const double MEMORY_USED_PERCENTAGE = 44.148416198995363;
}  // namespace

class TestLinuxMemoryMeasurementNode : public system_metrics_collector::LinuxMemoryMeasurementNode
{
public:
  TestLinuxMemoryMeasurementNode(
    const std::string & name,
    const std::chrono::milliseconds measurement_period,
    const std::string & publishing_topic)
  : LinuxMemoryMeasurementNode(name, measurement_period, publishing_topic,
      PeriodicMeasurementNode::DEFAULT_PUBLISH_WINDOW)
  {}
  virtual ~TestLinuxMemoryMeasurementNode() = default;

  double periodicMeasurement() override
  {
    // override to avoid calling methods involved in file i/o
    return system_metrics_collector::processMemInfoLines(test_string_);
  }

  void setTestString(std::string & test_string)
  {
    test_string_ = test_string;
  }

private:
  std::string test_string_{""};
};

class LinuxMemoryMeasurementTestFixture : public ::testing::Test
{
public:
  void SetUp() override
  {
    rclcpp::init(0, nullptr);

    test_measure_linux_memory = std::make_shared<TestLinuxMemoryMeasurementNode>(
      "test_periodic_node",
      TEST_PERIOD, "test_topic");

    ASSERT_FALSE(test_measure_linux_memory->isStarted());

    const moving_average_statistics::StatisticData data =
      test_measure_linux_memory->getStatisticsResults();
    ASSERT_TRUE(std::isnan(data.average));
    ASSERT_TRUE(std::isnan(data.min));
    ASSERT_TRUE(std::isnan(data.max));
    ASSERT_TRUE(std::isnan(data.standard_deviation));
    ASSERT_EQ(0, data.sample_count);
  }

  void TearDown() override
  {
    test_measure_linux_memory->stop();
    test_measure_linux_memory.reset();
    rclcpp::shutdown();
  }

protected:
  std::shared_ptr<TestLinuxMemoryMeasurementNode> test_measure_linux_memory;
};

TEST_F(LinuxMemoryMeasurementTestFixture, testManualMeasurement) {
  double mem_used_percentage = test_measure_linux_memory->periodicMeasurement();
  ASSERT_TRUE(std::isnan(mem_used_percentage));

  auto s = std::string(FULL_SAMPLE);
  test_measure_linux_memory->setTestString(s);

  mem_used_percentage = test_measure_linux_memory->periodicMeasurement();
  ASSERT_DOUBLE_EQ(MEMORY_USED_PERCENTAGE, mem_used_percentage);
}

TEST(LinuxMemoryMeasurementTest, testReadInvalidFile)
{
  const auto s = system_metrics_collector::readFileToString("this_will_fail.txt");
  ASSERT_EQ("", s);
}

TEST(LinuxMemoryMeasurementTest, testProcessLines)
{
  auto d = system_metrics_collector::processMemInfoLines(EMPTY_SAMPLE);
  ASSERT_TRUE(std::isnan(d));

  d = system_metrics_collector::processMemInfoLines(GARBAGE_SAMPLE);
  ASSERT_TRUE(std::isnan(d));

  d = system_metrics_collector::processMemInfoLines(INCOMPLETE_SAMPLE);
  ASSERT_TRUE(std::isnan(d));

  d = system_metrics_collector::processMemInfoLines(COMPLETE_SAMPLE);
  ASSERT_DOUBLE_EQ(MEMORY_USED_PERCENTAGE, d);

  d = system_metrics_collector::processMemInfoLines(FULL_SAMPLE);
  ASSERT_DOUBLE_EQ(MEMORY_USED_PERCENTAGE, d);
}
