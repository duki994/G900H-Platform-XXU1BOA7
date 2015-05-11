// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/mock_password_store_service.h"

#include "components/password_manager/core/browser/mock_password_store.h"

// static
BrowserContextKeyedService* MockPasswordStoreService::Build(
    content::BrowserContext* /*profile*/) {
  scoped_refptr<PasswordStore> store(new MockPasswordStore);
  if (!store || !store->Init())
    return NULL;
  return new MockPasswordStoreService(store);
}

MockPasswordStoreService::MockPasswordStoreService(
    scoped_refptr<PasswordStore> password_store)
    : PasswordStoreService(password_store) {}

MockPasswordStoreService::~MockPasswordStoreService() {}
