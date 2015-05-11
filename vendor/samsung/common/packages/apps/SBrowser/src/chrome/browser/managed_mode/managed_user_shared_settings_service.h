// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SHARED_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SHARED_SETTINGS_SERVICE_H_

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "sync/api/syncable_service.h"

class PrefService;

namespace base {
class DictionaryValue;
class Value;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// ManagedUserSharedSettingsService syncs settings (as key-value pairs) that can
// be modified both by a supervised user and their manager.
// A supervised user can only modify their own settings, whereas a manager can
// modify settings for all their supervised users.
//
// The shared settings are stored in the user preferences in a multi-level
// dictionary. The first level is the MU ID, the second level is the key for the
// setting, and the third level is a dictionary with a "value" key for the value
// and an "acknowledged" flag, which is used to wait for the Sync server to
// acknowledge that it has seen a setting change (see
// ManagedUserSharedSettingsUpdate for how to use this).
class ManagedUserSharedSettingsService : public BrowserContextKeyedService,
                                         public syncer::SyncableService {
 public:
  // Called whenever a setting changes (see Subscribe() below).
  typedef base::Callback<void(const std::string& /* mu_id */,
                              const std::string& /* key */)> ChangeCallback;
  typedef base::CallbackList<
      void(const std::string& /* mu_id */, const std::string& /* key */)>
      ChangeCallbackList;

  // This constructor is public only for testing. Use
  // |ManagedUserSyncServiceFactory::GetForProfile(...)| instead to get an
  // instance of this service in production code.
  explicit ManagedUserSharedSettingsService(PrefService* prefs);
  virtual ~ManagedUserSharedSettingsService();

  // Returns the value for the given |key| and the supervised user identified by
  // |mu_id|. If either the supervised user or the key does not exist, NULL is
  // returned. Note that if the profile that owns this service belongs to a
  // supervised user, callers will only see settings for their own |mu_id|, i.e.
  // a non-matching |mu_id| is treated as non-existent.
  const base::Value* GetValue(const std::string& mu_id, const std::string& key);

  // Sets the value for the given |key| and the supervised user identified by
  // |mu_id|. If the profile that owns this service belongs to a supervised
  // user, |mu_id| must be their own.
  void SetValue(const std::string& mu_id,
                const std::string& key,
                const base::Value& value);

  // Subscribes to changes in the synced settings. The callback will be notified
  // whenever any setting for any supervised user is changed via Sync (but not
  // for local changes). Subscribers should filter the settings and users they
  // are interested in with the |mu_id| and |key| parameters to the callback.
  scoped_ptr<ChangeCallbackList::Subscription> Subscribe(
      const ChangeCallback& cb);

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Public for testing.
  void SetValueInternal(const std::string& mu_id,
                        const std::string& key,
                        const base::Value& value,
                        bool acknowledged);

  // Public for testing.
  static syncer::SyncData CreateSyncDataForSetting(const std::string& mu_id,
                                                   const std::string& key,
                                                   const base::Value& value,
                                                   bool acknowledged);

  // BrowserContextKeyedService implementation:
  virtual void Shutdown() OVERRIDE;

  // SyncableService implementation:
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

 private:
  friend class ManagedUserSharedSettingsUpdate;

  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> error_handler_;

  ChangeCallbackList callbacks_;

  PrefService* prefs_;
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_SHARED_SETTINGS_SERVICE_H_
