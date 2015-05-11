// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_HASH_STORE_TRANSACTION_H_
#define CHROME_BROWSER_PREFS_PREF_HASH_STORE_TRANSACTION_H_

#include <string>
#include <vector>

namespace base {
class DictionaryValue;
class Value;
}  // namespace base

// Used to perform a series of checks/transformations on a PrefHashStore.
class PrefHashStoreTransaction {
 public:
  enum ValueState {
    // The preference value corresponds to its stored hash.
    UNCHANGED,
    // The preference has been cleared since the last hash.
    CLEARED,
    // The preference value corresponds to its stored hash, which was calculated
    // using a legacy hash algorithm.
    MIGRATED,
    // The preference value has been changed since the last hash.
    CHANGED,
    // No stored hash exists for the preference value.
    UNTRUSTED_UNKNOWN_VALUE,
    // No stored hash exists for the preference value, but the current set of
    // hashes stored is trusted and thus this value can safely be seeded. This
    // happens when all hashes are already properly seeded and a newly
    // tracked value needs to be seeded). NULL values are inherently trusted as
    // well.
    TRUSTED_UNKNOWN_VALUE,
  };

  // Finalizes any remaining work after the transaction has been performed.
  virtual ~PrefHashStoreTransaction() {}

  // Checks |initial_value| against the existing stored value hash.
  virtual ValueState CheckValue(
      const std::string& path, const base::Value* initial_value) const = 0;

  // Stores a hash of the current |value| of the preference at |path|.
  virtual void StoreHash(const std::string& path,
                         const base::Value* value) = 0;

  // Checks |initial_value| against the existing stored hashes for the split
  // preference at |path|. |initial_split_value| being an empty dictionary or
  // NULL is equivalent. |invalid_keys| must initially be empty. |invalid_keys|
  // will not be modified unless the return value is CHANGED, in which case it
  // will be filled with the keys that are considered invalid (unknown or
  // changed).
  virtual ValueState CheckSplitValue(
      const std::string& path,
      const base::DictionaryValue* initial_split_value,
      std::vector<std::string>* invalid_keys) const = 0;

  // Stores hashes for the |value| of the split preference at |path|.
  // |split_value| being an empty dictionary or NULL is equivalent.
  virtual void StoreSplitHash(
      const std::string& path,
      const base::DictionaryValue* split_value) = 0;
};

#endif  // CHROME_BROWSER_PREFS_PREF_HASH_STORE_TRANSACTION_H_
