// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/extension_uninstaller.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "net/base/url_util.h"
#include "ui/app_list/app_list_folder_item.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"

using extensions::ExtensionRegistry;

namespace {

const extensions::Extension* GetExtension(Profile* profile,
                                          const std::string& extension_id) {
  const ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  const extensions::Extension* extension =
      service->GetInstalledExtension(extension_id);
  return extension;
}

}  // namespace

AppListControllerDelegate::~AppListControllerDelegate() {}

bool AppListControllerDelegate::ForceNativeDesktop() const {
  return false;
}

void AppListControllerDelegate::ViewClosing() {}

void AppListControllerDelegate::OnShowExtensionPrompt() {}
void AppListControllerDelegate::OnCloseExtensionPrompt() {}

std::string AppListControllerDelegate::AppListSourceToString(
    AppListSource source) {
  switch (source) {
    case LAUNCH_FROM_APP_LIST:
      return extension_urls::kLaunchSourceAppList;
    case LAUNCH_FROM_APP_LIST_SEARCH:
      return extension_urls::kLaunchSourceAppListSearch;
    default:
      return std::string();
  }
}

bool AppListControllerDelegate::UserMayModifySettings(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  const extensions::ManagementPolicy* policy =
      extensions::ExtensionSystem::Get(profile)->management_policy();
  return extension &&
         policy->UserMayModifySettings(extension, NULL);
}

bool AppListControllerDelegate::CanDoShowAppInfoFlow() {
  return app_list::switches::IsAppInfoEnabled();
}

void AppListControllerDelegate::DoShowAppInfoFlow(
    Profile* profile,
    const std::string& extension_id) {
  DCHECK(CanDoShowAppInfoFlow());
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(service);
  const extensions::Extension* extension = service->GetInstalledExtension(
      extension_id);
  DCHECK(extension);

  gfx::NativeWindow parent_window = GetAppListWindow();
  if (!parent_window)
    return;

  OnShowExtensionPrompt();
  ShowChromeAppInfoDialog(
      parent_window, profile, extension,
      base::Bind(&AppListControllerDelegate::OnCloseExtensionPrompt,
                 base::Unretained(this)));
}

void AppListControllerDelegate::UninstallApp(Profile* profile,
                                             const std::string& app_id) {
  // ExtensionUninstall deletes itself when done or aborted.
  ExtensionUninstaller* uninstaller =
      new ExtensionUninstaller(profile, app_id, this);
  uninstaller->Run();
}

void AppListControllerDelegate::RemoveAppFromFolder(Profile* profile,
                                                    const std::string& app_id) {
  app_list::AppListModel* model =
      app_list::AppListSyncableServiceFactory::GetForProfile(
          profile)->model();
  app_list::AppListItem* item = model->FindItem(app_id);
  DCHECK(item) << "App not found in model: " << app_id;
  syncer::StringOrdinal position;
  app_list::AppListFolderItem* folder_item =
      model->FindFolderItem(item->folder_id());
  DCHECK(folder_item) << "No folder for item: " << item->ToDebugString();
  // Position the item just after the folder.
  position = folder_item->position().CreateAfter();
  model->MoveItemToFolderAt(item, "", position);
}

bool AppListControllerDelegate::IsAppFromWebStore(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  return extension && extension->from_webstore();
}

void AppListControllerDelegate::ShowAppInWebStore(
    Profile* profile,
    const std::string& app_id,
    bool is_search_result) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  if (!extension)
    return;

  const GURL url = extensions::ManifestURL::GetDetailsURL(extension);
  DCHECK_NE(url, GURL::EmptyGURL());

  const std::string source = AppListSourceToString(
      is_search_result ?
          AppListControllerDelegate::LAUNCH_FROM_APP_LIST_SEARCH :
          AppListControllerDelegate::LAUNCH_FROM_APP_LIST);
  chrome::NavigateParams params(
      profile,
      net::AppendQueryParameter(url,
                                extension_urls::kWebstoreSourceField,
                                source),
      content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

bool AppListControllerDelegate::HasOptionsPage(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  return extensions::util::IsAppLaunchableWithoutEnabling(app_id, profile) &&
         extension &&
         !extensions::ManifestURL::GetOptionsPage(extension).is_empty();
}

void AppListControllerDelegate::ShowOptionsPage(
    Profile* profile,
    const std::string& app_id) {
  const extensions::Extension* extension = GetExtension(profile, app_id);
  if (!extension)
    return;

  chrome::NavigateParams params(
      profile,
      extensions::ManifestURL::GetOptionsPage(extension),
      content::PAGE_TRANSITION_LINK);
  chrome::Navigate(&params);
}

extensions::LaunchType AppListControllerDelegate::GetExtensionLaunchType(
    Profile* profile,
    const std::string& app_id) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  return extensions::GetLaunchType(service->extension_prefs(),
                                   GetExtension(profile, app_id));
}

void AppListControllerDelegate::SetExtensionLaunchType(
    Profile* profile,
    const std::string& extension_id,
    extensions::LaunchType launch_type) {
  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  extensions::SetLaunchType(
      service, extension_id, launch_type);
}

bool AppListControllerDelegate::IsExtensionInstalled(
    Profile* profile, const std::string& app_id) {
  return !!GetExtension(profile, app_id);
}

extensions::InstallTracker* AppListControllerDelegate::GetInstallTrackerFor(
    Profile* profile) {
  if (extensions::ExtensionSystem::Get(profile)->extension_service())
    return extensions::InstallTrackerFactory::GetForProfile(profile);
  return NULL;
}

void AppListControllerDelegate::GetApps(Profile* profile,
                                        extensions::ExtensionSet* out_apps) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile);
  DCHECK(registry);
  out_apps->InsertAll(registry->enabled_extensions());
  out_apps->InsertAll(registry->disabled_extensions());
  out_apps->InsertAll(registry->terminated_extensions());
}
