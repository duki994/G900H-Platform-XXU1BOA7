// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "content/public/common/content_paths.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "content/renderer/webcrypto/webcrypto_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"
#include "third_party/re2/re2/re2.h"

// The OpenSSL implementation of WebCrypto is less complete, so don't run all of
// the tests: http://crbug.com/267888
#if defined(USE_OPENSSL)
#define MAYBE(test_name) DISABLED_##test_name
#else
#define MAYBE(test_name) test_name
#endif

// Helper macros to verify the value of a Status.
#define EXPECT_STATUS_ERROR(code) EXPECT_FALSE((code).IsSuccess())
#define EXPECT_STATUS(expected, code) \
    EXPECT_EQ(expected.ToString(), (code).ToString())
#define ASSERT_STATUS(expected, code) \
    ASSERT_EQ(expected.ToString(), (code).ToString())
#define EXPECT_STATUS_SUCCESS(code) EXPECT_STATUS(Status::Success(), code)
#define ASSERT_STATUS_SUCCESS(code) ASSERT_STATUS(Status::Success(), code)

namespace content {

using webcrypto::Status;

namespace {

// Returns a slightly modified version of the input vector.
//
//  - For non-empty inputs a single bit is inverted.
//  - For empty inputs, a byte is added.
std::vector<uint8> Corrupted(const std::vector<uint8>& input) {
  std::vector<uint8> corrupted_data(input);
  if (corrupted_data.empty())
    corrupted_data.push_back(0);
  corrupted_data[corrupted_data.size() / 2] ^= 0x01;
  return corrupted_data;
}

std::vector<uint8> HexStringToBytes(const std::string& hex) {
  std::vector<uint8> bytes;
  base::HexStringToBytes(hex, &bytes);
  return bytes;
}

void ExpectArrayBufferMatches(const std::vector<uint8>& expected,
                              const blink::WebArrayBuffer& actual) {
  EXPECT_EQ(
      base::HexEncode(webcrypto::Uint8VectorStart(expected), expected.size()),
      base::HexEncode(actual.data(), actual.byteLength()));
}

void ExpectArrayBufferMatchesHex(const std::string& expected_hex,
                                 const blink::WebArrayBuffer& array_buffer) {
  EXPECT_STRCASEEQ(
      expected_hex.c_str(),
      base::HexEncode(array_buffer.data(), array_buffer.byteLength()).c_str());
}

void ExpectVectorMatches(const std::vector<uint8>& expected,
                         const std::vector<uint8>& actual) {
  EXPECT_EQ(
      base::HexEncode(webcrypto::Uint8VectorStart(expected), expected.size()),
      base::HexEncode(webcrypto::Uint8VectorStart(actual), actual.size()));
}

std::vector<uint8> MakeJsonVector(const std::string& json_string) {
  return std::vector<uint8>(json_string.begin(), json_string.end());
}

std::vector<uint8> MakeJsonVector(const base::DictionaryValue& dict) {
  std::string json;
  base::JSONWriter::Write(&dict, &json);
  return MakeJsonVector(json);
}

// ----------------------------------------------------------------
// Helpers for working with JSON data files for test expectations.
// ----------------------------------------------------------------

// Reads a file in "src/content/test/data/webcrypto" to a base::Value.
// The file must be JSON, however it can also include C++ style comments.
::testing::AssertionResult ReadJsonTestFile(
    const char* test_file_name,
    scoped_ptr<base::Value>* value) {
  base::FilePath test_data_dir;
  if (!PathService::Get(DIR_TEST_DATA, &test_data_dir))
    return ::testing::AssertionFailure() << "Couldn't retrieve test dir";

  base::FilePath file_path =
      test_data_dir.AppendASCII("webcrypto").AppendASCII(test_file_name);

  std::string file_contents;
  if (!base::ReadFileToString(file_path, &file_contents)) {
    return ::testing::AssertionFailure() << "Couldn't read test file: "
                                         << file_path.value();
  }

  // Strip C++ style comments out of the "json" file, otherwise it cannot be
  // parsed.
  re2::RE2::GlobalReplace(&file_contents, re2::RE2("\\s*//.*"), "");

  // Parse the JSON to a dictionary.
  value->reset(base::JSONReader::Read(file_contents));
  if (!value->get()) {
    return ::testing::AssertionFailure() << "Couldn't parse test file JSON: "
                                         << file_path.value();
  }

  return ::testing::AssertionSuccess();
}

// Same as ReadJsonTestFile(), but return the value as a List.
::testing::AssertionResult ReadJsonTestFileToList(
    const char* test_file_name,
    scoped_ptr<base::ListValue>* list) {
  // Read the JSON.
  scoped_ptr<base::Value> json;
  ::testing::AssertionResult result = ReadJsonTestFile(test_file_name, &json);
  if (!result)
    return result;

  // Cast to an ListValue.
  base::ListValue* list_value = NULL;
  if (!json->GetAsList(&list_value) || !list_value)
    return ::testing::AssertionFailure() << "The JSON was not a list";

  list->reset(list_value);
  ignore_result(json.release());

  return ::testing::AssertionSuccess();
}

// Read a string property from the dictionary with path |property_name|
// (which can include periods for nested dictionaries). Interprets the
// string as a hex encoded string and converts it to a bytes list.
//
// Returns empty vector on failure.
std::vector<uint8> GetBytesFromHexString(
    base::DictionaryValue* dict,
    const char* property_name) {
  std::string hex_string;
  if (!dict->GetString(property_name, &hex_string)) {
    EXPECT_TRUE(false) << "Couldn't get string property: " << property_name;
    return std::vector<uint8>();
  }

  return HexStringToBytes(hex_string);
}

// Reads a string property with path "property_name" and converts it to a
// WebCryptoAlgorith. Returns null algorithm on failure.
blink::WebCryptoAlgorithm GetDigestAlgorithm(
    base::DictionaryValue* dict,
    const char* property_name) {
  std::string algorithm_name;
  if (!dict->GetString(property_name, &algorithm_name)) {
    EXPECT_TRUE(false) << "Couldn't get string property: " << property_name;
    return blink::WebCryptoAlgorithm::createNull();
  }

  struct {
    const char* name;
    blink::WebCryptoAlgorithmId id;
  } kDigestNameToId[] = {
    {"sha-1", blink::WebCryptoAlgorithmIdSha1},
    {"sha-224", blink::WebCryptoAlgorithmIdSha224},
    {"sha-256", blink::WebCryptoAlgorithmIdSha256},
    {"sha-384", blink::WebCryptoAlgorithmIdSha384},
    {"sha-512", blink::WebCryptoAlgorithmIdSha512},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kDigestNameToId); ++i) {
    if (kDigestNameToId[i].name == algorithm_name)
      return webcrypto::CreateAlgorithm(kDigestNameToId[i].id);
  }

  return blink::WebCryptoAlgorithm::createNull();
}

// Helper for ImportJwkFailures and ImportJwkOctFailures. Restores the JWK JSON
// dictionary to a good state
void RestoreJwkOctDictionary(base::DictionaryValue* dict) {
  dict->Clear();
  dict->SetString("kty", "oct");
  dict->SetString("alg", "A128CBC");
  dict->SetString("use", "enc");
  dict->SetBoolean("extractable", false);
  dict->SetString("k", "GADWrMRHwQfoNaXU5fZvTg==");
}

blink::WebCryptoAlgorithm CreateAesGcmAlgorithm(
    const std::vector<uint8>& iv,
    const std::vector<uint8>& additional_data,
    unsigned int tag_length_bits) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdAesGcm,
      new blink::WebCryptoAesGcmParams(
          webcrypto::Uint8VectorStart(iv), iv.size(),
          true,
          webcrypto::Uint8VectorStart(additional_data),
          additional_data.size(),
          true, tag_length_bits));
}

// Helper for ImportJwkRsaFailures. Restores the JWK JSON
// dictionary to a good state
void RestoreJwkRsaDictionary(base::DictionaryValue* dict) {
  dict->Clear();
  dict->SetString("kty", "RSA");
  dict->SetString("alg", "RSA1_5");
  dict->SetString("use", "enc");
  dict->SetBoolean("extractable", false);
  dict->SetString("n",
      "qLOyhK-OtQs4cDSoYPFGxJGfMYdjzWxVmMiuSBGh4KvEx-CwgtaTpef87Wdc9GaFEncsDLxk"
      "p0LGxjD1M8jMcvYq6DPEC_JYQumEu3i9v5fAEH1VvbZi9cTg-rmEXLUUjvc5LdOq_5OuHmtm"
      "e7PUJHYW1PW6ENTP0ibeiNOfFvs");
  dict->SetString("e", "AQAB");
}

blink::WebCryptoAlgorithm CreateRsaAlgorithmWithInnerHash(
    blink::WebCryptoAlgorithmId algorithm_id,
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(algorithm_id == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 ||
         algorithm_id == blink::WebCryptoAlgorithmIdRsaOaep);
  DCHECK(webcrypto::IsHashAlgorithm(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      algorithm_id,
      new blink::WebCryptoRsaSsaParams(webcrypto::CreateAlgorithm(hash_id)));
}

// Determines if two ArrayBuffers have identical content.
bool ArrayBuffersEqual(
    const blink::WebArrayBuffer& a,
    const blink::WebArrayBuffer& b) {
  return a.byteLength() == b.byteLength() &&
         memcmp(a.data(), b.data(), a.byteLength()) == 0;
}

// Given a vector of WebArrayBuffers, determines if there are any copies.
bool CopiesExist(std::vector<blink::WebArrayBuffer> bufs) {
  for (size_t i = 0; i < bufs.size(); ++i) {
    for (size_t j = i + 1; j < bufs.size(); ++j) {
      if (ArrayBuffersEqual(bufs[i], bufs[j]))
        return true;
    }
  }
  return false;
}

blink::WebCryptoAlgorithm CreateAesKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId aes_alg_id,
    unsigned short length) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      aes_alg_id, new blink::WebCryptoAesKeyGenParams(length));
}

