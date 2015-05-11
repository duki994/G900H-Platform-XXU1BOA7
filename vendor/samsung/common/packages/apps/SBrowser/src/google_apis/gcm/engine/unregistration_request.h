// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_UNREGISTRATION_REQUEST_H_
#define GOOGLE_APIS_GCM_ENGINE_UNREGISTRATION_REQUEST_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/gcm/base/gcm_export.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace net {
class URLRequestContextGetter;
}

namespace gcm {

// Unregistration request is used to revoke registration IDs for applications
// that were uninstalled and should no longer receive GCM messages. In case an
// attempt to unregister fails, it will retry using the backoff policy.
// TODO(fgorski): Consider sharing code with RegistrationRequest if possible.
class GCM_EXPORT UnregistrationRequest : public net::URLFetcherDelegate {
 public:
  // Callback completing the unregistration request.
  typedef base::Callback<void(bool success)> UnregistrationCallback;

  // Details of the of the Unregistration Request. All parameters are mandatory.
  struct GCM_EXPORT RequestInfo {
    RequestInfo(uint64 android_id,
                uint64 security_token,
                const std::string& app_id);
    ~RequestInfo();

    // Android ID of the device.
    uint64 android_id;
    // Security token of the device.
    uint64 security_token;
    // Application ID.
    std::string app_id;
  };

  // Creates an instance of UnregistrationRequest. |callback| will be called
  // once registration has been revoked or there has been an error that makes
  // further retries pointless.
  UnregistrationRequest(
      const RequestInfo& request_info,
      const net::BackoffEntry::Policy& backoff_policy,
      const UnregistrationCallback& callback,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter);
  virtual ~UnregistrationRequest();

  // Starts an unregistration request.
  void Start();

  // URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 private:
  // Schedules a retry attempt and informs the backoff of previous request's
  // failure, when |update_backoff| is true.
  void RetryWithBackoff(bool update_backoff);

  UnregistrationCallback callback_;
  RequestInfo request_info_;

  net::BackoffEntry backoff_entry_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_ptr<net::URLFetcher> url_fetcher_;

  base::WeakPtrFactory<UnregistrationRequest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(UnregistrationRequest);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_UNREGISTRATION_REQUEST_H_
