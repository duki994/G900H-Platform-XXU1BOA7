// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_error_factory_mock.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::DictionaryValue;
using base::FundamentalValue;
using base::StringValue;
using base::Value;
using sync_pb::ManagedUserSharedSettingSpecifics;
using syncer::MANAGED_USER_SHARED_SETTINGS;
using syncer::SyncChange;
using syncer::SyncChangeList;
using syncer::SyncChangeProcessor;
using syncer::SyncData;
using syncer::SyncDataList;
using syncer::SyncError;
using syncer::SyncErrorFactory;
using syncer::SyncMergeResult;

namespace {

class MockChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  MockChangeProcessor() {}
  virtual ~MockChangeProcessor() {}

  // SyncChangeProcessor implementation:
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE;

  const syncer::SyncChangeList& changes() const { return change_list_; }

 private:
  syncer::SyncChangeList change_list_;

  DISALLOW_COPY_AND_ASSIGN(MockChangeProcessor);
};

syncer::SyncError MockChangeProcessor::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  change_list_ = change_list;
  return syncer::SyncError();
}

syncer::SyncDataList
MockChangeProcessor::GetAllSyncData(syncer::ModelType type) const {
  return syncer::SyncDataList();
}

class MockSyncErrorFactory : public syncer::SyncErrorFactory {
 public:
  explicit MockSyncErrorFactory(syncer::ModelType type);
  virtual ~MockSyncErrorFactory();

  // SyncErrorFactory implementation:
  virtual syncer::SyncError CreateAndUploadError(
      const tracked_objects::Location& location,
      const std::string& message) OVERRIDE;

 private:
  syncer::ModelType type_;

  DISALLOW_COPY_AND_ASSIGN(MockSyncErrorFactory);
};

MockSyncErrorFactory::MockSyncErrorFactory(syncer::ModelType type)
    : type_(type) {}

MockSyncErrorFactory::~MockSyncErrorFactory() {}

syncer::SyncError MockSyncErrorFactory::CreateAndUploadError(
    const tracked_objects::Location& location,
    const std::string& message) {
  return syncer::SyncError(location, SyncError::DATATYPE_ERROR, message, type_);
}

// Convenience method to allow us to use EXPECT_EQ to compare values.
std::string ToJson(const Value* value) {
  if (!value)
    return std::string();

  std::string json_value;
  base::JSONWriter::Write(value, &json_value);
  return json_value;
}

}  // namespace

class ManagedUserSharedSettingsServiceTest : public ::testing::Test {
 protected:
  typedef base::CallbackList<void(const std::string&, const std::string&)>
      CallbackList;

  ManagedUserSharedSettingsServiceTest()
      : settings_service_(profile_.GetPrefs()) {}
  virtual ~ManagedUserSharedSettingsServiceTest() {}

  void StartSyncing(const syncer::SyncDataList& initial_sync_data) {
    sync_processor_ = new MockChangeProcessor();
    scoped_ptr<syncer::SyncErrorFactory> error_handler(
        new MockSyncErrorFactory(MANAGED_USER_SHARED_SETTINGS));
    SyncMergeResult result = settings_service_.MergeDataAndStartSyncing(
        MANAGED_USER_SHARED_SETTINGS,
        initial_sync_data,
        scoped_ptr<SyncChangeProcessor>(sync_processor_),
        error_handler.Pass());
    EXPECT_FALSE(result.error().IsSet());
  }

  const base::DictionaryValue* GetAllSettings() {
    return profile_.GetPrefs()->GetDictionary(
        prefs::kManagedUserSharedSettings);
  }

  void VerifySyncChanges() {
    const SyncChangeList& changes = sync_processor_->changes();
    for (SyncChangeList::const_iterator it = changes.begin();
         it != changes.end();
         ++it) {
      const sync_pb::ManagedUserSharedSettingSpecifics& setting =
          it->sync_data().GetSpecifics().managed_user_shared_setting();
      EXPECT_EQ(
          setting.value(),
          ToJson(settings_service_.GetValue(setting.mu_id(), setting.key())));
    }
  }

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    subscription_ = settings_service_.Subscribe(
        base::Bind(&ManagedUserSharedSettingsServiceTest::OnSettingChanged,
                   base::Unretained(this)));
  }

  virtual void TearDown() OVERRIDE { settings_service_.Shutdown(); }

  void OnSettingChanged(const std::string& mu_id, const std::string& key) {
    const Value* value = settings_service_.GetValue(mu_id, key);
    ASSERT_TRUE(value);
    changed_settings_.push_back(
        ManagedUserSharedSettingsService::CreateSyncDataForSetting(
            mu_id, key, *value, true));
  }

  TestingProfile profile_;
  ManagedUserSharedSettingsService settings_service_;
  SyncDataList changed_settings_;

  scoped_ptr<CallbackList::Subscription> subscription_;

  // Owned by the ManagedUserSettingsService.
  MockChangeProcessor* sync_processor_;
};

TEST_F(ManagedUserSharedSettingsServiceTest, Empty) {
  StartSyncing(SyncDataList());
  EXPECT_EQ(0u, sync_processor_->changes().size());
  EXPECT_EQ(0u, changed_settings_.size());
  EXPECT_EQ(
      0u,
      settings_service_.GetAllSyncData(MANAGED_USER_SHARED_SETTINGS).size());
  EXPECT_EQ(0u, GetAllSettings()->size());
}