blink::WebCryptoAlgorithm CreateAesCbcKeyGenAlgorithm(
    unsigned short key_length_bits) {
  return CreateAesKeyGenAlgorithm(blink::WebCryptoAlgorithmIdAesCbc,
                                  key_length_bits);
}

blink::WebCryptoAlgorithm CreateAesGcmKeyGenAlgorithm(
    unsigned short key_length_bits) {
  return CreateAesKeyGenAlgorithm(blink::WebCryptoAlgorithmIdAesGcm,
                                  key_length_bits);
}

blink::WebCryptoAlgorithm CreateAesKwKeyGenAlgorithm(
    unsigned short key_length_bits) {
  return CreateAesKeyGenAlgorithm(blink::WebCryptoAlgorithmIdAesKw,
                                  key_length_bits);
}

// The following key pair is comprised of the SPKI (public key) and PKCS#8
// (private key) representations of the key pair provided in Example 1 of the
// NIST test vectors at
// ftp://ftp.rsa.com/pub/rsalabs/tmp/pkcs1v15sign-vectors.txt
const unsigned int kModulusLength = 1024;
const char* const kPublicKeySpkiDerHex =
    "30819f300d06092a864886f70d010101050003818d0030818902818100a5"
    "6e4a0e701017589a5187dc7ea841d156f2ec0e36ad52a44dfeb1e61f7ad9"
    "91d8c51056ffedb162b4c0f283a12a88a394dff526ab7291cbb307ceabfc"
    "e0b1dfd5cd9508096d5b2b8b6df5d671ef6377c0921cb23c270a70e2598e"
    "6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef22e1e1f20d0ce8cf"
    "fb2249bd9a21370203010001";
const char* const kPrivateKeyPkcs8DerHex =
    "30820275020100300d06092a864886f70d01010105000482025f3082025b"
    "02010002818100a56e4a0e701017589a5187dc7ea841d156f2ec0e36ad52"
    "a44dfeb1e61f7ad991d8c51056ffedb162b4c0f283a12a88a394dff526ab"
    "7291cbb307ceabfce0b1dfd5cd9508096d5b2b8b6df5d671ef6377c0921c"
    "b23c270a70e2598e6ff89d19f105acc2d3f0cb35f29280e1386b6f64c4ef"
    "22e1e1f20d0ce8cffb2249bd9a2137020301000102818033a5042a90b27d"
    "4f5451ca9bbbd0b44771a101af884340aef9885f2a4bbe92e894a724ac3c"
    "568c8f97853ad07c0266c8c6a3ca0929f1e8f11231884429fc4d9ae55fee"
    "896a10ce707c3ed7e734e44727a39574501a532683109c2abacaba283c31"
    "b4bd2f53c3ee37e352cee34f9e503bd80c0622ad79c6dcee883547c6a3b3"
    "25024100e7e8942720a877517273a356053ea2a1bc0c94aa72d55c6e8629"
    "6b2dfc967948c0a72cbccca7eacb35706e09a1df55a1535bd9b3cc34160b"
    "3b6dcd3eda8e6443024100b69dca1cf7d4d7ec81e75b90fcca874abcde12"
    "3fd2700180aa90479b6e48de8d67ed24f9f19d85ba275874f542cd20dc72"
    "3e6963364a1f9425452b269a6799fd024028fa13938655be1f8a159cbaca"
    "5a72ea190c30089e19cd274a556f36c4f6e19f554b34c077790427bbdd8d"
    "d3ede2448328f385d81b30e8e43b2fffa02786197902401a8b38f398fa71"
    "2049898d7fb79ee0a77668791299cdfa09efc0e507acb21ed74301ef5bfd"
    "48be455eaeb6e1678255827580a8e4e8e14151d1510a82a3f2e729024027"
    "156aba4126d24a81f3a528cbfb27f56886f840a9f6e86e17a44b94fe9319"
    "584b8e22fdde1e5a2e3bd8aa5ba8d8584194eb2190acf832b847f13a3d24"
    "a79f4d";

}  // namespace

class WebCryptoImplTest : public testing::Test {
 protected:
  blink::WebCryptoKey ImportSecretKeyFromRaw(
      const std::vector<uint8>& key_raw,
      const blink::WebCryptoAlgorithm& algorithm,
      blink::WebCryptoKeyUsageMask usage) {
    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    bool extractable = true;
    EXPECT_STATUS_SUCCESS(
        crypto_.ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                  webcrypto::Uint8VectorStart(key_raw),
                                  key_raw.size(),
                                  algorithm,
                                  extractable,
                                  usage,
                                  &key));

    EXPECT_FALSE(key.isNull());
    EXPECT_TRUE(key.handle());
    EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
    EXPECT_EQ(algorithm.id(), key.algorithm().id());
    EXPECT_EQ(extractable, key.extractable());
    EXPECT_EQ(usage, key.usages());
    return key;
  }

  void ImportRsaKeyPair(
      const std::vector<uint8>& spki_der,
      const std::vector<uint8>& pkcs8_der,
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* public_key,
      blink::WebCryptoKey* private_key) {
    EXPECT_STATUS_SUCCESS(ImportKeyInternal(
        blink::WebCryptoKeyFormatSpki,
        spki_der,
        algorithm,
        true,
        usage_mask,
        public_key));
    EXPECT_FALSE(public_key->isNull());
    EXPECT_TRUE(public_key->handle());
    EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key->type());
    EXPECT_EQ(algorithm.id(), public_key->algorithm().id());
    EXPECT_EQ(extractable, extractable);
    EXPECT_EQ(usage_mask, public_key->usages());

    EXPECT_STATUS_SUCCESS(ImportKeyInternal(
        blink::WebCryptoKeyFormatPkcs8,
        pkcs8_der,
        algorithm,
        extractable,
        usage_mask,
        private_key));
    EXPECT_FALSE(private_key->isNull());
    EXPECT_TRUE(private_key->handle());
    EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key->type());
    EXPECT_EQ(algorithm.id(), private_key->algorithm().id());
    EXPECT_EQ(extractable, extractable);
    EXPECT_EQ(usage_mask, private_key->usages());
  }

  // TODO(eroman): For Linux builds using system NSS, AES-GCM support is a
  // runtime dependency. Test it by trying to import a key.
  bool SupportsAesGcm() {
    std::vector<uint8> key_raw(16, 0);

    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    Status status = crypto_.ImportKeyInternal(
        blink::WebCryptoKeyFormatRaw,
        webcrypto::Uint8VectorStart(key_raw),
        key_raw.size(),
        webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesGcm),
        true,
        blink::WebCryptoKeyUsageEncrypt,
        &key);

    if (status.IsError())
      EXPECT_EQ(Status::ErrorUnsupported().ToString(), status.ToString());
    return status.IsSuccess();

  }

  Status AesGcmEncrypt(const blink::WebCryptoKey& key,
                       const std::vector<uint8>& iv,
                       const std::vector<uint8>& additional_data,
                       unsigned int tag_length_bits,
                       const std::vector<uint8>& plain_text,
                       std::vector<uint8>* cipher_text,
                       std::vector<uint8>* authentication_tag) {
    blink::WebCryptoAlgorithm algorithm = CreateAesGcmAlgorithm(
        iv, additional_data, tag_length_bits);

    blink::WebArrayBuffer output;
    Status status = EncryptInternal(algorithm, key, plain_text, &output);
    if (status.IsError())
      return status;

    if (output.byteLength() * 8 < tag_length_bits) {
      EXPECT_TRUE(false);
      return Status::Error();
    }

    // The encryption result is cipher text with authentication tag appended.
    cipher_text->assign(
        static_cast<uint8*>(output.data()),
        static_cast<uint8*>(output.data()) +
            (output.byteLength() - tag_length_bits / 8));
    authentication_tag->assign(
        static_cast<uint8*>(output.data()) + cipher_text->size(),
        static_cast<uint8*>(output.data()) + output.byteLength());

    return Status::Success();
  }

  Status AesGcmDecrypt(const blink::WebCryptoKey& key,
                       const std::vector<uint8>& iv,
                       const std::vector<uint8>& additional_data,
                       unsigned int tag_length_bits,
                       const std::vector<uint8>& cipher_text,
                       const std::vector<uint8>& authentication_tag,
                       blink::WebArrayBuffer* plain_text) {
    blink::WebCryptoAlgorithm algorithm = CreateAesGcmAlgorithm(
        iv, additional_data, tag_length_bits);

    // Join cipher text and authentication tag.
    std::vector<uint8> cipher_text_with_tag;
    cipher_text_with_tag.reserve(
        cipher_text.size() + authentication_tag.size());
    cipher_text_with_tag.insert(
        cipher_text_with_tag.end(), cipher_text.begin(), cipher_text.end());
    cipher_text_with_tag.insert(
        cipher_text_with_tag.end(), authentication_tag.begin(),
        authentication_tag.end());

    return DecryptInternal(algorithm, key, cipher_text_with_tag, plain_text);
  }

  // Forwarding methods to gain access to protected methods of
  // WebCryptoImpl.

  Status DigestInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.DigestInternal(
        algorithm, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  Status GenerateKeyInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      blink::WebCryptoKey* key) {
    bool extractable = true;
    blink::WebCryptoKeyUsageMask usage_mask = 0;
    return crypto_.GenerateSecretKeyInternal(
        algorithm, extractable, usage_mask, key);
  }

  Status GenerateKeyPairInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* public_key,
      blink::WebCryptoKey* private_key) {
    return crypto_.GenerateKeyPairInternal(
        algorithm, extractable, usage_mask, public_key, private_key);
  }

  Status ImportKeyInternal(
      blink::WebCryptoKeyFormat format,
      const std::vector<uint8>& key_data,
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* key) {
    return crypto_.ImportKeyInternal(format,
                                     webcrypto::Uint8VectorStart(key_data),
                                     key_data.size(),
                                     algorithm,
                                     extractable,
                                     usage_mask,
                                     key);
  }

  Status ExportKeyInternal(
      blink::WebCryptoKeyFormat format,
      const blink::WebCryptoKey& key,
      blink::WebArrayBuffer* buffer) {
    return crypto_.ExportKeyInternal(format, key, buffer);
  }

  Status SignInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.SignInternal(
        algorithm, key, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  Status VerifySignatureInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* signature,
      unsigned int signature_size,
      const std::vector<uint8>& data,
      bool* signature_match) {
    return crypto_.VerifySignatureInternal(algorithm,
                                           key,
                                           signature,
                                           signature_size,
                                           webcrypto::Uint8VectorStart(data),
                                           data.size(),
                                           signature_match);
  }

  Status VerifySignatureInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const std::vector<uint8>& signature,
      const std::vector<uint8>& data,
      bool* signature_match) {
    return crypto_.VerifySignatureInternal(
        algorithm,
        key,
        webcrypto::Uint8VectorStart(signature),
        signature.size(),
        webcrypto::Uint8VectorStart(data),
        data.size(),
        signature_match);
  }

  Status EncryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned int data_size,
      blink::WebArrayBuffer* buffer) {
    return crypto_.EncryptInternal(algorithm, key, data, data_size, buffer);
  }

  Status EncryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.EncryptInternal(
        algorithm, key, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  Status DecryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const unsigned char* data,
      unsigned int data_size,
      blink::WebArrayBuffer* buffer) {
    return crypto_.DecryptInternal(algorithm, key, data, data_size, buffer);
  }

  Status DecryptInternal(
      const blink::WebCryptoAlgorithm& algorithm,
      const blink::WebCryptoKey& key,
      const std::vector<uint8>& data,
      blink::WebArrayBuffer* buffer) {
    return crypto_.DecryptInternal(
        algorithm, key, webcrypto::Uint8VectorStart(data), data.size(), buffer);
  }

  Status ImportKeyJwk(
      const std::vector<uint8>& key_data,
      const blink::WebCryptoAlgorithm& algorithm,
      bool extractable,
      blink::WebCryptoKeyUsageMask usage_mask,
      blink::WebCryptoKey* key) {
    return crypto_.ImportKeyJwk(webcrypto::Uint8VectorStart(key_data),
                                key_data.size(),
                                algorithm,
                                extractable,
                                usage_mask,
                                key);
  }

 private:
  WebCryptoImpl crypto_;
};

