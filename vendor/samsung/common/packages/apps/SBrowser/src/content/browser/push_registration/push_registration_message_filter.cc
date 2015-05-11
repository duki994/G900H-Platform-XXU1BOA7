// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_PUSH_API)

#include "content/browser/push_registration/push_registration_message_filter.h"

#include <string>
#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string16.h"
#include "content/browser/push_registration/push_provider.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/push_registration/push_registration_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/push_permission_context.h"

namespace content {

void SendPushPermissionResponse(
    int render_process_id,
    int routing_id,
    int callback_id,
    bool allowed) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderViewHostImpl* render_view_host =
      RenderViewHostImpl::FromID(render_process_id, routing_id);

  if (!render_view_host)
    return;

  render_view_host->Send(
      new PushRegistrationMsg_PermissionSet(routing_id, callback_id, allowed));
}

PushRegistrationMessageFilter::PushRegistrationMessageFilter(
    int render_process_id,
    PushPermissionContext* push_permission_context)
    : BrowserMessageFilter(),
      weak_factory_(this),
      render_process_id_(render_process_id),
      push_permission_context_(push_permission_context) {
  if (!push_provider_.get())
    push_provider_.reset(CreateProvider());
}

PushRegistrationMessageFilter::~PushRegistrationMessageFilter() {
}

// RequestDispatcher Class
class PushRegistrationMessageFilter::RequestDispatcher {
 public:
  RequestDispatcher(
      base::WeakPtr<PushRegistrationMessageFilter> message_filter,
      int callbacks_id)
      : message_filter_(message_filter),
        callbacks_id_(callbacks_id) {
    message_filter_->outstanding_requests_.AddWithID(this, callbacks_id_);
  }
  virtual ~RequestDispatcher() {}

 protected:
  // Subclass must call this when it's done with the request.
  void Completed() {
    if (message_filter_)
      message_filter_->outstanding_requests_.Remove(callbacks_id_);
  }

  PushRegistrationMessageFilter* message_filter() const {
    return message_filter_.get();
  }

  PushProvider* push_provider() const {
    return message_filter_ ? message_filter_->push_provider_.get() : NULL;
  }

  int callbacks_id() const { return callbacks_id_; }

  PushPermissionContext* push_permission_context() {
    return message_filter_->push_permission_context_.get();
  }
  int render_process_id() { return message_filter_->render_process_id_; }

 private:
  base::WeakPtr<PushRegistrationMessageFilter> message_filter_;
  const int callbacks_id_;
};

// RequestDispatcher Class
// RegisterPush Class
class PushRegistrationMessageFilter::RegisterDispatcher
    : public RequestDispatcher {
 public:
  RegisterDispatcher(
      base::WeakPtr<PushRegistrationMessageFilter> message_filter,
      int routing_id,
      int callbacks_id)
      : RequestDispatcher(message_filter, callbacks_id),
        routing_id_(routing_id),
        weak_factory_(this) {}

  virtual ~RegisterDispatcher() {}

  void Register(const GURL& origin) {
    if (!push_provider()) {
      message_filter()->Send(new PushRegistrationMsg_RegisterError(
          routing_id_, callbacks_id()));
      Completed();
      return;
    }
    push_provider()->Register(
        origin,
        base::Bind(&RegisterDispatcher::DidRegister,
            weak_factory_.GetWeakPtr()));
  }

 private:
  void DidRegister(
      const base::string16& endpoint,
      const base::string16& registration_id,
      bool error) {
    if (!message_filter())
      return;

    if (error)
      message_filter()->Send(new PushRegistrationMsg_RegisterError(
          routing_id_, callbacks_id()));
    else
      message_filter()->Send(new PushRegistrationMsg_RegisterSuccess(
          routing_id_,
          callbacks_id(),
          endpoint,
          registration_id));

    Completed();
  }

  const int routing_id_;
  base::WeakPtrFactory<RegisterDispatcher> weak_factory_;
};

