// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_RUNTIME_DATA_H_
#define EXTENSIONS_BROWSER_RUNTIME_DATA_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {

class Extension;
class ExtensionRegistry;

// Contains per-extension data that can change during the life of the process,
// but does not persist across restarts. Shared between incognito and regular
// browser contexts. Lives on the UI thread. Must be destroyed before
// ExtensionRegistry.
class RuntimeData : public ExtensionRegistryObserver {
 public:
  // Observes |registry| to clean itself up when extensions change state.
  // |registry| must not be NULL.
  explicit RuntimeData(ExtensionRegistry* registry);
  virtual ~RuntimeData();

  // Whether the persistent background page, if any, is ready. We don't load
  // other components until then. If there is no background page, or if it is
  // non-persistent (lazy), we consider it to be ready.
  bool IsBackgroundPageReady(const Extension* extension) const;
  void SetBackgroundPageReady(const Extension* extension, bool value);

  // Getter and setter for the flag that specifies whether the extension is
  // being upgraded.
  bool IsBeingUpgraded(const Extension* extension) const;
  void SetBeingUpgraded(const Extension* extension, bool value);

  // Getter and setter for the flag that specifies if the extension has used
  // the webrequest API.
  // TODO(mpcomplete): remove. http://crbug.com/100411
  bool HasUsedWebRequest(const Extension* extension) const;
  void SetHasUsedWebRequest(const Extension* extension, bool value);

  // Returns true if the extension is being tracked. Used only for testing.
  bool HasExtensionForTesting(const Extension* extension) const;

  // Erase runtime data for all extensions. Used only for testing. Cannot be
  // named ClearAllForTesting due to false-positive presubmit errors.
  void ClearAll();

  // ExtensionRegistryObserver overrides. Public for testing.
  virtual void OnExtensionUnloaded(const Extension* extension) OVERRIDE;

 private:
  // Bitmasks for runtime states.
  enum RuntimeFlag {
    // Set if the background page is ready.
    BACKGROUND_PAGE_READY = 1 << 0,
    // Set while the extension is being upgraded.
    BEING_UPGRADED        = 1 << 1,
    // Set if the extension has used the webRequest API.
    HAS_USED_WEBREQUEST   = 1 << 2,
  };

  // Returns the setting for the flag or false if the extension isn't found.
  bool HasFlag(const Extension* extension, RuntimeFlag flag) const;

  // Sets |flag| for |extension| to |value|. Adds |extension| to the list of
  // extensions if it isn't present.
  void SetFlag(const Extension* extension, RuntimeFlag flag, bool value);

  // Map from extension ID to the RuntimeFlags bits.
  typedef std::map<std::string, int> ExtensionFlagsMap;
  ExtensionFlagsMap extension_flags_;

  ExtensionRegistry* registry_;  // Not owned.
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_RUNTIME_DATA_H_
