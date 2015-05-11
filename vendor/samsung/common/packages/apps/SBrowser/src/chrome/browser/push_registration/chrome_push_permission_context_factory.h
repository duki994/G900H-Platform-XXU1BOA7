// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PUSH_REGISTRATION_CHROME_PUSH_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_PUSH_REGISTRATION_CHROME_PUSH_PERMISSION_CONTEXT_FACTORY_H_

#if defined(ENABLE_PUSH_API)

#include "base/memory/singleton.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class ChromePushPermissionContext;
class PrefRegistrySyncable;
class Profile;

class ChromePushPermissionContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ChromePushPermissionContext* GetForProfile(Profile* profile);

  static ChromePushPermissionContextFactory* GetInstance();

 private:
  friend struct
      DefaultSingletonTraits<ChromePushPermissionContextFactory>;

  ChromePushPermissionContextFactory();
  virtual ~ChromePushPermissionContextFactory();

  // BrowserContextKeyedBaseFactory methods:
  virtual BrowserContextKeyedService*
      BuildServiceInstanceFor(content::BrowserContext* profile) const OVERRIDE;
  virtual void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ChromePushPermissionContextFactory);
};

#endif  // defined(ENABLE_PUSH_API)

#endif  // CHROME_BROWSER_PUSH_REGISTRATION_CHROME_PUSH_PERMISSION_CONTEXT_FACTORY_H_