// UnregisterDispatcher
class PushRegistrationMessageFilter::UnregisterDispatcher
    : public RequestDispatcher {
 public:
  UnregisterDispatcher(
      base::WeakPtr<PushRegistrationMessageFilter> message_filter,
      int routing_id,
      int callbacks_id)
      : RequestDispatcher(message_filter, callbacks_id),
        routing_id_(routing_id),
        weak_factory_(this) {}
  virtual ~UnregisterDispatcher() {}

  void Unregister(const GURL& origin) {
    if (!push_provider()) {
      message_filter()->Send(new PushRegistrationMsg_UnregisterError(
          routing_id_, callbacks_id()));
      Completed();
      return;
    }
    push_provider()->Unregister(
        origin,
        base::Bind(&UnregisterDispatcher::DidUnregister,
            weak_factory_.GetWeakPtr()));
  }

 private:
  void DidUnregister(bool error) {
    if (!message_filter())
      return;

    if (error)
      message_filter()->Send(new PushRegistrationMsg_UnregisterError(
          routing_id_, callbacks_id()));
    else
      message_filter()->Send(new PushRegistrationMsg_UnregisterSuccess(
          routing_id_, callbacks_id()));

    Completed();
  }
  int routing_id_;
  base::WeakPtrFactory<UnregisterDispatcher> weak_factory_;
};

// IsRegisteredDispatcher
class PushRegistrationMessageFilter::IsRegisteredDispatcher
    : public RequestDispatcher {
 public:
  IsRegisteredDispatcher(
      base::WeakPtr<PushRegistrationMessageFilter> message_filter,
      int routing_id,
      int callbacks_id)
      : RequestDispatcher(message_filter, callbacks_id),
        routing_id_(routing_id),
        weak_factory_(this) {}
  virtual ~IsRegisteredDispatcher() {}

  void IsRegistered(const GURL& origin) {
    if (!push_provider()) {
      message_filter()->Send(new PushRegistrationMsg_IsRegisteredError(
          routing_id_, callbacks_id()));
      Completed();
      return;
    }
    push_provider()->IsRegistered(
        origin,
        base::Bind(&IsRegisteredDispatcher::DidIsRegistered,
            weak_factory_.GetWeakPtr()));
  }

 private:
  void DidIsRegistered(
      bool is_registered,
      bool error) {
    if (!message_filter())
      return;

    if (error)
      message_filter()->Send(new PushRegistrationMsg_IsRegisteredError(
          routing_id_, callbacks_id()));
    else
      message_filter()->Send(new PushRegistrationMsg_IsRegisteredSuccess(
          routing_id_, callbacks_id(), is_registered));

    Completed();
  }
  const int routing_id_;
  base::WeakPtrFactory<IsRegisteredDispatcher> weak_factory_;
};

// HasPermissionDispatcher
class PushRegistrationMessageFilter::HasPermissionDispatcher
    : public RequestDispatcher {
 public:
  HasPermissionDispatcher(
      base::WeakPtr<PushRegistrationMessageFilter> message_filter,
      int routing_id,
      int callbacks_id)
      : RequestDispatcher(message_filter, callbacks_id),
        routing_id_(routing_id),
        weak_factory_(this) {}
  virtual ~HasPermissionDispatcher() {}

  void HasPermission(const GURL& origin) {
    if (!push_provider()) {
      message_filter()->Send(new PushRegistrationMsg_HasPermissionError(
          routing_id_, callbacks_id()));
      Completed();
      return;
    }
    push_provider()->IsRegistered(
        origin,
        base::Bind(&HasPermissionDispatcher::DidIsRegistered,
            weak_factory_.GetWeakPtr()));
  }

 private:
  void DidIsRegistered(bool is_registered,
                       bool error) {
    if (!message_filter())
      return;

    if (error)
      message_filter()->Send(new PushRegistrationMsg_HasPermissionError(
          routing_id_, callbacks_id()));
    else
      message_filter()->Send(new PushRegistrationMsg_HasPermissionSuccess(
          routing_id_, callbacks_id(), is_registered));
    Completed();
  }
  const int routing_id_;
  base::WeakPtrFactory<HasPermissionDispatcher> weak_factory_;
};

