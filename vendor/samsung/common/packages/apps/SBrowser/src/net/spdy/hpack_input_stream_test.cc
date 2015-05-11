// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_input_stream.h"

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using base::StringPiece;
using std::string;

// Utility function to decode an assumed-valid uint32 with an N-bit
// prefix.
uint32 DecodeValidUint32(uint8 N, StringPiece str) {
  EXPECT_GT(N, 0);
  EXPECT_LE(N, 8);
  HpackInputStream input_stream(kuint32max, str);
  input_stream.SetBitOffsetForTest(8 - N);
  uint32 I;
  EXPECT_TRUE(input_stream.DecodeNextUint32ForTest(&I));
  return I;
}

// Utility function to decode an assumed-invalid uint32 with an N-bit
// prefix.
void ExpectDecodeUint32Invalid(uint8 N, StringPiece str) {
  EXPECT_GT(N, 0);
  EXPECT_LE(N, 8);
  HpackInputStream input_stream(kuint32max, str);
  input_stream.SetBitOffsetForTest(8 - N);
  uint32 I;
  EXPECT_FALSE(input_stream.DecodeNextUint32ForTest(&I));
}

// The {Number}ByteIntegersEightBitPrefix tests below test that
// certain integers are decoded correctly with an 8-bit prefix in
// exactly {Number} bytes.

TEST(HpackInputStreamTest, OneByteIntegersEightBitPrefix) {
  // Minimum.
  EXPECT_EQ(0x00u, DecodeValidUint32(8, string("\x00", 1)));
  EXPECT_EQ(0x7fu, DecodeValidUint32(8, "\x7f"));
  // Maximum.
  EXPECT_EQ(0xfeu, DecodeValidUint32(8, "\xfe"));
  // Invalid.
  ExpectDecodeUint32Invalid(8, "\xff");
}

TEST(HpackInputStreamTest, TwoByteIntegersEightBitPrefix) {
  // Minimum.
  EXPECT_EQ(0xffu, DecodeValidUint32(8, string("\xff\x00", 2)));
  EXPECT_EQ(0x0100u, DecodeValidUint32(8, "\xff\x01"));
  // Maximum.
  EXPECT_EQ(0x017eu, DecodeValidUint32(8, "\xff\x7f"));
  // Invalid.
  ExpectDecodeUint32Invalid(8, "\xff\x80");
  ExpectDecodeUint32Invalid(8, "\xff\xff");
}

TEST(HpackInputStreamTest, ThreeByteIntegersEightBitPrefix) {
  // Minimum.
  EXPECT_EQ(0x017fu, DecodeValidUint32(8, "\xff\x80\x01"));
  EXPECT_EQ(0x0fffu, DecodeValidUint32(8, "\xff\x80\x1e"));
  // Maximum.
  EXPECT_EQ(0x40feu, DecodeValidUint32(8, "\xff\xff\x7f"));
  // Invalid.
  ExpectDecodeUint32Invalid(8, "\xff\x80\x00");
  ExpectDecodeUint32Invalid(8, "\xff\xff\x00");
  ExpectDecodeUint32Invalid(8, "\xff\xff\x80");
  ExpectDecodeUint32Invalid(8, "\xff\xff\xff");
}

TEST(HpackInputStreamTest, FourByteIntegersEightBitPrefix) {
  // Minimum.
  EXPECT_EQ(0x40ffu, DecodeValidUint32(8, "\xff\x80\x80\x01"));
  EXPECT_EQ(0xffffu, DecodeValidUint32(8, "\xff\x80\xfe\x03"));
  // Maximum.
  EXPECT_EQ(0x002000feu, DecodeValidUint32(8, "\xff\xff\xff\x7f"));
  // Invalid.
  ExpectDecodeUint32Invalid(8, "\xff\xff\x80\x00");
  ExpectDecodeUint32Invalid(8, "\xff\xff\xff\x00");
  ExpectDecodeUint32Invalid(8, "\xff\xff\xff\x80");
  ExpectDecodeUint32Invalid(8, "\xff\xff\xff\xff");
}

