// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_util.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/sync_helper.h"
#include "content/public/browser/site_instance.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_handlers/incognito_info.h"

namespace extensions {
namespace util {

bool IsIncognitoEnabled(const std::string& extension_id,
                        content::BrowserContext* context) {
  const Extension* extension = ExtensionRegistry::Get(context)->
      GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
  if (extension) {
    if (!extension->can_be_incognito_enabled())
      return false;
    // If this is an existing component extension we always allow it to
    // work in incognito mode.
    if (extension->location() == Manifest::COMPONENT)
      return true;
    if (extension->force_incognito_enabled())
      return true;
  }

  return ExtensionPrefs::Get(context)->IsIncognitoEnabled(extension_id);
}

void SetIsIncognitoEnabled(const std::string& extension_id,
                           content::BrowserContext* context,
                           bool enabled) {
  ExtensionService* service =
      ExtensionSystem::Get(context)->extension_service();
  CHECK(service);
  const Extension* extension = service->GetInstalledExtension(extension_id);

  if (extension) {
    if (!extension->can_be_incognito_enabled())
      return;

    if (extension->location() == Manifest::COMPONENT) {
      // This shouldn't be called for component extensions unless it is called
      // by sync, for syncable component extensions.
      // See http://crbug.com/112290 and associated CLs for the sordid history.
      DCHECK(sync_helper::IsSyncable(extension));

      // If we are here, make sure the we aren't trying to change the value.
      DCHECK_EQ(enabled, IsIncognitoEnabled(extension_id, service->profile()));
      return;
    }
  }

  ExtensionPrefs* extension_prefs = service->extension_prefs();
  // Broadcast unloaded and loaded events to update browser state. Only bother
  // if the value changed and the extension is actually enabled, since there is
  // no UI otherwise.
  bool old_enabled = extension_prefs->IsIncognitoEnabled(extension_id);
  if (enabled == old_enabled)
    return;

  extension_prefs->SetIsIncognitoEnabled(extension_id, enabled);

  bool extension_is_enabled = service->extensions()->Contains(extension_id);

  // When we reload the extension the ID may be invalidated if we've passed it
  // by const ref everywhere. Make a copy to be safe.
  std::string id = extension_id;
  if (extension_is_enabled)
    service->ReloadExtension(id);

  // Reloading the extension invalidates the |extension| pointer.
  extension = service->GetInstalledExtension(id);
  if (extension) {
    ExtensionSyncService::Get(service->profile())->
        SyncExtensionChangeIfNeeded(*extension);
  }
}

bool CanCrossIncognito(const Extension* extension,
                       content::BrowserContext* context) {
  // We allow the extension to see events and data from another profile iff it
  // uses "spanning" behavior and it has incognito access. "split" mode
  // extensions only see events for a matching profile.
  CHECK(extension);
  return IsIncognitoEnabled(extension->id(), context) &&
         !IncognitoInfo::IsSplitMode(extension);
}

bool CanLoadInIncognito(const Extension* extension,
                        content::BrowserContext* context) {
  CHECK(extension);
  if (extension->is_hosted_app())
    return true;
  // Packaged apps and regular extensions need to be enabled specifically for
  // incognito (and split mode should be set).
  return IncognitoInfo::IsSplitMode(extension) &&
         IsIncognitoEnabled(extension->id(), context);
}

bool AllowFileAccess(const std::string& extension_id,
                     content::BrowserContext* context) {
  return CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableExtensionsFileAccessCheck) ||
         ExtensionPrefs::Get(context)->AllowFileAccess(extension_id);
}

void SetAllowFileAccess(const std::string& extension_id,
                        content::BrowserContext* context,
                        bool allow) {
  ExtensionService* service =
      ExtensionSystem::Get(context)->extension_service();
  CHECK(service);

  // Reload to update browser state. Only bother if the value changed and the
  // extension is actually enabled, since there is no UI otherwise.
  if (allow == AllowFileAccess(extension_id, context))
    return;

  service->extension_prefs()->SetAllowFileAccess(extension_id, allow);

  bool extension_is_enabled = service->extensions()->Contains(extension_id);
  if (extension_is_enabled)
    service->ReloadExtension(extension_id);
}

bool IsAppLaunchable(const std::string& extension_id,
                     content::BrowserContext* context) {
  return !(ExtensionPrefs::Get(context)->GetDisableReasons(extension_id) &
           Extension::DISABLE_UNSUPPORTED_REQUIREMENT);
}

bool IsAppLaunchableWithoutEnabling(const std::string& extension_id,
                                    content::BrowserContext* context) {
  return ExtensionRegistry::Get(context)->GetExtensionById(
      extension_id, ExtensionRegistry::ENABLED) != NULL;
}

bool IsExtensionIdle(const std::string& extension_id,
                     content::BrowserContext* context) {
  ProcessManager* process_manager =
      ExtensionSystem::Get(context)->process_manager();
  DCHECK(process_manager);
  ExtensionHost* host =
      process_manager->GetBackgroundHostForExtension(extension_id);
  if (host)
    return false;

  content::SiteInstance* site_instance = process_manager->GetSiteInstanceForURL(
      Extension::GetBaseURLFromExtensionId(extension_id));
  if (site_instance && site_instance->HasProcess())
    return false;

  return process_manager->GetRenderViewHostsForExtension(extension_id).empty();
}

bool IsExtensionInstalledPermanently(const std::string& extension_id,
                                     content::BrowserContext* context) {
  const Extension* extension = ExtensionRegistry::Get(context)->
      GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
  return extension && !extension->is_ephemeral();
}

}  // namespace util
}  // namespace extensions
