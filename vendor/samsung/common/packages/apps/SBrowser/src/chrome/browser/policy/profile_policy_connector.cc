// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/profile_policy_connector.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/forwarding_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account_policy_provider.h"
#include "chrome/browser/chromeos/policy/login_profile_policy_provider.h"
#endif

namespace policy {

ProfilePolicyConnector::ProfilePolicyConnector()
#if defined(OS_CHROMEOS)
    : is_primary_user_(false),
      user_cloud_policy_manager_(NULL)
#else
    : user_cloud_policy_manager_(NULL)
#endif
      {}

ProfilePolicyConnector::~ProfilePolicyConnector() {}

void ProfilePolicyConnector::Init(
    bool force_immediate_load,
#if defined(OS_CHROMEOS)
    const chromeos::User* user,
#endif
    SchemaRegistry* schema_registry,
    CloudPolicyManager* user_cloud_policy_manager) {
  user_cloud_policy_manager_ = user_cloud_policy_manager;

  // |providers| contains a list of the policy providers available for the
  // PolicyService of this connector, in decreasing order of priority.
  //
  // Note: all the providers appended to this vector must eventually become
  // initialized for every policy domain, otherwise some subsystems will never
  // use the policies exposed by the PolicyService!
  // The default ConfigurationPolicyProvider::IsInitializationComplete()
  // result is true, so take care if a provider overrides that.
  std::vector<ConfigurationPolicyProvider*> providers;

#if defined(OS_CHROMEOS)
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
#else
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
#endif

  if (connector->GetPlatformProvider()) {
    forwarding_policy_provider_.reset(
        new ForwardingPolicyProvider(connector->GetPlatformProvider()));
    forwarding_policy_provider_->Init(schema_registry);
    providers.push_back(forwarding_policy_provider_.get());
  }

#if defined(OS_CHROMEOS)
  if (connector->GetDeviceCloudPolicyManager())
    providers.push_back(connector->GetDeviceCloudPolicyManager());
#endif

  if (user_cloud_policy_manager)
    providers.push_back(user_cloud_policy_manager);

#if defined(OS_CHROMEOS)
  if (!user) {
    DCHECK(schema_registry);
    // This case occurs for the signin profile.
    special_user_policy_provider_.reset(
        new LoginProfilePolicyProvider(connector->GetPolicyService()));
    special_user_policy_provider_->Init(schema_registry);
  } else {
    // |user| should never be NULL except for the signin profile.
    is_primary_user_ = user == chromeos::UserManager::Get()->GetPrimaryUser();
    if (user->GetType() == chromeos::User::USER_TYPE_PUBLIC_ACCOUNT) {
      InitializeDeviceLocalAccountPolicyProvider(user->email(),
                                                 schema_registry);
    }
  }
  if (special_user_policy_provider_)
    providers.push_back(special_user_policy_provider_.get());
#endif

  policy_service_.reset(new PolicyServiceImpl(providers));

#if defined(OS_CHROMEOS)
  if (is_primary_user_) {
    if (user_cloud_policy_manager)
      connector->SetUserPolicyDelegate(user_cloud_policy_manager);
    else if (special_user_policy_provider_)
      connector->SetUserPolicyDelegate(special_user_policy_provider_.get());
  }
#endif
}

void ProfilePolicyConnector::InitForTesting(scoped_ptr<PolicyService> service) {
  policy_service_ = service.Pass();
}

void ProfilePolicyConnector::Shutdown() {
#if defined(OS_CHROMEOS)
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (is_primary_user_)
    connector->SetUserPolicyDelegate(NULL);
  if (special_user_policy_provider_)
    special_user_policy_provider_->Shutdown();
#endif
  if (forwarding_policy_provider_)
    forwarding_policy_provider_->Shutdown();
}

bool ProfilePolicyConnector::IsManaged() const {
  return !GetManagementDomain().empty();
}

std::string ProfilePolicyConnector::GetManagementDomain() const {
  if (!user_cloud_policy_manager_)
    return "";
  CloudPolicyStore* store = user_cloud_policy_manager_->core()->store();
  if (store && store->is_managed() && store->policy()->has_username())
    return gaia::ExtractDomainName(store->policy()->username());
  return "";
}

#if defined(OS_CHROMEOS)
void ProfilePolicyConnector::InitializeDeviceLocalAccountPolicyProvider(
    const std::string& username,
    SchemaRegistry* schema_registry) {
  BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DeviceLocalAccountPolicyService* device_local_account_policy_service =
      connector->GetDeviceLocalAccountPolicyService();
  if (!device_local_account_policy_service)
    return;
  special_user_policy_provider_.reset(new DeviceLocalAccountPolicyProvider(
      username, device_local_account_policy_service));
  special_user_policy_provider_->Init(schema_registry);
}
#endif

}  // namespace policy
