// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_REGISTRATION_PUSH_REGISTRATION_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_PUSH_REGISTRATION_PUSH_REGISTRATION_MESSAGE_FILTER_H_

#if defined(ENABLE_PUSH_API)

#include <string>
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/id_map.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/browser_message_filter.h"
#include "url/gurl.h"

class GURL;

namespace IPC {
class Message;
}

namespace content {

class PushPermissionContext;
class PushProvider;

class  CONTENT_EXPORT PushRegistrationMessageFilter
    : public BrowserMessageFilter {
 public:
  PushRegistrationMessageFilter(
      int render_process_id,
      PushPermissionContext* push_permission_context);

 private:
  class RequestDispatcher;
  class RegisterDispatcher;
  class UnregisterDispatcher;
  class IsRegisteredDispatcher;
  class HasPermissionDispatcher;
  class RequestPermissionDispatcher;

  virtual ~PushRegistrationMessageFilter();

  // content::BrowserMessageFilter implementation.
  virtual void OverrideThreadForMessage(
      const IPC::Message& message,
      content::BrowserThread::ID* thread) OVERRIDE;

  virtual bool OnMessageReceived(
      const IPC::Message& message,
      bool* message_was_ok) OVERRIDE;

  void OnRegister(int routing_id, int callbacks_id, const GURL& origin);
  void OnUnregister(int routing_id, int callbacks_id, const GURL& origin);
  void OnIsRegistered(int routing_id, int callbacks_id, const GURL& origin);
  void OnHasPermission(int routing_id, int callbacks_id, const GURL& origin);
  void OnRequestPermission(int routing_id, int callbacks_id, const GURL& origin);

  static PushProvider* CreateProvider();

  scoped_ptr<PushProvider> push_provider_;
  IDMap<RequestDispatcher, IDMapOwnPointer> outstanding_requests_;
  base::WeakPtrFactory<PushRegistrationMessageFilter> weak_factory_;

  int render_process_id_;
  scoped_refptr<PushPermissionContext> push_permission_context_;

  DISALLOW_COPY_AND_ASSIGN(PushRegistrationMessageFilter);
};

} // namespace content

#endif // defined(ENABLE_PUSH_API)

#endif // CONTENT_BROWSER_PUSH_MESSAGING_MESSAGE_FILTER_H_