TEST_F(WebCryptoImplTest, StatusToString) {
  EXPECT_EQ("Success", Status::Success().ToString());
  EXPECT_EQ("", Status::Error().ToString());
  EXPECT_EQ("The requested operation is unsupported",
            Status::ErrorUnsupported().ToString());
  EXPECT_EQ("The required JWK property \"kty\" was missing",
            Status::ErrorJwkPropertyMissing("kty").ToString());
  EXPECT_EQ("The JWK property \"kty\" must be a string",
            Status::ErrorJwkPropertyWrongType("kty", "string").ToString());
  EXPECT_EQ("The JWK property \"n\" could not be base64 decoded",
            Status::ErrorJwkBase64Decode("n").ToString());
}

TEST_F(WebCryptoImplTest, DigestSampleSets) {
  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("digest.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);
    base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    blink::WebCryptoAlgorithm test_algorithm =
        GetDigestAlgorithm(test, "algorithm");
    std::vector<uint8> test_input = GetBytesFromHexString(test, "input");
    std::vector<uint8> test_output = GetBytesFromHexString(test, "output");

    blink::WebArrayBuffer output;
    ASSERT_STATUS_SUCCESS(DigestInternal(test_algorithm, test_input, &output));
    ExpectArrayBufferMatches(test_output, output);
  }
}

TEST_F(WebCryptoImplTest, HMACSampleSets) {
  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("hmac.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);
    base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    blink::WebCryptoAlgorithm test_hash = GetDigestAlgorithm(test, "hash");
    const std::vector<uint8> test_key = GetBytesFromHexString(test, "key");
    const std::vector<uint8> test_message =
        GetBytesFromHexString(test, "message");
    const std::vector<uint8> test_mac = GetBytesFromHexString(test, "mac");

    blink::WebCryptoAlgorithm algorithm =
        webcrypto::CreateHmacAlgorithmByHashId(test_hash.id());

    blink::WebCryptoKey key = ImportSecretKeyFromRaw(
        test_key, algorithm, blink::WebCryptoKeyUsageSign);

    // Verify exported raw key is identical to the imported data
    blink::WebArrayBuffer raw_key;
    EXPECT_STATUS_SUCCESS(
        ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
    ExpectArrayBufferMatches(test_key, raw_key);

    blink::WebArrayBuffer output;

    ASSERT_STATUS_SUCCESS(SignInternal(algorithm, key, test_message, &output));

    ExpectArrayBufferMatches(test_mac, output);

    bool signature_match = false;
    EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
        algorithm,
        key,
        static_cast<const unsigned char*>(output.data()),
        output.byteLength(),
        test_message,
        &signature_match));
    EXPECT_TRUE(signature_match);

    // Ensure truncated signature does not verify by passing one less byte.
    EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
        algorithm,
        key,
        static_cast<const unsigned char*>(output.data()),
        output.byteLength() - 1,
        test_message,
        &signature_match));
    EXPECT_FALSE(signature_match);

    // Ensure truncated signature does not verify by passing no bytes.
    EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
        algorithm,
        key,
        NULL,
        0,
        test_message,
        &signature_match));
    EXPECT_FALSE(signature_match);

    // Ensure extra long signature does not cause issues and fails.
    const unsigned char kLongSignature[1024] = { 0 };
    EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
        algorithm,
        key,
        kLongSignature,
        sizeof(kLongSignature),
        test_message,
        &signature_match));
    EXPECT_FALSE(signature_match);
  }
}

TEST_F(WebCryptoImplTest, AesCbcFailures) {
  const std::string key_hex = "2b7e151628aed2a6abf7158809cf4f3c";
  blink::WebCryptoKey key = ImportSecretKeyFromRaw(
      HexStringToBytes(key_hex),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

  // Verify exported raw key is identical to the imported data
  blink::WebArrayBuffer raw_key;
  EXPECT_STATUS_SUCCESS(
      ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
  ExpectArrayBufferMatchesHex(key_hex, raw_key);

  blink::WebArrayBuffer output;

  // Use an invalid |iv| (fewer than 16 bytes)
  {
    std::vector<uint8> input(32);
    std::vector<uint8> iv;
    EXPECT_STATUS(Status::ErrorIncorrectSizeAesCbcIv(), EncryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, &output));
    EXPECT_STATUS(Status::ErrorIncorrectSizeAesCbcIv(), DecryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, &output));
  }

  // Use an invalid |iv| (more than 16 bytes)
  {
    std::vector<uint8> input(32);
    std::vector<uint8> iv(17);
    EXPECT_STATUS(Status::ErrorIncorrectSizeAesCbcIv(), EncryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, &output));
    EXPECT_STATUS(Status::ErrorIncorrectSizeAesCbcIv(), DecryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, &output));
  }

  // Give an input that is too large (would cause integer overflow when
  // narrowing to an int).
  {
    std::vector<uint8> iv(16);

    // Pretend the input is large. Don't pass data pointer as NULL in case that
    // is special cased; the implementation shouldn't actually dereference the
    // data.
    const unsigned char* input = &iv[0];
    unsigned int input_len = INT_MAX - 3;

    EXPECT_STATUS(Status::ErrorDataTooLarge(), EncryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, input_len, &output));
    EXPECT_STATUS(Status::ErrorDataTooLarge(), DecryptInternal(
        webcrypto::CreateAesCbcAlgorithm(iv), key, input, input_len, &output));
  }

  // Fail importing the key (too few bytes specified)
  {
    std::vector<uint8> key_raw(1);
    std::vector<uint8> iv(16);

    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    EXPECT_STATUS(
        Status::Error(),
        ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                          key_raw,
                          webcrypto::CreateAesCbcAlgorithm(iv),
                          true,
                          blink::WebCryptoKeyUsageEncrypt,
                          &key));
  }

  // TODO(eroman): Enable for OpenSSL once implemented.
#if !defined(USE_OPENSSL)
  // Fail exporting the key in SPKI and PKCS#8 formats (not allowed for secret
  // keys).
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(),
      ExportKeyInternal(blink::WebCryptoKeyFormatSpki, key, &output));
  EXPECT_STATUS(Status::ErrorUnsupported(),
      ExportKeyInternal(blink::WebCryptoKeyFormatPkcs8, key, &output));
#endif
}