TEST(HpackInputStreamTest, FiveByteIntegersEightBitPrefix) {
  // Minimum.
  EXPECT_EQ(0x002000ffu, DecodeValidUint32(8, "\xff\x80\x80\x80\x01"));
  EXPECT_EQ(0x00ffffffu, DecodeValidUint32(8, "\xff\x80\xfe\xff\x07"));
  // Maximum.
  EXPECT_EQ(0x100000feu, DecodeValidUint32(8, "\xff\xff\xff\xff\x7f"));
  // Invalid.
  ExpectDecodeUint32Invalid(8, "\xff\xff\xff\x80\x00");
  ExpectDecodeUint32Invalid(8, "\xff\xff\xff\xff\x00");
  ExpectDecodeUint32Invalid(8, "\xff\xff\xff\xff\x80");
  ExpectDecodeUint32Invalid(8, "\xff\xff\xff\xff\xff");
}

TEST(HpackInputStreamTest, SixByteIntegersEightBitPrefix) {
  // Minimum.
  EXPECT_EQ(0x100000ffu, DecodeValidUint32(8, "\xff\x80\x80\x80\x80\x01"));
  // Maximum.
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(8, "\xff\x80\xfe\xff\xff\x0f"));
  // Invalid.
  ExpectDecodeUint32Invalid(8, "\xff\x80\x80\x80\x80\x00");
  ExpectDecodeUint32Invalid(8, "\xff\x80\xfe\xff\xff\x10");
  ExpectDecodeUint32Invalid(8, "\xff\xff\xff\xff\xff\xff");
}

// There are no valid uint32 encodings that are greater than six
// bytes.
TEST(HpackInputStreamTest, SevenByteIntegersEightBitPrefix) {
  ExpectDecodeUint32Invalid(8, "\xff\x80\x80\x80\x80\x80\x00");
  ExpectDecodeUint32Invalid(8, "\xff\x80\x80\x80\x80\x80\x01");
  ExpectDecodeUint32Invalid(8, "\xff\xff\xff\xff\xff\xff\xff");
}

// The {Number}ByteIntegersOneToSevenBitPrefix tests below test that
// certain integers are encoded correctly with an N-bit prefix in
// exactly {Number} bytes for N in {1, 2, ..., 7}.

