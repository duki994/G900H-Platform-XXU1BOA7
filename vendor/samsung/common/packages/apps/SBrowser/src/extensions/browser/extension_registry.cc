// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registry.h"

#include "base/strings/string_util.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {

ExtensionRegistry::ExtensionRegistry() {}
ExtensionRegistry::~ExtensionRegistry() {}

// static
ExtensionRegistry* ExtensionRegistry::Get(content::BrowserContext* context) {
  return ExtensionRegistryFactory::GetForBrowserContext(context);
}

void ExtensionRegistry::AddObserver(ExtensionRegistryObserver* observer) {
  observers_.AddObserver(observer);
}

void ExtensionRegistry::RemoveObserver(ExtensionRegistryObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionRegistry::TriggerOnUnloaded(const Extension* extension) {
  DCHECK(!enabled_extensions_.Contains(extension->id()));
  FOR_EACH_OBSERVER(
      ExtensionRegistryObserver, observers_, OnExtensionUnloaded(extension));
}

const Extension* ExtensionRegistry::GetExtensionById(const std::string& id,
                                                     int include_mask) const {
  std::string lowercase_id = StringToLowerASCII(id);
  if (include_mask & ENABLED) {
    const Extension* extension = enabled_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & DISABLED) {
    const Extension* extension = disabled_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & TERMINATED) {
    const Extension* extension = terminated_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & BLACKLISTED) {
    const Extension* extension = blacklisted_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  return NULL;
}

bool ExtensionRegistry::AddEnabled(
    const scoped_refptr<const Extension>& extension) {
  return enabled_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveEnabled(const std::string& id) {
  return enabled_extensions_.Remove(id);
}

bool ExtensionRegistry::AddDisabled(
    const scoped_refptr<const Extension>& extension) {
  return disabled_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveDisabled(const std::string& id) {
  return disabled_extensions_.Remove(id);
}

bool ExtensionRegistry::AddTerminated(
    const scoped_refptr<const Extension>& extension) {
  return terminated_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveTerminated(const std::string& id) {
  return terminated_extensions_.Remove(id);
}

bool ExtensionRegistry::AddBlacklisted(
    const scoped_refptr<const Extension>& extension) {
  return blacklisted_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveBlacklisted(const std::string& id) {
  return blacklisted_extensions_.Remove(id);
}

void ExtensionRegistry::ClearAll() {
  enabled_extensions_.Clear();
  disabled_extensions_.Clear();
  terminated_extensions_.Clear();
  blacklisted_extensions_.Clear();
}

void ExtensionRegistry::SetDisabledModificationCallback(
    const ExtensionSet::ModificationCallback& callback) {
  disabled_extensions_.set_modification_callback(callback);
}

void ExtensionRegistry::Shutdown() {
  // Release references to all Extension objects in the sets.
  ClearAll();
}

}  // namespace extensions
