// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_PUSH_API)

#include "chrome/browser/push_registration/chrome_push_permission_context_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/push_registration/chrome_push_permission_context.h"
#include "chrome/common/pref_names.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/user_prefs/pref_registry_syncable.h"

namespace {

class Service : public BrowserContextKeyedService {
 public:
  explicit Service(Profile* profile) {
    context_ = new ChromePushPermissionContext(profile);
  }

  ChromePushPermissionContext* context() {
    return context_.get();
  }

  virtual void Shutdown() OVERRIDE {
    context()->ShutdownOnUIThread();
  }

 private:
  scoped_refptr<ChromePushPermissionContext> context_;

  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // namespace

ChromePushPermissionContext*
ChromePushPermissionContextFactory::GetForProfile(Profile* profile) {
  return static_cast<Service*>(
      GetInstance()->GetServiceForBrowserContext(profile, true))->context();
}

ChromePushPermissionContextFactory*
ChromePushPermissionContextFactory::GetInstance() {
  return Singleton<ChromePushPermissionContextFactory>::get();
}

ChromePushPermissionContextFactory::
ChromePushPermissionContextFactory()
    : BrowserContextKeyedServiceFactory(
          "ChromePushPermissionContext",
          BrowserContextDependencyManager::GetInstance()) {
}

ChromePushPermissionContextFactory::
~ChromePushPermissionContextFactory() {
}

BrowserContextKeyedService*
ChromePushPermissionContextFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new Service(static_cast<Profile*>(profile));
}

void ChromePushPermissionContextFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kPushEnabled,
      true,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

content::BrowserContext*
ChromePushPermissionContextFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

#endif  // defined(ENABLE_PUSH_API)
