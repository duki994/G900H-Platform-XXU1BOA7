// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_calculator.h"

#include <vector>

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "crypto/hmac.h"

namespace {

// Calculates an HMAC of |message| using |key|, encoded as a hexadecimal string.
std::string GetDigestString(const std::string& key,
                            const std::string& message) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  std::vector<uint8> digest(hmac.DigestLength());
  if (!hmac.Init(key) || !hmac.Sign(message, &digest[0], digest.size())) {
    NOTREACHED();
    return std::string();
  }
  return base::HexEncode(&digest[0], digest.size());
}

// Verifies that |digest_string| is a valid HMAC of |message| using |key|.
// |digest_string| must be encoded as a hexadecimal string.
bool VerifyDigestString(const std::string& key,
                        const std::string& message,
                        const std::string& digest_string) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  std::vector<uint8> digest;
  return base::HexStringToBytes(digest_string, &digest) &&
      hmac.Init(key) &&
      hmac.Verify(message,
                  base::StringPiece(reinterpret_cast<char*>(&digest[0]),
                                    digest.size()));
}

// Renders |value| as a string. |value| may be NULL, in which case the result
// is an empty string. This method can be expensive and its result should be
// re-used rather than recomputed where possible.
std::string ValueAsString(const base::Value* value) {
  // Dictionary values may contain empty lists and sub-dictionaries. Make a
  // deep copy with those removed to make the hash more stable.
  const base::DictionaryValue* dict_value;
  scoped_ptr<base::DictionaryValue> canonical_dict_value;
  if (value && value->GetAsDictionary(&dict_value)) {
    canonical_dict_value.reset(dict_value->DeepCopyWithoutEmptyChildren());
    value = canonical_dict_value.get();
  }

  std::string value_as_string;
  if (value) {
    JSONStringValueSerializer serializer(&value_as_string);
    serializer.Serialize(*value);
  }

  return value_as_string;
}

// Common helper for all hash algorithms.
std::string GetMessageFromValueAndComponents(
    const std::string& value_as_string,
    const std::vector<std::string>& extra_components) {
  return JoinString(extra_components, "") + value_as_string;
}


// Generates a device ID based on the input device ID. The derived device ID has
// no useful properties beyond those of the input device ID except that it is
// consistent with previous implementations.
std::string GenerateDeviceIdLikePrefMetricsServiceDid(
    const std::string& original_device_id) {
  if (original_device_id.empty())
    return std::string();
  return StringToLowerASCII(
      GetDigestString(original_device_id, "PrefMetricsService"));
}

// Verifies a hash using a deprecated hash algorithm. For validating old
// hashes during migration.
bool VerifyLegacyHash(const std::string& seed,
                      const std::string& value_as_string,
                      const std::string& digest_string) {
  return VerifyDigestString(
      seed,
      GetMessageFromValueAndComponents(value_as_string,
                                       std::vector<std::string>()),
      digest_string);
}

}  // namespace

PrefHashCalculator::PrefHashCalculator(const std::string& seed,
                                       const std::string& device_id)
    : seed_(seed),
      device_id_(GenerateDeviceIdLikePrefMetricsServiceDid(device_id)) {}

std::string PrefHashCalculator::Calculate(const std::string& path,
                                          const base::Value* value) const {
  return GetDigestString(seed_, GetMessage(path, ValueAsString(value)));
}

PrefHashCalculator::ValidationResult PrefHashCalculator::Validate(
    const std::string& path,
    const base::Value* value,
    const std::string& digest_string) const {
  const std::string value_as_string(ValueAsString(value));
  if (VerifyDigestString(seed_, GetMessage(path, value_as_string),
                         digest_string)) {
    return VALID;
  }
  if (VerifyLegacyHash(seed_, value_as_string, digest_string))
    return VALID_LEGACY;
  return INVALID;
}

std::string PrefHashCalculator::GetMessage(
    const std::string& path,
    const std::string& value_as_string) const {
  std::vector<std::string> components;
  if (!device_id_.empty())
    components.push_back(device_id_);
  components.push_back(path);
  return GetMessageFromValueAndComponents(value_as_string, components);
}
