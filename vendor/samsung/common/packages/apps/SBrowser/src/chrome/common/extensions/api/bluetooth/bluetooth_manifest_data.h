// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_MANIFEST_DATA_H_
#define CHROME_COMMON_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_MANIFEST_DATA_H_

#include <vector>

#include "base/strings/string16.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {
class BluetoothManifestPermission;
struct BluetoothPermissionRequest;
}

namespace extensions {

// The parsed form of the "bluetooth" manifest entry.
class BluetoothManifestData : public Extension::ManifestData {
 public:
  explicit BluetoothManifestData(
      scoped_ptr<BluetoothManifestPermission> permission);
  virtual ~BluetoothManifestData();

  // Gets the BluetoothManifestData for |extension|, or NULL if none was
  // specified.
  static BluetoothManifestData* Get(const Extension* extension);

  static bool CheckRequest(const Extension* extension,
                           const BluetoothPermissionRequest& request);

  // Tries to construct the info based on |value|, as it would have appeared in
  // the manifest. Sets |error| and returns an empty scoped_ptr on failure.
  static scoped_ptr<BluetoothManifestData> FromValue(const base::Value& value,
                                                     base::string16* error);

  const BluetoothManifestPermission* permission() const {
    return permission_.get();
  }

 private:
  scoped_ptr<BluetoothManifestPermission> permission_;
};

// Used for checking bluetooth permission.
struct BluetoothPermissionRequest {
  explicit BluetoothPermissionRequest(const std::string& profile_uuid);
  ~BluetoothPermissionRequest();

  std::string profile_uuid;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_MANIFEST_DATA_H_
