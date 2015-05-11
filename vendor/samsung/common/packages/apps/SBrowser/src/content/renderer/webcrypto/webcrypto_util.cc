// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_util.h"

#include "base/base64.h"
#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"

namespace content {

namespace webcrypto {

bool Status::IsError() const {
  return type_ == TYPE_ERROR;
}

bool Status::IsSuccess() const {
  return type_ == TYPE_SUCCESS;
}

bool Status::HasErrorDetails() const {
  return !error_details_.empty();
}

std::string Status::ToString() const {
  return IsSuccess() ? "Success" : error_details_;
}

Status Status::Success() {
  return Status(TYPE_SUCCESS);
}

Status Status::Error() {
  return Status(TYPE_ERROR);
}

Status Status::ErrorJwkNotDictionary() {
  return Status("JWK input could not be parsed to a JSON dictionary");
}

Status Status::ErrorJwkPropertyMissing(const std::string& property) {
  return Status("The required JWK property \"" + property + "\" was missing");
}

Status Status::ErrorJwkPropertyWrongType(const std::string& property,
                                         const std::string& expected_type) {
  return Status("The JWK property \"" + property + "\" must be a " +
                expected_type);
}

Status Status::ErrorJwkBase64Decode(const std::string& property) {
  return Status("The JWK property \"" + property +
                "\" could not be base64 decoded");
}

Status Status::ErrorJwkExtractableInconsistent() {
  return Status("The \"extractable\" property of the JWK dictionary is "
                "inconsistent what that specified by the Web Crypto call");
}

Status Status::ErrorJwkUnrecognizedAlgorithm() {
  return Status("The JWK \"alg\" property was not recognized");
}

Status Status::ErrorJwkAlgorithmInconsistent() {
  return Status("The JWK \"alg\" property was inconsistent with that specified "
                "by the Web Crypto call");
}

Status Status::ErrorJwkAlgorithmMissing() {
  return Status("The JWK optional \"alg\" property is missing or not a string, "
                "and one wasn't specified by the Web Crypto call");
}

Status Status::ErrorJwkUnrecognizedUsage() {
  return Status("The JWK \"use\" property could not be parsed");
}

Status Status::ErrorJwkUsageInconsistent() {
  return Status("The JWK \"use\" property was inconsistent with that specified "
                "by the Web Crypto call. The JWK usage must be a superset of "
                "those requested");
}

Status Status::ErrorJwkRsaPrivateKeyUnsupported() {
  return Status("JWK RSA key contained \"d\" property: Private key import is "
                "not yet supported");
}

Status Status::ErrorJwkUnrecognizedKty() {
  return Status("The JWK \"kty\" property was unrecognized");
}

Status Status::ErrorJwkIncorrectKeyLength() {
  return Status("The JWK \"k\" property did not include the right length "
                "of key data for the given algorithm.");
}

Status Status::ErrorImportEmptyKeyData() {
  return Status("No key data was provided");
}

Status Status::ErrorUnexpectedKeyType() {
  return Status("The key is not of the expected type");
}

Status Status::ErrorIncorrectSizeAesCbcIv() {
  return Status("The \"iv\" has an unexpected length -- must be 16 bytes");
}

Status Status::ErrorDataTooLarge() {
  return Status("The provided data is too large");
}

Status Status::ErrorUnsupported() {
  return Status("The requested operation is unsupported");
}

Status Status::ErrorUnexpected() {
  return Status("Something unexpected happened...");
}

Status Status::ErrorInvalidAesGcmTagLength() {
  return Status("The tag length is invalid: either too large or not a multiple "
                "of 8 bits");
}

Status Status::ErrorGenerateKeyPublicExponent() {
  return Status("The \"publicExponent\" is either empty, zero, or too large");
}

Status Status::ErrorMissingAlgorithmImportRawKey() {
  return Status("The key's algorithm must be specified when importing "
                "raw-formatted key.");
}

Status Status::ErrorImportRsaEmptyModulus() {
  return Status("The modulus is empty");
}

Status Status::ErrorGenerateRsaZeroModulus() {
  return Status("The modulus bit length cannot be zero");
}

Status Status::ErrorImportRsaEmptyExponent() {
  return Status("No bytes for the exponent were provided");
}

Status Status::ErrorKeyNotExtractable() {
  return Status("They key is not extractable");
}

Status Status::ErrorGenerateKeyLength() {
  return Status("Invalid key length: it is either zero or not a multiple of 8 "
                "bits");
}

Status::Status(const std::string& error_details_utf8)
    : type_(TYPE_ERROR), error_details_(error_details_utf8) {}

Status::Status(Type type) : type_(type) {}

const uint8* Uint8VectorStart(const std::vector<uint8>& data) {
  if (data.empty())
    return NULL;
  return &data[0];
}

void ShrinkBuffer(blink::WebArrayBuffer* buffer, unsigned int new_size) {
  DCHECK_LE(new_size, buffer->byteLength());

  if (new_size == buffer->byteLength())
    return;

  blink::WebArrayBuffer new_buffer = blink::WebArrayBuffer::create(new_size, 1);
  DCHECK(!new_buffer.isNull());
  memcpy(new_buffer.data(), buffer->data(), new_size);
  *buffer = new_buffer;
}

blink::WebArrayBuffer CreateArrayBuffer(const uint8* data,
                                        unsigned int data_size) {
  blink::WebArrayBuffer buffer = blink::WebArrayBuffer::create(data_size, 1);
  DCHECK(!buffer.isNull());
  if (data_size)  // data_size == 0 might mean the data pointer is invalid
    memcpy(buffer.data(), data, data_size);
  return buffer;
}

// This function decodes unpadded 'base64url' encoded data, as described in
// RFC4648 (http://www.ietf.org/rfc/rfc4648.txt) Section 5. To do this, first
// change the incoming data to 'base64' encoding by applying the appropriate
// transformation including adding padding if required, and then call a base64
// decoder.
bool Base64DecodeUrlSafe(const std::string& input, std::string* output) {
  std::string base64EncodedText(input);
  std::replace(base64EncodedText.begin(), base64EncodedText.end(), '-', '+');
  std::replace(base64EncodedText.begin(), base64EncodedText.end(), '_', '/');
  base64EncodedText.append((4 - base64EncodedText.size() % 4) % 4, '=');
  return base::Base64Decode(base64EncodedText, output);
}

bool IsHashAlgorithm(blink::WebCryptoAlgorithmId alg_id) {
  return alg_id == blink::WebCryptoAlgorithmIdSha1 ||
         alg_id == blink::WebCryptoAlgorithmIdSha224 ||
         alg_id == blink::WebCryptoAlgorithmIdSha256 ||
         alg_id == blink::WebCryptoAlgorithmIdSha384 ||
         alg_id == blink::WebCryptoAlgorithmIdSha512;
}

blink::WebCryptoAlgorithm GetInnerHashAlgorithm(
    const blink::WebCryptoAlgorithm& algorithm) {
  DCHECK(!algorithm.isNull());
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac:
      if (algorithm.hmacParams())
        return algorithm.hmacParams()->hash();
      else if (algorithm.hmacKeyParams())
        return algorithm.hmacKeyParams()->hash();
      break;
    case blink::WebCryptoAlgorithmIdRsaOaep:
      if (algorithm.rsaOaepParams())
        return algorithm.rsaOaepParams()->hash();
      break;
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      if (algorithm.rsaSsaParams())
        return algorithm.rsaSsaParams()->hash();
      break;
    default:
      break;
  }
  return blink::WebCryptoAlgorithm::createNull();
}

blink::WebCryptoAlgorithm CreateAlgorithm(blink::WebCryptoAlgorithmId id) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(id, NULL);
}