TEST_F(WebCryptoImplTest, MAYBE(AesCbcSampleSets)) {
  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("aes_cbc.json", &tests));

  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);
    base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    std::vector<uint8> test_key = GetBytesFromHexString(test, "key");
    std::vector<uint8> test_iv = GetBytesFromHexString(test, "iv");
    std::vector<uint8> test_plain_text =
        GetBytesFromHexString(test, "plain_text");
    std::vector<uint8> test_cipher_text =
        GetBytesFromHexString(test, "cipher_text");

    blink::WebCryptoKey key = ImportSecretKeyFromRaw(
        test_key,
        webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
        blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

    // Verify exported raw key is identical to the imported data
    blink::WebArrayBuffer raw_key;
    EXPECT_STATUS_SUCCESS(ExportKeyInternal(
        blink::WebCryptoKeyFormatRaw, key, &raw_key));
    ExpectArrayBufferMatches(test_key, raw_key);

    blink::WebArrayBuffer output;

    // Test encryption.
    EXPECT_STATUS(
        Status::Success(),
        EncryptInternal(webcrypto::CreateAesCbcAlgorithm(test_iv),
                        key,
                        test_plain_text,
                        &output));
    ExpectArrayBufferMatches(test_cipher_text, output);

    // Test decryption.
    EXPECT_STATUS(
        Status::Success(),
        DecryptInternal(webcrypto::CreateAesCbcAlgorithm(test_iv),
                        key,
                        test_cipher_text,
                        &output));
    ExpectArrayBufferMatches(test_plain_text, output);

    const unsigned int kAesCbcBlockSize = 16;

    // Decrypt with a padding error by stripping the last block. This also ends
    // up testing decryption over empty cipher text.
    if (test_cipher_text.size() >= kAesCbcBlockSize) {
      EXPECT_STATUS(
          Status::Error(),
          DecryptInternal(webcrypto::CreateAesCbcAlgorithm(test_iv),
                          key,
                          &test_cipher_text[0],
                          test_cipher_text.size() - kAesCbcBlockSize,
                          &output));
    }

    // Decrypt cipher text which is not a multiple of block size by stripping
    // a few bytes off the cipher text.
    if (test_cipher_text.size() > 3) {
      EXPECT_STATUS(
          Status::Error(),
          DecryptInternal(webcrypto::CreateAesCbcAlgorithm(test_iv),
                          key,
                          &test_cipher_text[0],
                          test_cipher_text.size() - 3,
                          &output));
    }
  }
}

TEST_F(WebCryptoImplTest, MAYBE(GenerateKeyAes)) {
  // Check key generation for each of AES-CBC, AES-GCM, and AES-KW, and for each
  // allowed key length.
  std::vector<blink::WebCryptoAlgorithm> algorithm;
  const unsigned short kKeyLength[] = {128, 192, 256};
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kKeyLength); ++i) {
    algorithm.push_back(CreateAesCbcKeyGenAlgorithm(kKeyLength[i]));
    algorithm.push_back(CreateAesGcmKeyGenAlgorithm(kKeyLength[i]));
    algorithm.push_back(CreateAesKwKeyGenAlgorithm(kKeyLength[i]));
  }
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  std::vector<blink::WebArrayBuffer> keys;
  blink::WebArrayBuffer key_bytes;
  for (size_t i = 0; i < algorithm.size(); ++i) {
    SCOPED_TRACE(i);
    // Generate a small sample of keys.
    keys.clear();
    for (int j = 0; j < 16; ++j) {
      ASSERT_STATUS_SUCCESS(GenerateKeyInternal(algorithm[i], &key));
      EXPECT_TRUE(key.handle());
      EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
      ASSERT_STATUS_SUCCESS(
          ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &key_bytes));
      keys.push_back(key_bytes);
    }
    // Ensure all entries in the key sample set are unique. This is a simplistic
    // estimate of whether the generated keys appear random.
    EXPECT_FALSE(CopiesExist(keys));
  }
}

TEST_F(WebCryptoImplTest, MAYBE(GenerateKeyAesBadLength)) {
  const unsigned short kKeyLen[] = {0, 127, 257};
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kKeyLen); ++i) {
    SCOPED_TRACE(i);
    EXPECT_STATUS(Status::ErrorGenerateKeyLength(), GenerateKeyInternal(
        CreateAesCbcKeyGenAlgorithm(kKeyLen[i]), &key));
    EXPECT_STATUS(Status::ErrorGenerateKeyLength(), GenerateKeyInternal(
        CreateAesGcmKeyGenAlgorithm(kKeyLen[i]), &key));
    EXPECT_STATUS(Status::ErrorGenerateKeyLength(), GenerateKeyInternal(
        CreateAesKwKeyGenAlgorithm(kKeyLen[i]), &key));
  }
}

TEST_F(WebCryptoImplTest, MAYBE(GenerateKeyHmac)) {
  // Generate a small sample of HMAC keys.
  std::vector<blink::WebArrayBuffer> keys;
  for (int i = 0; i < 16; ++i) {
    blink::WebArrayBuffer key_bytes;
    blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
    blink::WebCryptoAlgorithm algorithm = webcrypto::CreateHmacKeyGenAlgorithm(
        blink::WebCryptoAlgorithmIdSha1, 64);
    ASSERT_STATUS_SUCCESS(GenerateKeyInternal(algorithm, &key));
    EXPECT_FALSE(key.isNull());
    EXPECT_TRUE(key.handle());
    EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
    EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, key.algorithm().id());

    blink::WebArrayBuffer raw_key;
    ASSERT_STATUS_SUCCESS(
        ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
    EXPECT_EQ(64U, raw_key.byteLength());
    keys.push_back(raw_key);
  }
  // Ensure all entries in the key sample set are unique. This is a simplistic
  // estimate of whether the generated keys appear random.
  EXPECT_FALSE(CopiesExist(keys));
}

// If the key length is not provided, then the block size is used.
TEST_F(WebCryptoImplTest, MAYBE(GenerateKeyHmacNoLength)) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateHmacKeyGenAlgorithm(blink::WebCryptoAlgorithmIdSha1, 0);
  ASSERT_STATUS_SUCCESS(GenerateKeyInternal(algorithm, &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
  blink::WebArrayBuffer raw_key;
  ASSERT_STATUS_SUCCESS(
      ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
  EXPECT_EQ(64U, raw_key.byteLength());

  // The block size for HMAC SHA-512 is larger.
  algorithm = webcrypto::CreateHmacKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdSha512, 0);
  ASSERT_STATUS_SUCCESS(GenerateKeyInternal(algorithm, &key));
  ASSERT_STATUS_SUCCESS(
      ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &raw_key));
  EXPECT_EQ(128U, raw_key.byteLength());
}

TEST_F(WebCryptoImplTest, MAYBE(ImportSecretKeyNoAlgorithm)) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  // This fails because the algorithm is null.
  EXPECT_STATUS(Status::ErrorMissingAlgorithmImportRawKey(), ImportKeyInternal(
      blink::WebCryptoKeyFormatRaw,
      HexStringToBytes("00000000000000000000"),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));
}


