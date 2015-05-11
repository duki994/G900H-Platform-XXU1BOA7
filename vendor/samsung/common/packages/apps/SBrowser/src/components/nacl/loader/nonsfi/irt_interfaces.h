// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_LOADER_NONSFI_IRT_INTERFACES_H_
#define COMPONENTS_NACL_LOADER_NONSFI_IRT_INTERFACES_H_

#include "base/basictypes.h"
#include "native_client/src/untrusted/irt/irt.h"

namespace nacl {
namespace nonsfi {

size_t NaClIrtInterface(const char* interface_ident,
                        void* table, size_t tablesize);

extern const struct nacl_irt_basic kIrtBasic;
extern const struct nacl_irt_fdio kIrtFdIO;
extern const struct nacl_irt_memory kIrtMemory;
extern const struct nacl_irt_thread kIrtThread;
extern const struct nacl_irt_futex kIrtFutex;
extern const struct nacl_irt_tls kIrtTls;
extern const struct nacl_irt_clock kIrtClock;

}  // namespace nonsfi
}  // namespace nacl

#endif  // COMPONENTS_NACL_LOADER_NONSFI_IRT_INTERFACES_H_