TEST_F(ManagedUserSharedSettingsServiceTest, SetAndGet) {
  StartSyncing(SyncDataList());

  const char kIdA[] = "aaaaaa";
  const char kIdB[] = "bbbbbb";
  const char kIdC[] = "cccccc";

  StringValue name("Jack");
  FundamentalValue age(8);
  StringValue bar("bar");
  settings_service_.SetValue(kIdA, "name", name);
  ASSERT_EQ(1u, sync_processor_->changes().size());
  VerifySyncChanges();
  settings_service_.SetValue(kIdA, "age", FundamentalValue(6));
  ASSERT_EQ(1u, sync_processor_->changes().size());
  VerifySyncChanges();
  settings_service_.SetValue(kIdA, "age", age);
  ASSERT_EQ(1u, sync_processor_->changes().size());
  VerifySyncChanges();
  settings_service_.SetValue(kIdB, "foo", bar);
  ASSERT_EQ(1u, sync_processor_->changes().size());
  VerifySyncChanges();

  EXPECT_EQ(
      3u,
      settings_service_.GetAllSyncData(MANAGED_USER_SHARED_SETTINGS).size());

  EXPECT_EQ(ToJson(&name), ToJson(settings_service_.GetValue(kIdA, "name")));
  EXPECT_EQ(ToJson(&age), ToJson(settings_service_.GetValue(kIdA, "age")));
  EXPECT_EQ(ToJson(&bar), ToJson(settings_service_.GetValue(kIdB, "foo")));
  EXPECT_FALSE(settings_service_.GetValue(kIdA, "foo"));
  EXPECT_FALSE(settings_service_.GetValue(kIdB, "name"));
  EXPECT_FALSE(settings_service_.GetValue(kIdC, "name"));
}

TEST_F(ManagedUserSharedSettingsServiceTest, Merge) {
  // Set initial values, then stop syncing so we can restart.
  StartSyncing(SyncDataList());

  const char kIdA[] = "aaaaaa";
  const char kIdB[] = "bbbbbb";
  const char kIdC[] = "cccccc";

  FundamentalValue age(8);
  StringValue bar("bar");
  settings_service_.SetValue(kIdA, "name", StringValue("Jack"));
  settings_service_.SetValue(kIdA, "age", age);
  settings_service_.SetValue(kIdB, "foo", bar);

  settings_service_.StopSyncing(MANAGED_USER_SHARED_SETTINGS);

  StringValue name("Jill");
  StringValue blurp("blurp");
  SyncDataList sync_data;
  sync_data.push_back(
      ManagedUserSharedSettingsService::CreateSyncDataForSetting(
          kIdA, "name", name, true));
  sync_data.push_back(
      ManagedUserSharedSettingsService::CreateSyncDataForSetting(
          kIdC, "baz", blurp, true));

  StartSyncing(sync_data);
  EXPECT_EQ(2u, sync_processor_->changes().size());
  VerifySyncChanges();
  EXPECT_EQ(2u, changed_settings_.size());

  EXPECT_EQ(
      4u,
      settings_service_.GetAllSyncData(MANAGED_USER_SHARED_SETTINGS).size());
  EXPECT_EQ(ToJson(&name),
            ToJson(settings_service_.GetValue(kIdA, "name")));
  EXPECT_EQ(ToJson(&age), ToJson(settings_service_.GetValue(kIdA, "age")));
  EXPECT_EQ(ToJson(&bar), ToJson(settings_service_.GetValue(kIdB, "foo")));
  EXPECT_EQ(ToJson(&blurp), ToJson(settings_service_.GetValue(kIdC, "baz")));
  EXPECT_FALSE(settings_service_.GetValue(kIdA, "foo"));
  EXPECT_FALSE(settings_service_.GetValue(kIdB, "name"));
  EXPECT_FALSE(settings_service_.GetValue(kIdC, "name"));
}

TEST_F(ManagedUserSharedSettingsServiceTest, ProcessChanges) {
  StartSyncing(SyncDataList());

  const char kIdA[] = "aaaaaa";
  const char kIdB[] = "bbbbbb";
  const char kIdC[] = "cccccc";

  FundamentalValue age(8);
  StringValue bar("bar");
  settings_service_.SetValue(kIdA, "name", StringValue("Jack"));
  settings_service_.SetValue(kIdA, "age", age);
  settings_service_.SetValue(kIdB, "foo", bar);

  StringValue name("Jill");
  StringValue blurp("blurp");
  SyncChangeList changes;
  changes.push_back(
      SyncChange(FROM_HERE,
                 SyncChange::ACTION_UPDATE,
                 ManagedUserSharedSettingsService::CreateSyncDataForSetting(
                     kIdA, "name", name, true)));
  changes.push_back(
      SyncChange(FROM_HERE,
                 SyncChange::ACTION_ADD,
                 ManagedUserSharedSettingsService::CreateSyncDataForSetting(
                     kIdC, "baz", blurp, true)));
  SyncError error = settings_service_.ProcessSyncChanges(FROM_HERE, changes);
  EXPECT_FALSE(error.IsSet()) << error.ToString();
  EXPECT_EQ(2u, changed_settings_.size());

  EXPECT_EQ(
      4u,
      settings_service_.GetAllSyncData(MANAGED_USER_SHARED_SETTINGS).size());
  EXPECT_EQ(ToJson(&name),
            ToJson(settings_service_.GetValue(kIdA, "name")));
  EXPECT_EQ(ToJson(&age), ToJson(settings_service_.GetValue(kIdA, "age")));
  EXPECT_EQ(ToJson(&bar), ToJson(settings_service_.GetValue(kIdB, "foo")));
  EXPECT_EQ(ToJson(&blurp), ToJson(settings_service_.GetValue(kIdC, "baz")));
  EXPECT_FALSE(settings_service_.GetValue(kIdA, "foo"));
  EXPECT_FALSE(settings_service_.GetValue(kIdB, "name"));
  EXPECT_FALSE(settings_service_.GetValue(kIdC, "name"));
}
