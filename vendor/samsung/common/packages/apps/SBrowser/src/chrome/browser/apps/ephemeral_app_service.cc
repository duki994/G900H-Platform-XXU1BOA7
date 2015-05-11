// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/ephemeral_app_service.h"

#include "base/command_line.h"
#include "chrome/browser/apps/ephemeral_app_service_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionSet;
using extensions::ExtensionSystem;
using extensions::InstalledExtensionInfo;

namespace {

// The number of seconds after startup before performing garbage collection
// of ephemeral apps.
const int kGarbageCollectStartupDelay = 60;

// The number of seconds after an ephemeral app has been installed before
// performing garbage collection.
const int kGarbageCollectInstallDelay = 15;

// When the number of ephemeral apps reaches this count, trigger garbage
// collection to trim off the least-recently used apps in excess of
// kMaxEphemeralAppsCount.
const int kGarbageCollectTriggerCount = 35;

}  // namespace

// The number of days of inactivity before an ephemeral app will be removed.
const int EphemeralAppService::kAppInactiveThreshold = 10;

// If the ephemeral app has been launched within this number of days, it will
// definitely not be garbage collected.
const int EphemeralAppService::kAppKeepThreshold = 1;

// The maximum number of ephemeral apps to keep cached. Excess may be removed.
const int EphemeralAppService::kMaxEphemeralAppsCount = 30;

// static
EphemeralAppService* EphemeralAppService::Get(Profile* profile) {
  return EphemeralAppServiceFactory::GetForProfile(profile);
}

EphemeralAppService::EphemeralAppService(Profile* profile)
    : profile_(profile),
      ephemeral_app_count_(-1) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableEphemeralApps))
    return;

  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_INSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSIONS_READY,
                 content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                 content::Source<Profile>(profile_));
}

EphemeralAppService::~EphemeralAppService() {
}

void EphemeralAppService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSIONS_READY: {
      Init();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_INSTALLED: {
      const Extension* extension =
          content::Details<const InstalledExtensionInfo>(details)->extension;
      DCHECK(extension);
      if (extension->is_ephemeral()) {
        ++ephemeral_app_count_;
        if (ephemeral_app_count_ >= kGarbageCollectTriggerCount)
          TriggerGarbageCollect(
              base::TimeDelta::FromSeconds(kGarbageCollectInstallDelay));
      }
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNINSTALLED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      DCHECK(extension);
      if (extension->is_ephemeral())
        --ephemeral_app_count_;
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      // Ideally we need to know when the extension system is shutting down.
      garbage_collect_timer_.Stop();
      break;
    }
    default:
      NOTREACHED();
  };
}

void EphemeralAppService::Init() {
  InitEphemeralAppCount();
  TriggerGarbageCollect(
      base::TimeDelta::FromSeconds(kGarbageCollectStartupDelay));
}

void EphemeralAppService::InitEphemeralAppCount() {
  ExtensionService* service =
      ExtensionSystem::Get(profile_)->extension_service();
  DCHECK(service);
  scoped_ptr<ExtensionSet> extensions =
      service->GenerateInstalledExtensionsSet();

  ephemeral_app_count_ = 0;
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const Extension* extension = *it;
    if (extension->is_ephemeral())
      ++ephemeral_app_count_;
  }
}

void EphemeralAppService::TriggerGarbageCollect(const base::TimeDelta& delay) {
  if (!garbage_collect_timer_.IsRunning())
    garbage_collect_timer_.Start(
      FROM_HERE,
      delay,
      this,
      &EphemeralAppService::GarbageCollectApps);
}

void EphemeralAppService::GarbageCollectApps() {
  ExtensionSystem* system = ExtensionSystem::Get(profile_);
  DCHECK(system);
  ExtensionService* service = system->extension_service();
  DCHECK(service);
  ExtensionPrefs* prefs = service->extension_prefs();
  scoped_ptr<ExtensionSet> extensions =
      service->GenerateInstalledExtensionsSet();

  int app_count = 0;
  LaunchTimeAppMap app_launch_times;
  std::set<std::string> remove_app_ids;

  // Populate a list of ephemeral apps, ordered by their last launch time.
  for (ExtensionSet::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const Extension* extension = *it;
    if (!extension->is_ephemeral())
      continue;

    ++app_count;

    // Do not remove ephemeral apps that are running.
    if (!extensions::util::IsExtensionIdle(extension->id(), profile_))
      continue;

    base::Time last_launch_time = prefs->GetLastLaunchTime(extension->id());

    // If the last launch time is invalid, this may be because it was just
    // installed. So use the install time. If this is also null for some reason,
    // the app will be removed.
    if (last_launch_time.is_null())
      last_launch_time = prefs->GetInstallTime(extension->id());

    app_launch_times.insert(std::make_pair(last_launch_time, extension->id()));
  }

  // Execute the replacement policies and remove apps marked for deletion.
  if (!app_launch_times.empty()) {
    GetAppsToRemove(app_count, app_launch_times, &remove_app_ids);
    for (std::set<std::string>::const_iterator id = remove_app_ids.begin();
         id != remove_app_ids.end(); ++id) {
      if (service->UninstallExtension(*id, false, NULL))
        --app_count;
    }
  }

  ephemeral_app_count_ = app_count;
}

// static
void EphemeralAppService::GetAppsToRemove(
    int app_count,
    const LaunchTimeAppMap& app_launch_times,
    std::set<std::string>* remove_app_ids) {
  base::Time time_now = base::Time::Now();
  const base::Time inactive_threshold =
      time_now - base::TimeDelta::FromDays(kAppInactiveThreshold);
  const base::Time keep_threshold =
      time_now - base::TimeDelta::FromDays(kAppKeepThreshold);

  // Visit the apps in order of least recently to most recently launched.
  for (LaunchTimeAppMap::const_iterator it = app_launch_times.begin();
       it != app_launch_times.end(); ++it) {
    // Cannot remove apps that have been launched recently. So break when we
    // reach the new apps.
    if (it->first > keep_threshold)
        break;

    // Remove ephemeral apps that have been inactive for a while or if the cache
    // is larger than the desired size.
    if (it->first < inactive_threshold || app_count > kMaxEphemeralAppsCount) {
      remove_app_ids->insert(it->second);
      --app_count;
    } else {
      break;
    }
  }
}