TEST_F(WebCryptoImplTest, ImportJwkFailures) {

  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageEncrypt;

  // Baseline pass: each test below breaks a single item, so we start with a
  // passing case to make sure each failure is caused by the isolated break.
  // Each breaking subtest below resets the dictionary to this passing case when
  // complete.
  base::DictionaryValue dict;
  RestoreJwkOctDictionary(&dict);
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));

  // Fail on empty JSON.
  EXPECT_STATUS(Status::ErrorImportEmptyKeyData(), ImportKeyJwk(
      MakeJsonVector(""), algorithm, false, usage_mask, &key));

  // Fail on invalid JSON.
  const std::vector<uint8> bad_json_vec = MakeJsonVector(
      "{"
        "\"kty\"         : \"oct\","
        "\"alg\"         : \"HS256\","
        "\"use\"         : "
  );
  EXPECT_STATUS(Status::ErrorJwkNotDictionary(),
      ImportKeyJwk(bad_json_vec, algorithm, false, usage_mask, &key));

  // Fail on JWK alg present but unrecognized.
  dict.SetString("alg", "A127CBC");
  EXPECT_STATUS(Status::ErrorJwkUnrecognizedAlgorithm(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on both JWK and input algorithm missing.
  dict.Remove("alg", NULL);
  EXPECT_STATUS(
      Status::ErrorJwkAlgorithmMissing(),
      ImportKeyJwk(MakeJsonVector(dict),
                   blink::WebCryptoAlgorithm::createNull(),
                   false,
                   usage_mask,
                   &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on invalid kty.
  dict.SetString("kty", "foo");
  EXPECT_STATUS(Status::ErrorJwkUnrecognizedKty(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on missing kty.
  dict.Remove("kty", NULL);
  EXPECT_STATUS(
      Status::ErrorJwkPropertyMissing("kty"),
      ImportKeyJwk(MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on kty wrong type.
  dict.SetDouble("kty", 0.1);
  EXPECT_STATUS(
      Status::ErrorJwkPropertyWrongType("kty", "string"),
      ImportKeyJwk(MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on invalid use.
  dict.SetString("use", "foo");
  EXPECT_STATUS(Status::ErrorJwkUnrecognizedUsage(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on invalid use (wrong type).
  dict.SetBoolean("use", true);
  EXPECT_STATUS(
      Status::ErrorJwkPropertyWrongType("use", "string"),
      ImportKeyJwk(MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on invalid extractable (wrong type).
  dict.SetInteger("extractable", 0);
  EXPECT_STATUS(
      Status::ErrorJwkPropertyWrongType("extractable", "boolean"),
      ImportKeyJwk(MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);
}

TEST_F(WebCryptoImplTest, ImportJwkOctFailures) {

  base::DictionaryValue dict;
  RestoreJwkOctDictionary(&dict);
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageEncrypt;
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  // Baseline pass.
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  EXPECT_EQ(algorithm.id(), key.algorithm().id());
  EXPECT_FALSE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());
  EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());

  // The following are specific failure cases for when kty = "oct".

  // Fail on missing k.
  dict.Remove("k", NULL);
  EXPECT_STATUS(
      Status::ErrorJwkPropertyMissing("k"),
      ImportKeyJwk(MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on bad b64 encoding for k.
  dict.SetString("k", "Qk3f0DsytU8lfza2au #$% Htaw2xpop9GYyTuH0p5GghxTI=");
  EXPECT_STATUS(
      Status::ErrorJwkBase64Decode("k"),
      ImportKeyJwk(MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on empty k.
  dict.SetString("k", "");
  EXPECT_STATUS(Status::ErrorJwkIncorrectKeyLength(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on k actual length (120 bits) inconsistent with the embedded JWK alg
  // value (128) for an AES key.
  dict.SetString("k", "AVj42h0Y5aqGtE3yluKL");
  EXPECT_STATUS(Status::ErrorJwkIncorrectKeyLength(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);

  // Fail on k actual length (192 bits) inconsistent with the embedded JWK alg
  // value (128) for an AES key.
  dict.SetString("k", "dGhpcyAgaXMgIDI0ICBieXRlcyBsb25n");
  EXPECT_STATUS(Status::ErrorJwkIncorrectKeyLength(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkOctDictionary(&dict);
}

TEST_F(WebCryptoImplTest, MAYBE(ImportJwkRsaFailures)) {

  base::DictionaryValue dict;
  RestoreJwkRsaDictionary(&dict);
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageEncrypt;
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();

  // An RSA public key JWK _must_ have an "n" (modulus) and an "e" (exponent)
  // entry, while an RSA private key must have those plus at least a "d"
  // (private exponent) entry.
  // See http://tools.ietf.org/html/draft-ietf-jose-json-web-algorithms-18,
  // section 6.3.

  // Baseline pass.
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  EXPECT_EQ(algorithm.id(), key.algorithm().id());
  EXPECT_FALSE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, key.type());

  // The following are specific failure cases for when kty = "RSA".

  // Fail if either "n" or "e" is not present or malformed.
  const std::string kKtyParmName[] = {"n", "e"};
  for (size_t idx = 0; idx < ARRAYSIZE_UNSAFE(kKtyParmName); ++idx) {

    // Fail on missing parameter.
    dict.Remove(kKtyParmName[idx], NULL);
    EXPECT_STATUS_ERROR(ImportKeyJwk(
        MakeJsonVector(dict), algorithm, false, usage_mask, &key));
    RestoreJwkRsaDictionary(&dict);

    // Fail on bad b64 parameter encoding.
    dict.SetString(kKtyParmName[idx], "Qk3f0DsytU8lfza2au #$% Htaw2xpop9yTuH0");
    EXPECT_STATUS_ERROR(ImportKeyJwk(
        MakeJsonVector(dict), algorithm, false, usage_mask, &key));
    RestoreJwkRsaDictionary(&dict);

    // Fail on empty parameter.
    dict.SetString(kKtyParmName[idx], "");
    EXPECT_STATUS_ERROR(ImportKeyJwk(
        MakeJsonVector(dict), algorithm, false, usage_mask, &key));
    RestoreJwkRsaDictionary(&dict);
  }

  // Fail if "d" parameter is present, implying the JWK is a private key, which
  // is not supported.
  dict.SetString("d", "Qk3f0Dsyt");
  EXPECT_STATUS(Status::ErrorJwkRsaPrivateKeyUnsupported(), ImportKeyJwk(
      MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  RestoreJwkRsaDictionary(&dict);
}

TEST_F(WebCryptoImplTest, MAYBE(ImportJwkInputConsistency)) {
  // The Web Crypto spec says that if a JWK value is present, but is
  // inconsistent with the input value, the operation must fail.

  // Consistency rules when JWK value is not present: Inputs should be used.
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  bool extractable = false;
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateHmacAlgorithmByHashId(blink::WebCryptoAlgorithmIdSha256);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageVerify;
  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  std::vector<uint8> json_vec = MakeJsonVector(dict);
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(
      json_vec, algorithm, extractable, usage_mask, &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypeSecret, key.type());
  EXPECT_EQ(extractable, key.extractable());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, key.algorithm().id());
  EXPECT_EQ(blink::WebCryptoAlgorithmIdSha256,
            key.algorithm().hmacParams()->hash().id());
  EXPECT_EQ(blink::WebCryptoKeyUsageVerify, key.usages());
  key = blink::WebCryptoKey::createNull();

  // Consistency rules when JWK value exists: Fail if inconsistency is found.

  // Pass: All input values are consistent with the JWK values.
  dict.Clear();
  dict.SetString("kty", "oct");
  dict.SetString("alg", "HS256");
  dict.SetString("use", "sig");
  dict.SetBoolean("extractable", false);
  dict.SetString("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  json_vec = MakeJsonVector(dict);
  EXPECT_STATUS_SUCCESS(
      ImportKeyJwk(json_vec, algorithm, extractable, usage_mask, &key));

  // Extractable cases:
  // 1. input=T, JWK=F ==> fail (inconsistent)
  // 4. input=F, JWK=F ==> pass, result extractable is F
  // 2. input=T, JWK=T ==> pass, result extractable is T
  // 3. input=F, JWK=T ==> pass, result extractable is F
  EXPECT_STATUS(Status::ErrorJwkExtractableInconsistent(),
      ImportKeyJwk(json_vec, algorithm, true, usage_mask, &key));
  EXPECT_STATUS_SUCCESS(
      ImportKeyJwk(json_vec, algorithm, false, usage_mask, &key));
  EXPECT_FALSE(key.extractable());
  dict.SetBoolean("extractable", true);
  EXPECT_STATUS_SUCCESS(
      ImportKeyJwk(MakeJsonVector(dict), algorithm, true, usage_mask, &key));
  EXPECT_TRUE(key.extractable());
  EXPECT_STATUS_SUCCESS(
      ImportKeyJwk(MakeJsonVector(dict), algorithm, false, usage_mask, &key));
  EXPECT_FALSE(key.extractable());
  dict.SetBoolean("extractable", true);  // restore previous value

  // Fail: Input algorithm (AES-CBC) is inconsistent with JWK value
  // (HMAC SHA256).
  EXPECT_STATUS(Status::ErrorJwkAlgorithmInconsistent(), ImportKeyJwk(
      json_vec,
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      extractable,
      usage_mask,
      &key));

  // Fail: Input algorithm (HMAC SHA1) is inconsistent with JWK value
  // (HMAC SHA256).
  EXPECT_STATUS(Status::ErrorJwkAlgorithmInconsistent(), ImportKeyJwk(
      json_vec,
      webcrypto::CreateHmacAlgorithmByHashId(blink::WebCryptoAlgorithmIdSha1),
      extractable,
      usage_mask,
      &key));

  // Pass: JWK alg valid but input algorithm isNull: use JWK algorithm value.
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(json_vec,
                        blink::WebCryptoAlgorithm::createNull(),
                        extractable,
                        usage_mask,
                        &key));
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, algorithm.id());

  // Pass: JWK alg missing but input algorithm specified: use input value
  dict.Remove("alg", NULL);
  EXPECT_STATUS_SUCCESS(ImportKeyJwk(
      MakeJsonVector(dict),
      webcrypto::CreateHmacAlgorithmByHashId(blink::WebCryptoAlgorithmIdSha256),
      extractable,
      usage_mask,
      &key));
  EXPECT_EQ(blink::WebCryptoAlgorithmIdHmac, algorithm.id());
  dict.SetString("alg", "HS256");

  // Fail: Input usage_mask (encrypt) is not a subset of the JWK value
  // (sign|verify)
  EXPECT_STATUS(Status::ErrorJwkUsageInconsistent(), ImportKeyJwk(
      json_vec, algorithm, extractable, blink::WebCryptoKeyUsageEncrypt, &key));

  // Fail: Input usage_mask (encrypt|sign|verify) is not a subset of the JWK
  // value (sign|verify)
  usage_mask = blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageSign |
               blink::WebCryptoKeyUsageVerify;
  EXPECT_STATUS(
      Status::ErrorJwkUsageInconsistent(),
      ImportKeyJwk(json_vec, algorithm, extractable, usage_mask, &key));

  // TODO(padolph): kty vs alg consistency tests: Depending on the kty value,
  // only certain alg values are permitted. For example, when kty = "RSA" alg
  // must be of the RSA family, or when kty = "oct" alg must be symmetric
  // algorithm.
}

TEST_F(WebCryptoImplTest, MAYBE(ImportJwkHappy)) {

  // This test verifies the happy path of JWK import, including the application
  // of the imported key material.

  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  bool extractable = false;
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateHmacAlgorithmByHashId(blink::WebCryptoAlgorithmIdSha256);
  blink::WebCryptoKeyUsageMask usage_mask = blink::WebCryptoKeyUsageSign;

  // Import a symmetric key JWK and HMAC-SHA256 sign()
  // Uses the first SHA256 test vector from the HMAC sample set above.

  base::DictionaryValue dict;
  dict.SetString("kty", "oct");
  dict.SetString("alg", "HS256");
  dict.SetString("use", "sig");
  dict.SetBoolean("extractable", false);
  dict.SetString("k", "l3nZEgZCeX8XRwJdWyK3rGB8qwjhdY8vOkbIvh4lxTuMao9Y_--hdg");
  std::vector<uint8> json_vec = MakeJsonVector(dict);

  ASSERT_STATUS_SUCCESS(ImportKeyJwk(
      json_vec, algorithm, extractable, usage_mask, &key));

  const std::vector<uint8> message_raw = HexStringToBytes(
      "b1689c2591eaf3c9e66070f8a77954ffb81749f1b00346f9dfe0b2ee905dcc288baf4a"
      "92de3f4001dd9f44c468c3d07d6c6ee82faceafc97c2fc0fc0601719d2dcd0aa2aec92"
      "d1b0ae933c65eb06a03c9c935c2bad0459810241347ab87e9f11adb30415424c6c7f5f"
      "22a003b8ab8de54f6ded0e3ab9245fa79568451dfa258e");

  blink::WebArrayBuffer output;

  ASSERT_STATUS_SUCCESS(SignInternal(algorithm, key, message_raw, &output));

  const std::string mac_raw =
      "769f00d3e6a6cc1fb426a14a4f76c6462e6149726e0dee0ec0cf97a16605ac8b";

  ExpectArrayBufferMatchesHex(mac_raw, output);

  // TODO(padolph): Import an RSA public key JWK and use it
}

TEST_F(WebCryptoImplTest, MAYBE(ImportExportSpki)) {
  // Passing case: Import a valid RSA key in SPKI format.
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(kPublicKeySpkiDerHex),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, key.type());
  EXPECT_TRUE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageEncrypt, key.usages());

  // Failing case: Empty SPKI data
  EXPECT_STATUS(Status::ErrorImportEmptyKeyData(), ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      std::vector<uint8>(),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Failing case: Import RSA key with NULL input algorithm. This is not
  // allowed because the SPKI ASN.1 format for RSA keys is not specific enough
  // to map to a Web Crypto algorithm.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(kPublicKeySpkiDerHex),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Failing case: Bad DER encoding.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes("618333c4cb"),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Failing case: Import RSA key but provide an inconsistent input algorithm.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(kPublicKeySpkiDerHex),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      true,
      blink::WebCryptoKeyUsageEncrypt,
      &key));

  // Passing case: Export a previously imported RSA public key in SPKI format
  // and compare to original data.
  blink::WebArrayBuffer output;
  ASSERT_STATUS_SUCCESS(
      ExportKeyInternal(blink::WebCryptoKeyFormatSpki, key, &output));
  ExpectArrayBufferMatchesHex(kPublicKeySpkiDerHex, output);

  // Failing case: Try to export a previously imported RSA public key in raw
  // format (not allowed for a public key).
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(),
      ExportKeyInternal(blink::WebCryptoKeyFormatRaw, key, &output));

  // Failing case: Try to export a non-extractable key
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(
      blink::WebCryptoKeyFormatSpki,
      HexStringToBytes(kPublicKeySpkiDerHex),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5),
      false,
      blink::WebCryptoKeyUsageEncrypt,
      &key));
  EXPECT_TRUE(key.handle());
  EXPECT_FALSE(key.extractable());
  EXPECT_STATUS(Status::ErrorKeyNotExtractable(),
      ExportKeyInternal(blink::WebCryptoKeyFormatSpki, key, &output));
}

TEST_F(WebCryptoImplTest, MAYBE(ImportPkcs8)) {
  // Passing case: Import a valid RSA key in PKCS#8 format.
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));
  EXPECT_TRUE(key.handle());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, key.type());
  EXPECT_TRUE(key.extractable());
  EXPECT_EQ(blink::WebCryptoKeyUsageSign, key.usages());

  // Failing case: Empty PKCS#8 data
  EXPECT_STATUS(Status::ErrorImportEmptyKeyData(), ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      std::vector<uint8>(),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));

  // Failing case: Import RSA key with NULL input algorithm. This is not
  // allowed because the PKCS#8 ASN.1 format for RSA keys is not specific enough
  // to map to a Web Crypto algorithm.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      blink::WebCryptoAlgorithm::createNull(),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));

  // Failing case: Bad DER encoding.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes("618333c4cb"),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));

  // Failing case: Import RSA key but provide an inconsistent input algorithm.
  EXPECT_STATUS(Status::Error(), ImportKeyInternal(
      blink::WebCryptoKeyFormatPkcs8,
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesCbc),
      true,
      blink::WebCryptoKeyUsageSign,
      &key));
}

TEST_F(WebCryptoImplTest, MAYBE(GenerateKeyPairRsa)) {
  // Note: using unrealistic short key lengths here to avoid bogging down tests.

  // Successful WebCryptoAlgorithmIdRsaEsPkcs1v1_5 key generation.
  const unsigned int modulus_length = 256;
  const std::vector<uint8> public_exponent = HexStringToBytes("010001");
  blink::WebCryptoAlgorithm algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      public_exponent);
  bool extractable = false;
  const blink::WebCryptoKeyUsageMask usage_mask = 0;
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  EXPECT_STATUS_SUCCESS(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_TRUE(public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());

  // Fail with bad modulus.
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5, 0, public_exponent);
  EXPECT_STATUS(Status::ErrorGenerateRsaZeroModulus(), GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));

  // Fail with bad exponent: larger than unsigned long.
  unsigned int exponent_length = sizeof(unsigned long) + 1;  // NOLINT
  const std::vector<uint8> long_exponent(exponent_length, 0x01);
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      long_exponent);
  EXPECT_STATUS(Status::ErrorGenerateKeyPublicExponent(),
      GenerateKeyPairInternal(algorithm, extractable, usage_mask, &public_key,
          &private_key));

  // Fail with bad exponent: empty.
  const std::vector<uint8> empty_exponent;
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      empty_exponent);
  EXPECT_STATUS(Status::ErrorGenerateKeyPublicExponent(),
      GenerateKeyPairInternal(algorithm, extractable, usage_mask, &public_key,
          &private_key));

  // Fail with bad exponent: all zeros.
  std::vector<uint8> exponent_with_leading_zeros(15, 0x00);
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      exponent_with_leading_zeros);
  EXPECT_STATUS(Status::ErrorGenerateKeyPublicExponent(),
      GenerateKeyPairInternal(algorithm, extractable, usage_mask, &public_key,
          &private_key));

  // Key generation success using exponent with leading zeros.
  exponent_with_leading_zeros.insert(exponent_with_leading_zeros.end(),
                                     public_exponent.begin(),
                                     public_exponent.end());
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5,
      modulus_length,
      exponent_with_leading_zeros);
  EXPECT_STATUS_SUCCESS(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_TRUE(public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());

  // Successful WebCryptoAlgorithmIdRsaOaep key generation.
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaOaep, modulus_length, public_exponent);
  EXPECT_STATUS_SUCCESS(GenerateKeyPairInternal(
      algorithm, extractable, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
  EXPECT_TRUE(public_key.extractable());
  EXPECT_EQ(extractable, private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());

  // Successful WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 key generation.
  algorithm = webcrypto::CreateRsaKeyGenAlgorithm(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      modulus_length,
      public_exponent);
  EXPECT_STATUS_SUCCESS(GenerateKeyPairInternal(
      algorithm, false, usage_mask, &public_key, &private_key));
  EXPECT_FALSE(public_key.isNull());
  EXPECT_FALSE(private_key.isNull());
  EXPECT_EQ(blink::WebCryptoKeyTypePublic, public_key.type());
  EXPECT_EQ(blink::WebCryptoKeyTypePrivate, private_key.type());
  // Even though "extractable" was set to false, the public key remains
  // extractable.
  EXPECT_TRUE(public_key.extractable());
  EXPECT_FALSE(private_key.extractable());
  EXPECT_EQ(usage_mask, public_key.usages());
  EXPECT_EQ(usage_mask, private_key.usages());

  // Exporting a private key as SPKI format doesn't make sense. However this
  // will first fail because the key is not extractable.
  blink::WebArrayBuffer output;
  EXPECT_STATUS(Status::ErrorKeyNotExtractable(), ExportKeyInternal(
      blink::WebCryptoKeyFormatSpki, private_key, &output));

  // Re-generate an extractable private_key and try to export it as SPKI format.
  // This should fail since spki is for public keys.
  EXPECT_STATUS_SUCCESS(GenerateKeyPairInternal(
      algorithm, true, usage_mask, &public_key, &private_key));
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(), ExportKeyInternal(
      blink::WebCryptoKeyFormatSpki, private_key, &output));
}

TEST_F(WebCryptoImplTest, MAYBE(RsaEsRoundTrip)) {
  // Import a key pair.
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      algorithm,
      false,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
      &public_key,
      &private_key);

  // Make a maximum-length data message. RSAES can operate on messages up to
  // length of k - 11 bytes, where k is the octet length of the RSA modulus.
  const unsigned int kMaxMsgSizeBytes = kModulusLength / 8 - 11;
  // There are two hex chars for each byte.
  const unsigned int kMsgHexSize = kMaxMsgSizeBytes * 2;
  char max_data_hex[kMsgHexSize+1];
  std::fill(&max_data_hex[0], &max_data_hex[0] + kMsgHexSize, 'a');
  max_data_hex[kMsgHexSize] = '\0';

  // Verify encrypt / decrypt round trip on a few messages. Note that RSA
  // encryption does not support empty input.
  algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  const char* const kTestDataHex[] = {
      "ff",
      "0102030405060708090a0b0c0d0e0f",
      max_data_hex
  };
  blink::WebArrayBuffer encrypted_data;
  blink::WebArrayBuffer decrypted_data;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestDataHex); ++i) {
    SCOPED_TRACE(i);
    EXPECT_STATUS_SUCCESS(EncryptInternal(
        algorithm,
        public_key,
        HexStringToBytes(kTestDataHex[i]),
        &encrypted_data));
    EXPECT_EQ(kModulusLength / 8, encrypted_data.byteLength());
    ASSERT_STATUS_SUCCESS(DecryptInternal(
        algorithm,
        private_key,
        reinterpret_cast<const unsigned char*>(encrypted_data.data()),
        encrypted_data.byteLength(),
        &decrypted_data));
    ExpectArrayBufferMatchesHex(kTestDataHex[i], decrypted_data);
  }
}

TEST_F(WebCryptoImplTest, MAYBE(RsaEsKnownAnswer)) {
  scoped_ptr<base::Value> json;
  ASSERT_TRUE(ReadJsonTestFile("rsa_es.json", &json));
  base::DictionaryValue* test = NULL;
  ASSERT_TRUE(json->GetAsDictionary(&test));

  // Because the random data in PKCS1.5 padding makes the encryption output non-
  // deterministic, we cannot easily do a typical known-answer test for RSA
  // encryption / decryption. Instead we will take a known-good encrypted
  // message, decrypt it, re-encrypt it, then decrypt again, verifying that the
  // original known cleartext is the result.

  const std::vector<uint8> rsa_spki_der =
      GetBytesFromHexString(test, "rsa_spki_der");

  const std::vector<uint8> rsa_pkcs8_der =
      GetBytesFromHexString(test, "rsa_pkcs8_der");
  const std::vector<uint8> ciphertext =
      GetBytesFromHexString(test, "ciphertext");
  const std::vector<uint8> cleartext =
      GetBytesFromHexString(test, "cleartext");

  // Import the key pair.
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ImportRsaKeyPair(
      rsa_spki_der,
      rsa_pkcs8_der,
      algorithm,
      false,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
      &public_key,
      &private_key);

  // Decrypt the known-good ciphertext with the private key. As a check we must
  // get the known original cleartext.
  blink::WebArrayBuffer decrypted_data;
  ASSERT_STATUS_SUCCESS(DecryptInternal(
      algorithm,
      private_key,
      ciphertext,
      &decrypted_data));
  EXPECT_FALSE(decrypted_data.isNull());
  ExpectArrayBufferMatches(cleartext, decrypted_data);

  // Encrypt this decrypted data with the public key.
  blink::WebArrayBuffer encrypted_data;
  ASSERT_STATUS_SUCCESS(EncryptInternal(
      algorithm,
      public_key,
      reinterpret_cast<const unsigned char*>(decrypted_data.data()),
      decrypted_data.byteLength(),
      &encrypted_data));
  EXPECT_EQ(128u, encrypted_data.byteLength());

  // Finally, decrypt the newly encrypted result with the private key, and
  // compare to the known original cleartext.
  decrypted_data.reset();
  ASSERT_STATUS_SUCCESS(DecryptInternal(
      algorithm,
      private_key,
      reinterpret_cast<const unsigned char*>(encrypted_data.data()),
      encrypted_data.byteLength(),
      &decrypted_data));
  EXPECT_FALSE(decrypted_data.isNull());
  ExpectArrayBufferMatches(cleartext, decrypted_data);
}

TEST_F(WebCryptoImplTest, MAYBE(RsaEsFailures)) {
  // Import a key pair.
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      algorithm,
      false,
      blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt,
      &public_key,
      &private_key);

  // Fail encrypt with a private key.
  blink::WebArrayBuffer encrypted_data;
  const std::string message_hex_str("0102030405060708090a0b0c0d0e0f");
  const std::vector<uint8> message_hex(HexStringToBytes(message_hex_str));
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(),
      EncryptInternal(algorithm, private_key, message_hex, &encrypted_data));

  // Fail encrypt with empty message.
  EXPECT_STATUS(Status::Error(), EncryptInternal(
      algorithm, public_key, std::vector<uint8>(), &encrypted_data));

  // Fail encrypt with message too large. RSAES can operate on messages up to
  // length of k - 11 bytes, where k is the octet length of the RSA modulus.
  const unsigned int kMaxMsgSizeBytes = kModulusLength / 8 - 11;
  EXPECT_STATUS(
      Status::ErrorDataTooLarge(),
      EncryptInternal(algorithm,
                      public_key,
                      std::vector<uint8>(kMaxMsgSizeBytes + 1, '0'),
                      &encrypted_data));

  // Generate encrypted data.
  EXPECT_STATUS(Status::Success(),
      EncryptInternal(algorithm, public_key, message_hex, &encrypted_data));

  // Fail decrypt with a public key.
  blink::WebArrayBuffer decrypted_data;
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(), DecryptInternal(
      algorithm,
      public_key,
      reinterpret_cast<const unsigned char*>(encrypted_data.data()),
      encrypted_data.byteLength(),
      &decrypted_data));

  // Corrupt encrypted data; ensure decrypt fails because padding was disrupted.
  std::vector<uint8> corrupted_data(
      static_cast<uint8*>(encrypted_data.data()),
      static_cast<uint8*>(encrypted_data.data()) + encrypted_data.byteLength());
  corrupted_data[corrupted_data.size() / 2] ^= 0x01;
 EXPECT_STATUS(Status::Error(),
      DecryptInternal(algorithm, private_key, corrupted_data, &decrypted_data));

  // TODO(padolph): Are there other specific data corruption scenarios to
  // consider?

  // Do a successful decrypt with good data just for confirmation.
  EXPECT_STATUS_SUCCESS(DecryptInternal(
      algorithm,
      private_key,
      reinterpret_cast<const unsigned char*>(encrypted_data.data()),
      encrypted_data.byteLength(),
      &decrypted_data));
  ExpectArrayBufferMatchesHex(message_hex_str, decrypted_data);
}