// RequestPermissionDispatcher
class PushRegistrationMessageFilter::RequestPermissionDispatcher
    : public RequestDispatcher {
 public:
  RequestPermissionDispatcher(
      base::WeakPtr<PushRegistrationMessageFilter> message_filter,
      int routing_id,
      int callbacks_id)
      : RequestDispatcher(message_filter, callbacks_id),
        routing_id_(routing_id),
        weak_factory_(this) {}

  virtual ~RequestPermissionDispatcher() {}

  void RequestPermission(const GURL& origin) {
    if (!push_provider()) {
      message_filter()->Send(new PushRegistrationMsg_PermissionSet(
          routing_id_, callbacks_id(), false));
      Completed();
      return;
    }

    // Check if registed or not
    push_provider()->IsRegistered(
        origin,
        base::Bind(&RequestPermissionDispatcher::DidIsRegistered,
        weak_factory_.GetWeakPtr(),
        origin));
  }

 private:
  void DidIsRegistered(
      const GURL& origin,
      bool is_registered,
      bool error) {
    if (!message_filter())
      return;

    if (error) {
      message_filter()->Send(new PushRegistrationMsg_IsRegisteredError(
          routing_id_, callbacks_id()));
      Completed();
      return;
    }

    if (is_registered) {
      message_filter()->Send(new PushRegistrationMsg_PermissionSet(
          routing_id_, callbacks_id(), true));
      Completed();
      return;
    }

    if (push_permission_context()) {
      push_permission_context()->RequestPushPermission(
          render_process_id(),
          routing_id_,
          callbacks_id(),
          origin,
          base::Bind(&RequestPermissionDispatcher::DidRequestPermission,
              weak_factory_.GetWeakPtr()));
    }
  }

  void DidRequestPermission(bool allowed) {
    if (!message_filter())
      return;

    message_filter()->Send(new PushRegistrationMsg_PermissionSet(
        routing_id_, callbacks_id(), allowed));
    Completed();
  }

  const int routing_id_;
  base::WeakPtrFactory<RequestPermissionDispatcher> weak_factory_;
};

bool PushRegistrationMessageFilter::OnMessageReceived(
    const IPC::Message& message,
    bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(PushRegistrationMessageFilter, message, *message_was_ok)
  IPC_MESSAGE_HANDLER(PushRegistrationHostMsg_Register, OnRegister)
  IPC_MESSAGE_HANDLER(PushRegistrationHostMsg_Unregister, OnUnregister)
  IPC_MESSAGE_HANDLER(PushRegistrationHostMsg_IsRegistered, OnIsRegistered)
  IPC_MESSAGE_HANDLER(PushRegistrationHostMsg_HasPermission, OnHasPermission)
  IPC_MESSAGE_HANDLER(PushRegistrationHostMsg_RequestPermission, OnRequestPermission)
      IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushRegistrationMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  if (message.type() == PushRegistrationHostMsg_Register::ID)
    *thread = BrowserThread::IO;
}

void PushRegistrationMessageFilter::OnRegister(
    int routing_id,
    int callbacks_id,
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  RegisterDispatcher* dispatcher = new RegisterDispatcher(
      weak_factory_.GetWeakPtr(), routing_id, callbacks_id);

  dispatcher->Register(origin);
}

void PushRegistrationMessageFilter::OnUnregister(
    int routing_id,
    int callbacks_id,
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  UnregisterDispatcher* dispatcher = new UnregisterDispatcher(
      weak_factory_.GetWeakPtr(), routing_id, callbacks_id);

  dispatcher->Unregister(origin);
}

void PushRegistrationMessageFilter::OnIsRegistered(
    int routing_id,
    int callbacks_id,
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  IsRegisteredDispatcher* dispatcher = new IsRegisteredDispatcher(
      weak_factory_.GetWeakPtr(), routing_id, callbacks_id);

  dispatcher->IsRegistered(origin);
}

void PushRegistrationMessageFilter::OnHasPermission(
    int routing_id,
    int callbacks_id,
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  HasPermissionDispatcher* dispatcher = new HasPermissionDispatcher(
      weak_factory_.GetWeakPtr(), routing_id, callbacks_id);

  dispatcher->HasPermission(origin);
}

void PushRegistrationMessageFilter::OnRequestPermission(
    int routing_id,
    int callbacks_id,
    const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  RequestPermissionDispatcher* dispatcher = new RequestPermissionDispatcher(
      weak_factory_.GetWeakPtr(), routing_id, callbacks_id);

  dispatcher->RequestPermission(origin);
}

} // namespace content

#endif // defined(ENABLE_PUSH_API)
