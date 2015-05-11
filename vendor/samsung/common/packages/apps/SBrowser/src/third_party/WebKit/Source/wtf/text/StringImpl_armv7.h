/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
 * Copyright (c) 2013 Samsung Electronics Co., Ltd All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StringImpl_armv7_h
#define StringImpl_armv7_h

namespace WTF {

#pragma GCC diagnostic push
// GCC cannot detect that aValue and bValue are initialized in asm code.
// Let's disable those warnings here.
#pragma GCC diagnostic ignored "-Wuninitialized"
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"

ALWAYS_INLINE bool equal(const LChar* a, const LChar* b, unsigned length)
{
  bool isEqual = false;
  uint32_t aValue;
  uint32_t bValue;
  asm("subs   %[length], #4\n"
      "blo    2f\n"

      "0:\n" // Tag 0 = Start of loop over 32 bits.
      "ldr    %[aValue], [%[a]], #4\n"
      "ldr    %[bValue], [%[b]], #4\n"
      "cmp    %[aValue], %[bValue]\n"
      "bne    66f\n"
      "subs   %[length], #4\n"
      "bhs    0b\n"

      // At this point, length can be:
      // -0: 00000000000000000000000000000000 (0 bytes left)
      // -1: 11111111111111111111111111111111 (3 bytes left)
      // -2: 11111111111111111111111111111110 (2 bytes left)
      // -3: 11111111111111111111111111111101 (1 byte left)
      // -4: 11111111111111111111111111111100 (length was 0)
      // The pointers are at the correct position.
      "2:\n" // Tag 2 = End of loop over 32 bits, check for pair of characters.
      "tst    %[length], #2\n"
      "beq    1f\n"
      "ldrh   %[aValue], [%[a]], #2\n"
      "ldrh   %[bValue], [%[b]], #2\n"
      "cmp    %[aValue], %[bValue]\n"
      "bne    66f\n"

      "1:\n" // Tag 1 = Check for a single character left.
      "tst    %[length], #1\n"
      "beq    42f\n"
      "ldrb   %[aValue], [%[a]]\n"
      "ldrb   %[bValue], [%[b]]\n"
      "cmp    %[aValue], %[bValue]\n"
      "bne    66f\n"

      "42:\n" // Tag 42 = Success.
      "mov    %[isEqual], #1\n"
      "66:\n" // Tag 66 = End without changing isEqual to 1.
      : [length]"+r"(length), [isEqual]"+r"(isEqual), [a]"+r"(a), [b]"+r"(b), [aValue]"+r"(aValue), [bValue]"+r"(bValue)
      :
      :
    );

  return isEqual;
}

ALWAYS_INLINE bool equal(const UChar* a, const UChar* b, unsigned length)
{
  bool isEqual = false;
  uint32_t aValue;
  uint32_t bValue;
  asm("subs   %[length], #2\n"
      "blo    1f\n"

      "0:\n" // Label 0 = Start of loop over 32 bits.
      "ldr    %[aValue], [%[a]], #4\n"
      "ldr    %[bValue], [%[b]], #4\n"
      "cmp    %[aValue], %[bValue]\n"
      "bne    66f\n"
      "subs   %[length], #2\n"
      "bhs    0b\n"

      // At this point, length can be:
      // -0: 00000000000000000000000000000000 (0 bytes left)
      // -1: 11111111111111111111111111111111 (1 character left, 2 bytes)
      // -2: 11111111111111111111111111111110 (length was zero)
      // The pointers are at the correct position.
      "1:\n" // Label 1 = Check for a single character left.
      "tst    %[length], #1\n"
      "beq    42f\n"
      "ldrh   %[aValue], [%[a]]\n"
      "ldrh   %[bValue], [%[b]]\n"
      "cmp    %[aValue], %[bValue]\n"
      "bne    66f\n"

      "42:\n" // Label 42 = Success.
      "mov    %[isEqual], #1\n"
      "66:\n" // Label 66 = End without changing isEqual to 1.
      : [length]"+r"(length), [isEqual]"+r"(isEqual), [a]"+r"(a), [b]"+r"(b), [aValue]"+r"(aValue), [bValue]"+r"(bValue)
      :
      :
      );
  return isEqual;
}

#pragma GCC diagnostic pop

} // namespace WTF

#endif // StringImpl_armv7_h