TEST_F(WebCryptoImplTest, MAYBE(RsaSsaSignVerifyFailures)) {
  // Import a key pair.
  blink::WebCryptoAlgorithm algorithm = CreateRsaAlgorithmWithInnerHash(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::WebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      algorithm,
      false,
      blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify,
      &public_key,
      &private_key);

  blink::WebArrayBuffer signature;
  bool signature_match;

  // Compute a signature.
  const std::vector<uint8> data = HexStringToBytes("010203040506070809");
  ASSERT_STATUS_SUCCESS(SignInternal(algorithm, private_key, data, &signature));

  // Ensure truncated signature does not verify by passing one less byte.
  EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
      algorithm,
      public_key,
      static_cast<const unsigned char*>(signature.data()),
      signature.byteLength() - 1,
      data,
      &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure truncated signature does not verify by passing no bytes.
  EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
      algorithm,
      public_key,
      NULL,
      0,
      data,
      &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure corrupted signature does not verify.
  std::vector<uint8> corrupt_sig(
      static_cast<uint8*>(signature.data()),
      static_cast<uint8*>(signature.data()) + signature.byteLength());
  corrupt_sig[corrupt_sig.size() / 2] ^= 0x1;
  EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
      algorithm,
      public_key,
      webcrypto::Uint8VectorStart(corrupt_sig),
      corrupt_sig.size(),
      data,
      &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure signatures that are greater than the modulus size fail.
  const unsigned int long_message_size_bytes = 1024;
  DCHECK_GT(long_message_size_bytes, kModulusLength/8);
  const unsigned char kLongSignature[long_message_size_bytes] = { 0 };
  EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
      algorithm,
      public_key,
      kLongSignature,
      sizeof(kLongSignature),
      data,
      &signature_match));
  EXPECT_FALSE(signature_match);

  // Ensure that verifying using a private key, rather than a public key, fails.
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(), VerifySignatureInternal(
      algorithm,
      private_key,
      static_cast<const unsigned char*>(signature.data()),
      signature.byteLength(),
      data,
      &signature_match));

  // Ensure that signing using a public key, rather than a private key, fails.
  EXPECT_STATUS(Status::ErrorUnexpectedKeyType(),
      SignInternal(algorithm, public_key, data, &signature));

  // Ensure that signing and verifying with an incompatible algorithm fails.
  algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5);
  EXPECT_STATUS(Status::ErrorUnsupported(),
      SignInternal(algorithm, private_key, data, &signature));
  EXPECT_STATUS(Status::ErrorUnsupported(), VerifySignatureInternal(
      algorithm,
      public_key,
      static_cast<const unsigned char*>(signature.data()),
      signature.byteLength(),
      data,
      &signature_match));

  // Some crypto libraries (NSS) can automatically select the RSA SSA inner hash
  // based solely on the contents of the input signature data. In the Web Crypto
  // implementation, the inner hash should be specified uniquely by the input
  // algorithm parameter. To validate this behavior, call Verify with a computed
  // signature that used one hash type (SHA-1), but pass in an algorithm with a
  // different inner hash type (SHA-256). If the hash type is determined by the
  // signature itself (undesired), the verify will pass, while if the hash type
  // is specified by the input algorithm (desired), the verify will fail.

  // Compute a signature using SHA-1 as the inner hash.
  EXPECT_STATUS_SUCCESS(SignInternal(CreateRsaAlgorithmWithInnerHash(
                               blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
                               blink::WebCryptoAlgorithmIdSha1),
                           private_key,
                           data,
                           &signature));

  // Now verify using an algorithm whose inner hash is SHA-256, not SHA-1. The
  // signature should not verify.
  // NOTE: public_key was produced by generateKey, and so its associated
  // algorithm has WebCryptoRsaKeyGenParams and not WebCryptoRsaSsaParams. Thus
  // it has no inner hash to conflict with the input algorithm.
  bool is_match;
  EXPECT_STATUS_SUCCESS(VerifySignatureInternal(
      CreateRsaAlgorithmWithInnerHash(
          blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
          blink::WebCryptoAlgorithmIdSha256),
      public_key,
      static_cast<const unsigned char*>(signature.data()),
      signature.byteLength(),
      data,
      &is_match));
  EXPECT_FALSE(is_match);
}

