// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_extension_system_factory.h"

#include "apps/shell/browser/shell_extension_system.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"

using content::BrowserContext;

namespace extensions {

ExtensionSystem* ShellExtensionSystemFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ShellExtensionSystem*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ShellExtensionSystemFactory* ShellExtensionSystemFactory::GetInstance() {
  return Singleton<ShellExtensionSystemFactory>::get();
}

ShellExtensionSystemFactory::ShellExtensionSystemFactory()
    : ExtensionSystemProvider("ShellExtensionSystem",
                              BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

ShellExtensionSystemFactory::~ShellExtensionSystemFactory() {}

BrowserContextKeyedService*
ShellExtensionSystemFactory::BuildServiceInstanceFor(BrowserContext* context)
    const {
  return new ShellExtensionSystem(context);
}

BrowserContext* ShellExtensionSystemFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Use a separate instance for incognito.
  return context;
}

bool ShellExtensionSystemFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
