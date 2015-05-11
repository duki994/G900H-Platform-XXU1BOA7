// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_CHROME_STORAGE_IMPL_H_
#define THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_CHROME_STORAGE_IMPL_H_

#include <list>
#include <string>

#include "base/memory/scoped_vector.h"
#include "base/prefs/pref_store.h"
#include "base/scoped_observer.h"
#include "third_party/libaddressinput/chromium/cpp/include/libaddressinput/storage.h"
#include "third_party/libaddressinput/chromium/cpp/include/libaddressinput/util/scoped_ptr.h"

class WriteablePrefStore;

namespace autofill {

// An implementation of the Storage interface which passes through to an
// underlying WriteablePrefStore.
class ChromeStorageImpl : public ::i18n::addressinput::Storage,
                          public PrefStore::Observer {
 public:
  // |store| must outlive |this|.
  explicit ChromeStorageImpl(WriteablePrefStore* store);
  virtual ~ChromeStorageImpl();

  // ::i18n::addressinput::Storage implementation.
  virtual void Put(const std::string& key, scoped_ptr<std::string> data)
      OVERRIDE;
  virtual void Get(const std::string& key, scoped_ptr<Callback> data_ready)
      const OVERRIDE;

  // PrefStore::Observer implementation.
  virtual void OnPrefValueChanged(const std::string& key) OVERRIDE;
  virtual void OnInitializationCompleted(bool succeeded) OVERRIDE;

 private:
  struct Request {
    Request(const std::string& key, scoped_ptr<Callback> callback);

    std::string key;
    scoped_ptr<Callback> callback;
  };

  // Non-const version of Get().
  void DoGet(const std::string& key, scoped_ptr<Callback> data_ready);

  WriteablePrefStore* backing_store_;  // weak

  // Get requests that haven't yet been serviced.
  ScopedVector<Request> outstanding_requests_;

  ScopedObserver<PrefStore, ChromeStorageImpl> scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeStorageImpl);
};

}  // namespace autofill

#endif  // THIRD_PARTY_LIBADDRESSINPUT_CHROMIUM_CHROME_STORAGE_IMPL_H_
