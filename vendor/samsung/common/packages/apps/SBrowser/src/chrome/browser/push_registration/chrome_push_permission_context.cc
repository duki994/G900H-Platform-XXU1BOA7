// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_PUSH_API)

#include "chrome/browser/push_registration/chrome_push_permission_context.h"

#include <functional>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/permission_request_id.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/extensions/suggest_permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/browser/ui/website_settings/permission_bubble_request.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/extension.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

using extensions::APIPermission;
using extensions::ExtensionRegistry;

class PushPermissionRequest : public PermissionBubbleRequest {
 public:
  PushPermissionRequest(
      ChromePushPermissionContext* context,
      const PermissionRequestID& id,
      const GURL& origin,
      base::Callback<void(bool)> callback,
      const std::string& display_languages);
  virtual ~PushPermissionRequest();

  // PermissionBubbleDelegate:
  virtual base::string16 GetMessageText() const OVERRIDE;
  virtual base::string16 GetMessageTextFragment() const OVERRIDE;
  virtual base::string16 GetAlternateAcceptButtonText() const OVERRIDE;
  virtual base::string16 GetAlternateDenyButtonText() const OVERRIDE;
  virtual void PermissionGranted() OVERRIDE;
  virtual void PermissionDenied() OVERRIDE;
  virtual void Cancelled() OVERRIDE;
  virtual void RequestFinished() OVERRIDE;

 private:
  ChromePushPermissionContext* context_;
  PermissionRequestID id_;
  GURL origin_;
  base::Callback<void(bool)> callback_;
  std::string display_languages_;
};

PushPermissionRequest::PushPermissionRequest(
    ChromePushPermissionContext* context,
    const PermissionRequestID& id,
    const GURL& origin,
    base::Callback<void(bool)> callback,
    const std::string& display_languages)
    : context_(context),
      id_(id),
      origin_(origin),
      callback_(callback),
      display_languages_(display_languages) {}

PushPermissionRequest::~PushPermissionRequest() {}

base::string16 PushPermissionRequest::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_NOTIFICATION_PERMISSIONS,
      net::FormatUrl(origin_, display_languages_));
}

base::string16 PushPermissionRequest::GetMessageTextFragment() const {
  return l10n_util::GetStringUTF16(IDS_NOTIFICATION_PERMISSIONS_FRAGMENT);
}

base::string16
PushPermissionRequest::GetAlternateAcceptButtonText() const {
  return l10n_util::GetStringUTF16(IDS_NOTIFICATION_PERMISSION_YES);
}

base::string16
PushPermissionRequest::GetAlternateDenyButtonText() const {
  return l10n_util::GetStringUTF16(IDS_NOTIFICATION_PERMISSION_NO);
}

void PushPermissionRequest::PermissionGranted() {
  context_->NotifyPermissionSet(id_, origin_, callback_, true);
}

void PushPermissionRequest::PermissionDenied() {
  context_->NotifyPermissionSet(id_, origin_, callback_, false);
}

void PushPermissionRequest::Cancelled() {
  context_->NotifyPermissionSet(id_, origin_, callback_, false);
}

void PushPermissionRequest::RequestFinished() {
  delete this;
}

ChromePushPermissionContext::ChromePushPermissionContext(
    Profile* profile)
    : profile_(profile),
      shutting_down_(false) {
}

ChromePushPermissionContext::~ChromePushPermissionContext() {
  DCHECK(!permission_queue_controller_.get());
}

void ChromePushPermissionContext::RequestPushPermission(
    int render_process_id,
    int routing_id,
    int callback_id,
    const GURL& origin,
    base::Callback<void(bool)> callback) {

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(
            &ChromePushPermissionContext::RequestPushPermission,
            this, render_process_id, routing_id, callback_id,
            origin, callback));
    return;
  }

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (shutting_down_)
    return;

  const PermissionRequestID id(render_process_id, routing_id, callback_id, 0);
    if (!origin.is_valid()) {
    LOG(WARNING) << "Attempt to use push from an invalid URL: "
                 << origin << " (push is not supported in popups)";
    NotifyPermissionSet(id, origin, callback, false);
    return;
  }

  DecidePermission(id, origin, callback);
}

void ChromePushPermissionContext::DecidePermission(
    const PermissionRequestID& id,
    const GURL& origin,
    base::Callback<void(bool)> callback) {

  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  QueueController()->CreateInfoBarRequest(
      id, origin, origin, base::Bind(
          &ChromePushPermissionContext::NotifyPermissionSet,
          base::Unretained(this), id, origin, callback));
}

void ChromePushPermissionContext::ShutdownOnUIThread() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  permission_queue_controller_.reset();
  shutting_down_ = true;
}

void ChromePushPermissionContext::PermissionDecided(
    const PermissionRequestID& id,
    const GURL& origin,
    const GURL& embedder,
    base::Callback<void(bool)> callback,
    bool allowed) {
  NotifyPermissionSet(id, origin, callback, allowed);
}

void ChromePushPermissionContext::NotifyPermissionSet(
    const PermissionRequestID& id,
    const GURL& origin,
    base::Callback<void(bool)> callback,
    bool allowed) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  callback.Run(allowed);
}

PermissionQueueController*
    ChromePushPermissionContext::QueueController() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!shutting_down_);
  if (!permission_queue_controller_)
    permission_queue_controller_.reset(CreateQueueController());
  return permission_queue_controller_.get();
}

PermissionQueueController*
    ChromePushPermissionContext::CreateQueueController() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return new PermissionQueueController(profile(),
                                       CONTENT_SETTINGS_TYPE_PUSH);
}

#endif  // defined(ENABLE_PUSH_API)
