// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_REGISTRATION_PUSH_PROVIDER_H_
#define CONTENT_BROWSER_PUSH_REGISTRATION_PUSH_PROVIDER_H_

#if defined(ENABLE_PUSH_API)

#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"

class GURL;

namespace content {

// Interface that needs to be implemented by any backend that wants to handle for push service
class PushProvider {
 public:
  typedef base::Callback<void(
      const base::string16& endpoint,
      const base::string16& registration_id,
      bool error)> RegistrationCallback;
  typedef base::Callback<void(bool error)> UnregistrationCallback;
  typedef base::Callback<void(
      bool is_registered,
      bool error)> IsRegisteredCallback;

  virtual void Register(
      const GURL& origin,
      const RegistrationCallback& callback) = 0;
  virtual void Unregister(
      const GURL& origin,
      const UnregistrationCallback& callback) = 0;
  virtual void IsRegistered(
      const GURL& origin,
      const IsRegisteredCallback& callback) = 0;
  virtual ~PushProvider() {}
};

} // namespace content

#endif // defined(ENABLE_PUSH_API)

#endif // CONTENT_BROWSER_PUSH_REGISTRATION_PUSH_PROVIDER_H_