blink::WebCryptoAlgorithm CreateHmacAlgorithmByHashId(
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(IsHashAlgorithm(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdHmac,
      new blink::WebCryptoHmacParams(CreateAlgorithm(hash_id)));
}

blink::WebCryptoAlgorithm CreateHmacKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId hash_id,
    unsigned int key_length_bytes) {
  DCHECK(IsHashAlgorithm(hash_id));
  // key_length_bytes == 0 means unspecified
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdHmac,
      new blink::WebCryptoHmacKeyParams(
          CreateAlgorithm(hash_id), (key_length_bytes != 0), key_length_bytes));
}

blink::WebCryptoAlgorithm CreateRsaSsaAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(IsHashAlgorithm(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5,
      new blink::WebCryptoRsaSsaParams(CreateAlgorithm(hash_id)));
}

blink::WebCryptoAlgorithm CreateRsaOaepAlgorithm(
    blink::WebCryptoAlgorithmId hash_id) {
  DCHECK(IsHashAlgorithm(hash_id));
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdRsaOaep,
      new blink::WebCryptoRsaOaepParams(
          CreateAlgorithm(hash_id), false, NULL, 0));
}

blink::WebCryptoAlgorithm CreateRsaKeyGenAlgorithm(
    blink::WebCryptoAlgorithmId algorithm_id,
    unsigned int modulus_length,
    const std::vector<uint8>& public_exponent) {
  DCHECK(algorithm_id == blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5 ||
         algorithm_id == blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5 ||
         algorithm_id == blink::WebCryptoAlgorithmIdRsaOaep);
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      algorithm_id,
      new blink::WebCryptoRsaKeyGenParams(
          modulus_length,
          webcrypto::Uint8VectorStart(public_exponent),
          public_exponent.size()));
}

blink::WebCryptoAlgorithm CreateAesCbcAlgorithm(const std::vector<uint8>& iv) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdAesCbc,
      new blink::WebCryptoAesCbcParams(Uint8VectorStart(iv), iv.size()));
}

blink::WebCryptoAlgorithm CreateAesGcmAlgorithm(
    const std::vector<uint8>& iv,
    const std::vector<uint8>& additional_data,
    uint8 tag_length_bytes) {
  return blink::WebCryptoAlgorithm::adoptParamsAndCreate(
      blink::WebCryptoAlgorithmIdAesCbc,
      new blink::WebCryptoAesGcmParams(Uint8VectorStart(iv),
                                       iv.size(),
                                       additional_data.size() != 0,
                                       Uint8VectorStart(additional_data),
                                       additional_data.size(),
                                       tag_length_bytes != 0,
                                       tag_length_bytes));
}

unsigned int ShaBlockSizeBytes(blink::WebCryptoAlgorithmId hash_id) {
  switch (hash_id) {
    case blink::WebCryptoAlgorithmIdSha1:
    case blink::WebCryptoAlgorithmIdSha224:
    case blink::WebCryptoAlgorithmIdSha256:
      return 64;
    case blink::WebCryptoAlgorithmIdSha384:
    case blink::WebCryptoAlgorithmIdSha512:
      return 128;
    default:
      NOTREACHED();
      return 0;
  }
}

}  // namespace webcrypto

}  // namespace content
