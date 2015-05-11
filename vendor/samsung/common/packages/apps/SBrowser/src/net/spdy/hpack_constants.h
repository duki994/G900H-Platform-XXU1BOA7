// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_HPACK_CONSTANTS_H_
#define NET_SPDY_HPACK_CONSTANTS_H_

#include "base/basictypes.h"

// All section references below are to
// http://tools.ietf.org/html/draft-ietf-httpbis-header-compression-05
// .

namespace net {

// An HpackPrefix signifies |bits| stored in the top |bit_size| bits
// of an octet.
struct HpackPrefix {
  uint8 bits;
  size_t bit_size;
};

// The marker for a string literal that is stored unmodified (i.e.,
// without Huffman encoding) (from 4.1.2).
const HpackPrefix kStringLiteralIdentityEncoded = { 0x0, 1 };

// The opcode for an indexed header field (from 4.2).
const HpackPrefix kIndexedOpcode = { 0x1, 1 };

// The opcode for a literal header field without indexing (from
// 4.3.1).
const HpackPrefix kLiteralNoIndexOpcode = { 0x01, 2 };

// The opcode for a literal header field with incremental indexing
// (from 4.3.2).
const HpackPrefix kLiteralIncrementalIndexOpcode = { 0x00, 2 };

}  // namespace net

#endif  // NET_SPDY_HPACK_CONSTANTS_H_