TEST(HpackInputStreamTest, OneByteIntegersOneToSevenBitPrefixes) {
  // Minimums.
  EXPECT_EQ(0x00u, DecodeValidUint32(7, string("\x00", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(7, string("\x80", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(6, string("\x00", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(6, string("\xc0", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(5, string("\x00", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(5, string("\xe0", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(4, string("\x00", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(4, string("\xf0", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(3, string("\x00", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(3, string("\xf8", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(2, string("\x00", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(2, string("\xfc", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(1, string("\x00", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(1, string("\xfe", 1)));

  // Maximums.
  EXPECT_EQ(0x7eu, DecodeValidUint32(7, "\x7e"));
  EXPECT_EQ(0x7eu, DecodeValidUint32(7, "\xfe"));
  EXPECT_EQ(0x3eu, DecodeValidUint32(6, "\x3e"));
  EXPECT_EQ(0x3eu, DecodeValidUint32(6, "\xfe"));
  EXPECT_EQ(0x1eu, DecodeValidUint32(5, "\x1e"));
  EXPECT_EQ(0x1eu, DecodeValidUint32(5, "\xfe"));
  EXPECT_EQ(0x0eu, DecodeValidUint32(4, "\x0e"));
  EXPECT_EQ(0x0eu, DecodeValidUint32(4, "\xfe"));
  EXPECT_EQ(0x06u, DecodeValidUint32(3, "\x06"));
  EXPECT_EQ(0x06u, DecodeValidUint32(3, "\xfe"));
  EXPECT_EQ(0x02u, DecodeValidUint32(2, "\x02"));
  EXPECT_EQ(0x02u, DecodeValidUint32(2, "\xfe"));
  EXPECT_EQ(0x00u, DecodeValidUint32(1, string("\x00", 1)));
  EXPECT_EQ(0x00u, DecodeValidUint32(1, string("\xfe", 1)));

  // Invalid.
  ExpectDecodeUint32Invalid(7, "\x7f");
  ExpectDecodeUint32Invalid(7, "\xff");
  ExpectDecodeUint32Invalid(6, "\x3f");
  ExpectDecodeUint32Invalid(6, "\xff");
  ExpectDecodeUint32Invalid(5, "\x1f");
  ExpectDecodeUint32Invalid(5, "\xff");
  ExpectDecodeUint32Invalid(4, "\x0f");
  ExpectDecodeUint32Invalid(4, "\xff");
  ExpectDecodeUint32Invalid(3, "\x07");
  ExpectDecodeUint32Invalid(3, "\xff");
  ExpectDecodeUint32Invalid(2, "\x03");
  ExpectDecodeUint32Invalid(2, "\xff");
  ExpectDecodeUint32Invalid(1, "\x01");
  ExpectDecodeUint32Invalid(1, "\xff");
}

TEST(HpackInputStreamTest, TwoByteIntegersOneToSevenBitPrefixes) {
  // Minimums.
  EXPECT_EQ(0x7fu, DecodeValidUint32(7, string("\x7f\x00", 2)));
  EXPECT_EQ(0x7fu, DecodeValidUint32(7, string("\xff\x00", 2)));
  EXPECT_EQ(0x3fu, DecodeValidUint32(6, string("\x3f\x00", 2)));
  EXPECT_EQ(0x3fu, DecodeValidUint32(6, string("\xff\x00", 2)));
  EXPECT_EQ(0x1fu, DecodeValidUint32(5, string("\x1f\x00", 2)));
  EXPECT_EQ(0x1fu, DecodeValidUint32(5, string("\xff\x00", 2)));
  EXPECT_EQ(0x0fu, DecodeValidUint32(4, string("\x0f\x00", 2)));
  EXPECT_EQ(0x0fu, DecodeValidUint32(4, string("\xff\x00", 2)));
  EXPECT_EQ(0x07u, DecodeValidUint32(3, string("\x07\x00", 2)));
  EXPECT_EQ(0x07u, DecodeValidUint32(3, string("\xff\x00", 2)));
  EXPECT_EQ(0x03u, DecodeValidUint32(2, string("\x03\x00", 2)));
  EXPECT_EQ(0x03u, DecodeValidUint32(2, string("\xff\x00", 2)));
  EXPECT_EQ(0x01u, DecodeValidUint32(1, string("\x01\x00", 2)));
  EXPECT_EQ(0x01u, DecodeValidUint32(1, string("\xff\x00", 2)));

  // Maximums.
  EXPECT_EQ(0xfeu, DecodeValidUint32(7, "\x7f\x7f"));
  EXPECT_EQ(0xfeu, DecodeValidUint32(7, "\xff\x7f"));
  EXPECT_EQ(0xbeu, DecodeValidUint32(6, "\x3f\x7f"));
  EXPECT_EQ(0xbeu, DecodeValidUint32(6, "\xff\x7f"));
  EXPECT_EQ(0x9eu, DecodeValidUint32(5, "\x1f\x7f"));
  EXPECT_EQ(0x9eu, DecodeValidUint32(5, "\xff\x7f"));
  EXPECT_EQ(0x8eu, DecodeValidUint32(4, "\x0f\x7f"));
  EXPECT_EQ(0x8eu, DecodeValidUint32(4, "\xff\x7f"));
  EXPECT_EQ(0x86u, DecodeValidUint32(3, "\x07\x7f"));
  EXPECT_EQ(0x86u, DecodeValidUint32(3, "\xff\x7f"));
  EXPECT_EQ(0x82u, DecodeValidUint32(2, "\x03\x7f"));
  EXPECT_EQ(0x82u, DecodeValidUint32(2, "\xff\x7f"));
  EXPECT_EQ(0x80u, DecodeValidUint32(1, "\x01\x7f"));
  EXPECT_EQ(0x80u, DecodeValidUint32(1, "\xff\x7f"));

  // Invalid.
  ExpectDecodeUint32Invalid(7, "\x7f\x80");
  ExpectDecodeUint32Invalid(7, "\xff\xff");
  ExpectDecodeUint32Invalid(6, "\x3f\x80");
  ExpectDecodeUint32Invalid(6, "\xff\xff");
  ExpectDecodeUint32Invalid(5, "\x1f\x80");
  ExpectDecodeUint32Invalid(5, "\xff\xff");
  ExpectDecodeUint32Invalid(4, "\x0f\x80");
  ExpectDecodeUint32Invalid(4, "\xff\xff");
  ExpectDecodeUint32Invalid(3, "\x07\x80");
  ExpectDecodeUint32Invalid(3, "\xff\xff");
  ExpectDecodeUint32Invalid(2, "\x03\x80");
  ExpectDecodeUint32Invalid(2, "\xff\xff");
  ExpectDecodeUint32Invalid(1, "\x01\x80");
  ExpectDecodeUint32Invalid(1, "\xff\xff");
}

TEST(HpackInputStreamTest, ThreeByteIntegersOneToSevenBitPrefixes) {
  // Minimums.
  EXPECT_EQ(0xffu, DecodeValidUint32(7, "\x7f\x80\x01"));
  EXPECT_EQ(0xffu, DecodeValidUint32(7, "\xff\x80\x01"));
  EXPECT_EQ(0xbfu, DecodeValidUint32(6, "\x3f\x80\x01"));
  EXPECT_EQ(0xbfu, DecodeValidUint32(6, "\xff\x80\x01"));
  EXPECT_EQ(0x9fu, DecodeValidUint32(5, "\x1f\x80\x01"));
  EXPECT_EQ(0x9fu, DecodeValidUint32(5, "\xff\x80\x01"));
  EXPECT_EQ(0x8fu, DecodeValidUint32(4, "\x0f\x80\x01"));
  EXPECT_EQ(0x8fu, DecodeValidUint32(4, "\xff\x80\x01"));
  EXPECT_EQ(0x87u, DecodeValidUint32(3, "\x07\x80\x01"));
  EXPECT_EQ(0x87u, DecodeValidUint32(3, "\xff\x80\x01"));
  EXPECT_EQ(0x83u, DecodeValidUint32(2, "\x03\x80\x01"));
  EXPECT_EQ(0x83u, DecodeValidUint32(2, "\xff\x80\x01"));
  EXPECT_EQ(0x81u, DecodeValidUint32(1, "\x01\x80\x01"));
  EXPECT_EQ(0x81u, DecodeValidUint32(1, "\xff\x80\x01"));

  // Maximums.
  EXPECT_EQ(0x407eu, DecodeValidUint32(7, "\x7f\xff\x7f"));
  EXPECT_EQ(0x407eu, DecodeValidUint32(7, "\xff\xff\x7f"));
  EXPECT_EQ(0x403eu, DecodeValidUint32(6, "\x3f\xff\x7f"));
  EXPECT_EQ(0x403eu, DecodeValidUint32(6, "\xff\xff\x7f"));
  EXPECT_EQ(0x401eu, DecodeValidUint32(5, "\x1f\xff\x7f"));
  EXPECT_EQ(0x401eu, DecodeValidUint32(5, "\xff\xff\x7f"));
  EXPECT_EQ(0x400eu, DecodeValidUint32(4, "\x0f\xff\x7f"));
  EXPECT_EQ(0x400eu, DecodeValidUint32(4, "\xff\xff\x7f"));
  EXPECT_EQ(0x4006u, DecodeValidUint32(3, "\x07\xff\x7f"));
  EXPECT_EQ(0x4006u, DecodeValidUint32(3, "\xff\xff\x7f"));
  EXPECT_EQ(0x4002u, DecodeValidUint32(2, "\x03\xff\x7f"));
  EXPECT_EQ(0x4002u, DecodeValidUint32(2, "\xff\xff\x7f"));
  EXPECT_EQ(0x4000u, DecodeValidUint32(1, "\x01\xff\x7f"));
  EXPECT_EQ(0x4000u, DecodeValidUint32(1, "\xff\xff\x7f"));

  // Invalid.
  ExpectDecodeUint32Invalid(7, "\x7f\xff\x80");
  ExpectDecodeUint32Invalid(7, "\xff\xff\xff");
  ExpectDecodeUint32Invalid(6, "\x3f\xff\x80");
  ExpectDecodeUint32Invalid(6, "\xff\xff\xff");
  ExpectDecodeUint32Invalid(5, "\x1f\xff\x80");
  ExpectDecodeUint32Invalid(5, "\xff\xff\xff");
  ExpectDecodeUint32Invalid(4, "\x0f\xff\x80");
  ExpectDecodeUint32Invalid(4, "\xff\xff\xff");
  ExpectDecodeUint32Invalid(3, "\x07\xff\x80");
  ExpectDecodeUint32Invalid(3, "\xff\xff\xff");
  ExpectDecodeUint32Invalid(2, "\x03\xff\x80");
  ExpectDecodeUint32Invalid(2, "\xff\xff\xff");
  ExpectDecodeUint32Invalid(1, "\x01\xff\x80");
  ExpectDecodeUint32Invalid(1, "\xff\xff\xff");
}

TEST(HpackInputStreamTest, FourByteIntegersOneToSevenBitPrefixes) {
  // Minimums.
  EXPECT_EQ(0x407fu, DecodeValidUint32(7, "\x7f\x80\x80\x01"));
  EXPECT_EQ(0x407fu, DecodeValidUint32(7, "\xff\x80\x80\x01"));
  EXPECT_EQ(0x403fu, DecodeValidUint32(6, "\x3f\x80\x80\x01"));
  EXPECT_EQ(0x403fu, DecodeValidUint32(6, "\xff\x80\x80\x01"));
  EXPECT_EQ(0x401fu, DecodeValidUint32(5, "\x1f\x80\x80\x01"));
  EXPECT_EQ(0x401fu, DecodeValidUint32(5, "\xff\x80\x80\x01"));
  EXPECT_EQ(0x400fu, DecodeValidUint32(4, "\x0f\x80\x80\x01"));
  EXPECT_EQ(0x400fu, DecodeValidUint32(4, "\xff\x80\x80\x01"));
  EXPECT_EQ(0x4007u, DecodeValidUint32(3, "\x07\x80\x80\x01"));
  EXPECT_EQ(0x4007u, DecodeValidUint32(3, "\xff\x80\x80\x01"));
  EXPECT_EQ(0x4003u, DecodeValidUint32(2, "\x03\x80\x80\x01"));
  EXPECT_EQ(0x4003u, DecodeValidUint32(2, "\xff\x80\x80\x01"));
  EXPECT_EQ(0x4001u, DecodeValidUint32(1, "\x01\x80\x80\x01"));
  EXPECT_EQ(0x4001u, DecodeValidUint32(1, "\xff\x80\x80\x01"));

  // Maximums.
  EXPECT_EQ(0x20007eu, DecodeValidUint32(7, "\x7f\xff\xff\x7f"));
  EXPECT_EQ(0x20007eu, DecodeValidUint32(7, "\xff\xff\xff\x7f"));
  EXPECT_EQ(0x20003eu, DecodeValidUint32(6, "\x3f\xff\xff\x7f"));
  EXPECT_EQ(0x20003eu, DecodeValidUint32(6, "\xff\xff\xff\x7f"));
  EXPECT_EQ(0x20001eu, DecodeValidUint32(5, "\x1f\xff\xff\x7f"));
  EXPECT_EQ(0x20001eu, DecodeValidUint32(5, "\xff\xff\xff\x7f"));
  EXPECT_EQ(0x20000eu, DecodeValidUint32(4, "\x0f\xff\xff\x7f"));
  EXPECT_EQ(0x20000eu, DecodeValidUint32(4, "\xff\xff\xff\x7f"));
  EXPECT_EQ(0x200006u, DecodeValidUint32(3, "\x07\xff\xff\x7f"));
  EXPECT_EQ(0x200006u, DecodeValidUint32(3, "\xff\xff\xff\x7f"));
  EXPECT_EQ(0x200002u, DecodeValidUint32(2, "\x03\xff\xff\x7f"));
  EXPECT_EQ(0x200002u, DecodeValidUint32(2, "\xff\xff\xff\x7f"));
  EXPECT_EQ(0x200000u, DecodeValidUint32(1, "\x01\xff\xff\x7f"));
  EXPECT_EQ(0x200000u, DecodeValidUint32(1, "\xff\xff\xff\x7f"));

  // Invalid.
  ExpectDecodeUint32Invalid(7, "\x7f\xff\xff\x80");
  ExpectDecodeUint32Invalid(7, "\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(6, "\x3f\xff\xff\x80");
  ExpectDecodeUint32Invalid(6, "\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(5, "\x1f\xff\xff\x80");
  ExpectDecodeUint32Invalid(5, "\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(4, "\x0f\xff\xff\x80");
  ExpectDecodeUint32Invalid(4, "\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(3, "\x07\xff\xff\x80");
  ExpectDecodeUint32Invalid(3, "\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(2, "\x03\xff\xff\x80");
  ExpectDecodeUint32Invalid(2, "\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(1, "\x01\xff\xff\x80");
  ExpectDecodeUint32Invalid(1, "\xff\xff\xff\xff");
}

TEST(HpackInputStreamTest, FiveByteIntegersOneToSevenBitPrefixes) {
  // Minimums.
  EXPECT_EQ(0x20007fu, DecodeValidUint32(7, "\x7f\x80\x80\x80\x01"));
  EXPECT_EQ(0x20007fu, DecodeValidUint32(7, "\xff\x80\x80\x80\x01"));
  EXPECT_EQ(0x20003fu, DecodeValidUint32(6, "\x3f\x80\x80\x80\x01"));
  EXPECT_EQ(0x20003fu, DecodeValidUint32(6, "\xff\x80\x80\x80\x01"));
  EXPECT_EQ(0x20001fu, DecodeValidUint32(5, "\x1f\x80\x80\x80\x01"));
  EXPECT_EQ(0x20001fu, DecodeValidUint32(5, "\xff\x80\x80\x80\x01"));
  EXPECT_EQ(0x20000fu, DecodeValidUint32(4, "\x0f\x80\x80\x80\x01"));
  EXPECT_EQ(0x20000fu, DecodeValidUint32(4, "\xff\x80\x80\x80\x01"));
  EXPECT_EQ(0x200007u, DecodeValidUint32(3, "\x07\x80\x80\x80\x01"));
  EXPECT_EQ(0x200007u, DecodeValidUint32(3, "\xff\x80\x80\x80\x01"));
  EXPECT_EQ(0x200003u, DecodeValidUint32(2, "\x03\x80\x80\x80\x01"));
  EXPECT_EQ(0x200003u, DecodeValidUint32(2, "\xff\x80\x80\x80\x01"));
  EXPECT_EQ(0x200001u, DecodeValidUint32(1, "\x01\x80\x80\x80\x01"));
  EXPECT_EQ(0x200001u, DecodeValidUint32(1, "\xff\x80\x80\x80\x01"));

  // Maximums.
  EXPECT_EQ(0x1000007eu, DecodeValidUint32(7, "\x7f\xff\xff\xff\x7f"));
  EXPECT_EQ(0x1000007eu, DecodeValidUint32(7, "\xff\xff\xff\xff\x7f"));
  EXPECT_EQ(0x1000003eu, DecodeValidUint32(6, "\x3f\xff\xff\xff\x7f"));
  EXPECT_EQ(0x1000003eu, DecodeValidUint32(6, "\xff\xff\xff\xff\x7f"));
  EXPECT_EQ(0x1000001eu, DecodeValidUint32(5, "\x1f\xff\xff\xff\x7f"));
  EXPECT_EQ(0x1000001eu, DecodeValidUint32(5, "\xff\xff\xff\xff\x7f"));
  EXPECT_EQ(0x1000000eu, DecodeValidUint32(4, "\x0f\xff\xff\xff\x7f"));
  EXPECT_EQ(0x1000000eu, DecodeValidUint32(4, "\xff\xff\xff\xff\x7f"));
  EXPECT_EQ(0x10000006u, DecodeValidUint32(3, "\x07\xff\xff\xff\x7f"));
  EXPECT_EQ(0x10000006u, DecodeValidUint32(3, "\xff\xff\xff\xff\x7f"));
  EXPECT_EQ(0x10000002u, DecodeValidUint32(2, "\x03\xff\xff\xff\x7f"));
  EXPECT_EQ(0x10000002u, DecodeValidUint32(2, "\xff\xff\xff\xff\x7f"));
  EXPECT_EQ(0x10000000u, DecodeValidUint32(1, "\x01\xff\xff\xff\x7f"));
  EXPECT_EQ(0x10000000u, DecodeValidUint32(1, "\xff\xff\xff\xff\x7f"));

  // Invalid.
  ExpectDecodeUint32Invalid(7, "\x7f\xff\xff\xff\x80");
  ExpectDecodeUint32Invalid(7, "\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(6, "\x3f\xff\xff\xff\x80");
  ExpectDecodeUint32Invalid(6, "\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(5, "\x1f\xff\xff\xff\x80");
  ExpectDecodeUint32Invalid(5, "\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(4, "\x0f\xff\xff\xff\x80");
  ExpectDecodeUint32Invalid(4, "\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(3, "\x07\xff\xff\xff\x80");
  ExpectDecodeUint32Invalid(3, "\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(2, "\x03\xff\xff\xff\x80");
  ExpectDecodeUint32Invalid(2, "\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(1, "\x01\xff\xff\xff\x80");
  ExpectDecodeUint32Invalid(1, "\xff\xff\xff\xff\xff");
}

TEST(HpackInputStreamTest, SixByteIntegersOneToSevenBitPrefixes) {
  // Minimums.
  EXPECT_EQ(0x1000007fu, DecodeValidUint32(7, "\x7f\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x1000007fu, DecodeValidUint32(7, "\xff\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x1000003fu, DecodeValidUint32(6, "\x3f\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x1000003fu, DecodeValidUint32(6, "\xff\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x1000001fu, DecodeValidUint32(5, "\x1f\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x1000001fu, DecodeValidUint32(5, "\xff\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x1000000fu, DecodeValidUint32(4, "\x0f\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x1000000fu, DecodeValidUint32(4, "\xff\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x10000007u, DecodeValidUint32(3, "\x07\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x10000007u, DecodeValidUint32(3, "\xff\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x10000003u, DecodeValidUint32(2, "\x03\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x10000003u, DecodeValidUint32(2, "\xff\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x10000001u, DecodeValidUint32(1, "\x01\x80\x80\x80\x80\x01"));
  EXPECT_EQ(0x10000001u, DecodeValidUint32(1, "\xff\x80\x80\x80\x80\x01"));

  // Maximums.
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(7, "\x7f\x80\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(7, "\xff\x80\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(6, "\x3f\xc0\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(6, "\xff\xc0\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(5, "\x1f\xe0\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(5, "\xff\xe0\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(4, "\x0f\xf0\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(4, "\xff\xf0\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(3, "\x07\xf8\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(3, "\xff\xf8\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(2, "\x03\xfc\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(2, "\xff\xfc\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(1, "\x01\xfe\xff\xff\xff\x0f"));
  EXPECT_EQ(0xffffffffu, DecodeValidUint32(1, "\xff\xfe\xff\xff\xff\x0f"));

  // Invalid.
  ExpectDecodeUint32Invalid(7, "\x7f\x80\xff\xff\xff\x10");
  ExpectDecodeUint32Invalid(7, "\xff\x80\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(6, "\x3f\xc0\xff\xff\xff\x10");
  ExpectDecodeUint32Invalid(6, "\xff\xc0\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(5, "\x1f\xe0\xff\xff\xff\x10");
  ExpectDecodeUint32Invalid(5, "\xff\xe0\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(4, "\x0f\xf0\xff\xff\xff\x10");
  ExpectDecodeUint32Invalid(4, "\xff\xf0\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(3, "\x07\xf8\xff\xff\xff\x10");
  ExpectDecodeUint32Invalid(3, "\xff\xf8\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(2, "\x03\xfc\xff\xff\xff\x10");
  ExpectDecodeUint32Invalid(2, "\xff\xfc\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(1, "\x01\xfe\xff\xff\xff\x10");
  ExpectDecodeUint32Invalid(1, "\xff\xfe\xff\xff\xff\xff");
}

// There are no valid uint32 encodings that are greater than six
// bytes.
TEST(HpackInputStreamTest, SevenByteIntegersOneToSevenBitPrefixes) {
  ExpectDecodeUint32Invalid(7, "\x7f\x80\x80\x80\x80\x80\x00");
  ExpectDecodeUint32Invalid(7, "\x7f\x80\x80\x80\x80\x80\x01");
  ExpectDecodeUint32Invalid(7, "\xff\xff\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(6, "\x3f\x80\x80\x80\x80\x80\x00");
  ExpectDecodeUint32Invalid(6, "\x3f\x80\x80\x80\x80\x80\x01");
  ExpectDecodeUint32Invalid(6, "\xff\xff\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(5, "\x1f\x80\x80\x80\x80\x80\x00");
  ExpectDecodeUint32Invalid(5, "\x1f\x80\x80\x80\x80\x80\x01");
  ExpectDecodeUint32Invalid(5, "\xff\xff\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(4, "\x0f\x80\x80\x80\x80\x80\x00");
  ExpectDecodeUint32Invalid(4, "\x0f\x80\x80\x80\x80\x80\x01");
  ExpectDecodeUint32Invalid(4, "\xff\xff\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(3, "\x07\x80\x80\x80\x80\x80\x00");
  ExpectDecodeUint32Invalid(3, "\x07\x80\x80\x80\x80\x80\x01");
  ExpectDecodeUint32Invalid(3, "\xff\xff\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(2, "\x03\x80\x80\x80\x80\x80\x00");
  ExpectDecodeUint32Invalid(2, "\x03\x80\x80\x80\x80\x80\x01");
  ExpectDecodeUint32Invalid(2, "\xff\xff\xff\xff\xff\xff\xff");
  ExpectDecodeUint32Invalid(1, "\x01\x80\x80\x80\x80\x80\x00");
  ExpectDecodeUint32Invalid(1, "\x01\x80\x80\x80\x80\x80\x01");
  ExpectDecodeUint32Invalid(1, "\xff\xff\xff\xff\xff\xff\xff");
}

// Decoding a valid encoded string literal should work.
TEST(HpackInputStreamTest, DecodeNextStringLiteral) {
  HpackInputStream input_stream(kuint32max, "\x0estring literal");

  EXPECT_TRUE(input_stream.HasMoreData());
  StringPiece string_piece;
  EXPECT_TRUE(input_stream.DecodeNextStringLiteralForTest(&string_piece));
  EXPECT_EQ("string literal", string_piece);
  EXPECT_FALSE(input_stream.HasMoreData());
}

// Decoding an encoded string literal with size larger than
// |max_string_literal_size_| should fail.
TEST(HpackInputStreamTest, DecodeNextStringLiteralSizeLimit) {
  HpackInputStream input_stream(13, "\x0estring literal");

  EXPECT_TRUE(input_stream.HasMoreData());
  StringPiece string_piece;
  EXPECT_FALSE(input_stream.DecodeNextStringLiteralForTest(&string_piece));
}

// Decoding an encoded string literal with size larger than the
// remainder of the buffer should fail.
TEST(HpackInputStreamTest, DecodeNextStringLiteralInvalidSize) {
  // Set the length to be one more than it should be.
  HpackInputStream input_stream(kuint32max, "\x0fstring literal");

  EXPECT_TRUE(input_stream.HasMoreData());
  StringPiece string_piece;
  EXPECT_FALSE(input_stream.DecodeNextStringLiteralForTest(&string_piece));
}

}  // namespace

}  // namespace net
