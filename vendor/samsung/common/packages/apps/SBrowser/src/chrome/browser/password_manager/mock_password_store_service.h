// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_SERVICE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_SERVICE_H_

#include "chrome/browser/password_manager/password_store_factory.h"

class PasswordStore;

namespace content {
class BrowserContext;
}

class MockPasswordStoreService : public PasswordStoreService {
 public:
  static BrowserContextKeyedService* Build(content::BrowserContext* profile);

 private:
  explicit MockPasswordStoreService(
      scoped_refptr<PasswordStore> password_store);

  virtual ~MockPasswordStoreService();

  DISALLOW_COPY_AND_ASSIGN(MockPasswordStoreService);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_MOCK_PASSWORD_STORE_SERVICE_H_