TEST_F(WebCryptoImplTest, MAYBE(RsaSignVerifyKnownAnswer)) {
  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("pkcs1v15_sign.json", &tests));

  // Import the key pair.
  blink::WebCryptoAlgorithm algorithm = CreateRsaAlgorithmWithInnerHash(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      blink::WebCryptoAlgorithmIdSha1);
  blink::WebCryptoKey public_key = blink::WebCryptoKey::createNull();
  blink::WebCryptoKey private_key = blink::WebCryptoKey::createNull();
  ImportRsaKeyPair(
      HexStringToBytes(kPublicKeySpkiDerHex),
      HexStringToBytes(kPrivateKeyPkcs8DerHex),
      algorithm,
      false,
      blink::WebCryptoKeyUsageSign | blink::WebCryptoKeyUsageVerify,
      &public_key,
      &private_key);

  // Validate the signatures are computed and verified as expected.
  blink::WebArrayBuffer signature;
  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);

    base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    std::vector<uint8> test_message =
        GetBytesFromHexString(test, "message_hex");
    std::vector<uint8> test_signature =
        GetBytesFromHexString(test, "signature_hex");

    signature.reset();
    ASSERT_STATUS_SUCCESS(
        SignInternal(algorithm, private_key, test_message, &signature));
    ExpectArrayBufferMatches(test_signature, signature);

    bool is_match = false;
    ASSERT_STATUS_SUCCESS(VerifySignatureInternal(
        algorithm,
        public_key,
        test_signature,
        test_message,
        &is_match));
    EXPECT_TRUE(is_match);
  }
}

