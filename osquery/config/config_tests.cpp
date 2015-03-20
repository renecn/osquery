/*
 *  Copyright (c) 2014, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant 
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */
#include <vector>

#include <gtest/gtest.h>

#include <osquery/config.h>
#include <osquery/core.h>
#include <osquery/flags.h>
#include <osquery/registry.h>
#include <osquery/sql.h>

#include "osquery/core/test_util.h"

namespace osquery {

// The config_path flag is defined in the filesystem config plugin.
DECLARE_string(config_path);

class ConfigTests : public testing::Test {
 public:
  ConfigTests() {
    Registry::setActive("config", "filesystem");
    FLAGS_config_path = kTestDataPath + "test.config";
  }

 protected:

  void SetUp() {
    createMockFileStructure();
    Registry::setUp();
    Config::load();
  }

  void TearDown() { tearDownMockFileStructure(); }
};

class TestConfigPlugin : public ConfigPlugin {
 public:
  TestConfigPlugin() {}
  Status genConfig(std::map<std::string, std::string>& config) {
    config["data"] = "foobar";
    return Status(0, "OK");
    ;
  }
};

TEST_F(ConfigTests, test_plugin) {
  Registry::add<TestConfigPlugin>("config", "test");

  // Change the active config plugin.
  EXPECT_TRUE(Registry::setActive("config", "test").ok());

  PluginResponse response;
  auto status = Registry::call("config", {{"action", "genConfig"}}, response);

  EXPECT_EQ(status.ok(), true);
  EXPECT_EQ(status.toString(), "OK");
  EXPECT_EQ(response[0].at("data"), "foobar");
}

TEST_F(ConfigTests, test_queries_execute) {
  auto queries = Config::getScheduledQueries();
  EXPECT_EQ(queries.size(), 2);
}

TEST_F(ConfigTests, test_watched_files) {
  auto files = Config::getWatchedFiles();

  EXPECT_EQ(files.size(), 2);
  EXPECT_EQ(files["downloads"].size(), 1);
  EXPECT_EQ(files["system_binaries"].size(), 2);
}

TEST_F(ConfigTests, test_config_update) {
  std::string digest;
  // Get a snapshot of the digest before making config updates.
  auto status = Config::getMD5(digest);
  EXPECT_TRUE(status);

  // Request an update of the 'new_source1'.
  status =
      Config::update({{"1new_source", "{\"options\": {\"new1\": \"value\"}}"}});
  EXPECT_TRUE(status);

  // At least, the amalgamated config digest should have changed.
  std::string new_digest;
  Config::getMD5(new_digest);
  EXPECT_NE(digest, new_digest);

  // Access the option that was added in the update to source 'new_source1'.
  auto config = Config::getEntireConfiguration();
  auto option = config.get<std::string>("options.new1", "");
  EXPECT_EQ(option, "value");

  // Add a lexically larger source that emits the same option 'new1'.
  Config::update({{"2new_source", "{\"options\": {\"new1\": \"changed\"}}"}});
  config = Config::getEntireConfiguration();
  option = config.get<std::string>("options.new1", "");
  // Expect the amalgamation to have overritten 'new_source1'.
  EXPECT_EQ(option, "changed");

  // Again add a source but emit a different option, both 'new1' and 'new2'
  // should be in the amalgamated/merged config.
  Config::update({{"3new_source", "{\"options\": {\"new2\": \"different\"}}"}});
  config = Config::getEntireConfiguration();
  option = config.get<std::string>("options.new1", "");
  EXPECT_EQ(option, "changed");
  option = config.get<std::string>("options.new2", "");
  EXPECT_EQ(option, "different");
}
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
