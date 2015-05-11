// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_MANIFEST_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_MANIFEST_PERMISSION_H_

#include <set>
#include <vector>

#include "extensions/common/install_warning.h"
#include "extensions/common/permissions/manifest_permission.h"

namespace extensions {
class Extension;
}

namespace extensions {
struct BluetoothPermissionRequest;
}

namespace extensions {

class BluetoothManifestPermission : public ManifestPermission {
 public:
  typedef std::set<std::string> BluetoothProfileUuidSet;
  BluetoothManifestPermission();
  virtual ~BluetoothManifestPermission();

  // Tries to construct the info based on |value|, as it would have appeared in
  // the manifest. Sets |error| and returns an empty scoped_ptr on failure.
  static scoped_ptr<BluetoothManifestPermission> FromValue(
      const base::Value& value,
      base::string16* error);

  bool CheckRequest(const Extension* extension,
                    const BluetoothPermissionRequest& request) const;

  void AddPermission(const std::string& profile_uuid);

  // extensions::ManifestPermission overrides.
  virtual std::string name() const OVERRIDE;
  virtual std::string id() const OVERRIDE;
  virtual bool HasMessages() const OVERRIDE;
  virtual PermissionMessages GetMessages() const OVERRIDE;
  virtual bool FromValue(const base::Value* value) OVERRIDE;
  virtual scoped_ptr<base::Value> ToValue() const OVERRIDE;
  virtual ManifestPermission* Clone() const OVERRIDE;
  virtual ManifestPermission* Diff(const ManifestPermission* rhs)
      const OVERRIDE;
  virtual ManifestPermission* Union(const ManifestPermission* rhs)
      const OVERRIDE;
  virtual ManifestPermission* Intersect(const ManifestPermission* rhs)
      const OVERRIDE;
  virtual bool Contains(const ManifestPermission* rhs) const OVERRIDE;
  virtual bool Equal(const ManifestPermission* rhs) const OVERRIDE;
  virtual void Write(IPC::Message* m) const OVERRIDE;
  virtual bool Read(const IPC::Message* m, PickleIterator* iter) OVERRIDE;
  virtual void Log(std::string* log) const OVERRIDE;

  const BluetoothProfileUuidSet& profile_uuids() const {
    return profile_uuids_;
  }

 private:
  BluetoothProfileUuidSet profile_uuids_;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_MANIFEST_PERMISSION_H_