TEST_F(WebCryptoImplTest, MAYBE(AesKwKeyImport)) {
  blink::WebCryptoKey key = blink::WebCryptoKey::createNull();
  blink::WebCryptoAlgorithm algorithm =
      webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesKw);

  // Import a 128-bit Key Encryption Key (KEK)
  std::string key_raw_hex_in = "025a8cf3f08b4f6c5f33bbc76a471939";
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                          HexStringToBytes(key_raw_hex_in),
                                          algorithm,
                                          true,
                                          blink::WebCryptoKeyUsageWrapKey,
                                          &key));
  blink::WebArrayBuffer key_raw_out;
  EXPECT_STATUS_SUCCESS(ExportKeyInternal(blink::WebCryptoKeyFormatRaw,
                        key,
                        &key_raw_out));
  ExpectArrayBufferMatchesHex(key_raw_hex_in, key_raw_out);

  // Import a 192-bit KEK
  key_raw_hex_in = "c0192c6466b2370decbb62b2cfef4384544ffeb4d2fbc103";
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                          HexStringToBytes(key_raw_hex_in),
                                          algorithm,
                                          true,
                                          blink::WebCryptoKeyUsageWrapKey,
                                          &key));
  EXPECT_STATUS_SUCCESS(ExportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                          key,
                                          &key_raw_out));
  ExpectArrayBufferMatchesHex(key_raw_hex_in, key_raw_out);

  // Import a 256-bit Key Encryption Key (KEK)
  key_raw_hex_in =
      "e11fe66380d90fa9ebefb74e0478e78f95664d0c67ca20ce4a0b5842863ac46f";
  ASSERT_STATUS_SUCCESS(ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                          HexStringToBytes(key_raw_hex_in),
                                          algorithm,
                                          true,
                                          blink::WebCryptoKeyUsageWrapKey,
                                          &key));
  EXPECT_STATUS_SUCCESS(ExportKeyInternal(blink::WebCryptoKeyFormatRaw,
                                          key,
                                          &key_raw_out));
  ExpectArrayBufferMatchesHex(key_raw_hex_in, key_raw_out);

  // Fail import of 0 length key
  EXPECT_STATUS(Status::Error(),
      ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                        HexStringToBytes(""),
                        algorithm,
                        true,
                        blink::WebCryptoKeyUsageWrapKey,
                        &key));

  // Fail import of 124-bit KEK
  key_raw_hex_in = "3e4566a2bdaa10cb68134fa66c15ddb";
  EXPECT_STATUS(Status::Error(),
      ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                        HexStringToBytes(key_raw_hex_in),
                        algorithm,
                        true,
                        blink::WebCryptoKeyUsageWrapKey,
                        &key));

  // Fail import of 200-bit KEK
  key_raw_hex_in = "0a1d88608a5ad9fec64f1ada269ebab4baa2feeb8d95638c0e";
  EXPECT_STATUS(Status::Error(),
        ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                          HexStringToBytes(key_raw_hex_in),
                          algorithm,
                          true,
                          blink::WebCryptoKeyUsageWrapKey,
                          &key));

  // Fail import of 260-bit KEK
  key_raw_hex_in =
      "72d4e475ff34215416c9ad9c8281247a4d730c5f275ac23f376e73e3bce8d7d5a";
  EXPECT_STATUS(Status::Error(),
      ImportKeyInternal(blink::WebCryptoKeyFormatRaw,
                        HexStringToBytes(key_raw_hex_in),
                        algorithm,
                        true,
                        blink::WebCryptoKeyUsageWrapKey,
                        &key));
}

// TODO(eroman):
//   * Test decryption when the tag length exceeds input size
//   * Test decryption with empty input
//   * Test decryption with tag length of 0.
TEST_F(WebCryptoImplTest, MAYBE(AesGcmSampleSets)) {
  // Some Linux test runners may not have a new enough version of NSS.
  if (!SupportsAesGcm()) {
    LOG(WARNING) << "AES GCM not supported, skipping tests";
    return;
  }

  scoped_ptr<base::ListValue> tests;
  ASSERT_TRUE(ReadJsonTestFileToList("aes_gcm.json", &tests));

  // Note that WebCrypto appends the authentication tag to the ciphertext.
  for (size_t test_index = 0; test_index < tests->GetSize(); ++test_index) {
    SCOPED_TRACE(test_index);
    base::DictionaryValue* test;
    ASSERT_TRUE(tests->GetDictionary(test_index, &test));

    const std::vector<uint8> test_key = GetBytesFromHexString(test, "key");
    const std::vector<uint8> test_iv = GetBytesFromHexString(test, "iv");
    const std::vector<uint8> test_additional_data =
        GetBytesFromHexString(test, "additional_data");
    const std::vector<uint8> test_plain_text =
        GetBytesFromHexString(test, "plain_text");
    const std::vector<uint8> test_authentication_tag =
        GetBytesFromHexString(test, "authentication_tag");
    const unsigned int test_tag_size_bits = test_authentication_tag.size() * 8;
    const std::vector<uint8> test_cipher_text =
        GetBytesFromHexString(test, "cipher_text");

    blink::WebCryptoKey key = ImportSecretKeyFromRaw(
        test_key,
        webcrypto::CreateAlgorithm(blink::WebCryptoAlgorithmIdAesGcm),
        blink::WebCryptoKeyUsageEncrypt | blink::WebCryptoKeyUsageDecrypt);

    // Verify exported raw key is identical to the imported data
    blink::WebArrayBuffer raw_key;
    EXPECT_STATUS_SUCCESS(ExportKeyInternal(
        blink::WebCryptoKeyFormatRaw, key, &raw_key));

    ExpectArrayBufferMatches(test_key, raw_key);

    // Test encryption.
    std::vector<uint8> cipher_text;
    std::vector<uint8> authentication_tag;
    EXPECT_STATUS_SUCCESS(AesGcmEncrypt(key, test_iv, test_additional_data,
                                        test_tag_size_bits, test_plain_text,
                                        &cipher_text, &authentication_tag));

    ExpectVectorMatches(test_cipher_text, cipher_text);
    ExpectVectorMatches(test_authentication_tag, authentication_tag);

    // Test decryption.
    blink::WebArrayBuffer plain_text;
    EXPECT_STATUS_SUCCESS(AesGcmDecrypt(key, test_iv, test_additional_data,
                                        test_tag_size_bits, test_cipher_text,
                                        test_authentication_tag, &plain_text));
    ExpectArrayBufferMatches(test_plain_text, plain_text);

    // Decryption should fail if any of the inputs are tampered with.
    EXPECT_STATUS(Status::Error(),
        AesGcmDecrypt(key, Corrupted(test_iv), test_additional_data,
                      test_tag_size_bits, test_cipher_text,
                      test_authentication_tag, &plain_text));
    EXPECT_STATUS(Status::Error(),
        AesGcmDecrypt(key, test_iv, Corrupted(test_additional_data),
                      test_tag_size_bits, test_cipher_text,
                      test_authentication_tag, &plain_text));
    EXPECT_STATUS(Status::Error(),
        AesGcmDecrypt(key, test_iv, test_additional_data,
                      test_tag_size_bits, Corrupted(test_cipher_text),
                      test_authentication_tag, &plain_text));
    EXPECT_STATUS(Status::Error(),
        AesGcmDecrypt(key, test_iv, test_additional_data,
                      test_tag_size_bits, test_cipher_text,
                      Corrupted(test_authentication_tag),
                      &plain_text));

    // Try different incorrect tag lengths
    uint8 kAlternateTagLengths[] = {8, 96, 120, 128, 160, 255};
    for (size_t tag_i = 0; tag_i < arraysize(kAlternateTagLengths); ++tag_i) {
      unsigned int wrong_tag_size_bits = kAlternateTagLengths[tag_i];
      if (test_tag_size_bits == wrong_tag_size_bits)
        continue;
      EXPECT_STATUS_ERROR(AesGcmDecrypt(key, test_iv, test_additional_data,
                                        wrong_tag_size_bits, test_cipher_text,
                                        test_authentication_tag, &plain_text));
    }
  }
}

}  // namespace content
