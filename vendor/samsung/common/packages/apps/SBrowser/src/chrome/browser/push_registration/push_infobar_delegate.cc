// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/push_registration/push_infobar_delegate.h"

#if defined(ENABLE_PUSH_API)

#include "base/metrics/histogram.h"
#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

typedef PushInfoBarDelegate DelegateType;

namespace {

enum PushInfoBarDelegateEvent {
  // The bar was created.
  PUSH_INFO_BAR_DELEGATE_EVENT_CREATE = 0,

  // User allowed use of push api.
  PUSH_INFO_BAR_DELEGATE_EVENT_ALLOW = 1,

  // User denied use of push api.
  PUSH_INFO_BAR_DELEGATE_EVENT_DENY = 2,

  // User dismissed the bar.
  PUSH_INFO_BAR_DELEGATE_EVENT_DISMISS = 3,

  // User clicked on link.
  PUSH_INFO_BAR_DELEGATE_EVENT_LINK_CLICK = 4,

  // User ignored the bar.
  PUSH_INFO_BAR_DELEGATE_EVENT_IGNORED = 5,

  PUSH_INFO_BAR_DELEGATE_EVENT_COUNT = 6
};

void RecordUmaEvent(PushInfoBarDelegateEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Push.InfoBarDelegate.Event",
      event, PUSH_INFO_BAR_DELEGATE_EVENT_COUNT);
}

}  // namespace

// static
InfoBar* PushInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    const std::string& display_languages) {
  RecordUmaEvent(PUSH_INFO_BAR_DELEGATE_EVENT_CREATE);
  const content::NavigationEntry* committed_entry =
      infobar_service->web_contents()->GetController().GetLastCommittedEntry();
  return infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new DelegateType(
          controller, id, requesting_frame,
          committed_entry ? committed_entry->GetUniqueID() : 0,
          display_languages))));
}

PushInfoBarDelegate::PushInfoBarDelegate(
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame,
    int contents_unique_id,
    const std::string& display_languages)
    : ConfirmInfoBarDelegate(),
      controller_(controller),
      id_(id),
      requesting_frame_(requesting_frame.GetOrigin()),
      contents_unique_id_(contents_unique_id),
      display_languages_(display_languages),
      user_has_interacted_(false) {
}

PushInfoBarDelegate::~PushInfoBarDelegate() {
  if (!user_has_interacted_)
    RecordUmaEvent(PUSH_INFO_BAR_DELEGATE_EVENT_IGNORED);
}

bool PushInfoBarDelegate::Accept() {
  RecordUmaEvent(PUSH_INFO_BAR_DELEGATE_EVENT_ALLOW);
  set_user_has_interacted();
  SetPermission(false, true);
  return true;
}

void PushInfoBarDelegate::SetPermission(bool update_content_setting,
                                               bool allowed) {
  controller_->OnPermissionSet(
        id_, requesting_frame_,
        web_contents()->GetLastCommittedURL().GetOrigin(),
        update_content_setting, allowed);
}

void PushInfoBarDelegate::InfoBarDismissed() {
  RecordUmaEvent(PUSH_INFO_BAR_DELEGATE_EVENT_DISMISS);
  set_user_has_interacted();
  SetPermission(false, false);
}

int PushInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_DESKTOP_NOTIFICATIONS;
}

InfoBarDelegate::Type PushInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

bool PushInfoBarDelegate::ShouldExpireInternal(
    const content::LoadCommittedDetails& details) const {
  return (contents_unique_id_ != details.entry->GetUniqueID()) ||
      (content::PageTransitionStripQualifier(
          details.entry->GetTransitionType()) ==
              content::PAGE_TRANSITION_RELOAD);
}

base::string16 PushInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_NOTIFICATION_PERMISSIONS,
      net::FormatUrl(requesting_frame_, display_languages_));
}

base::string16 PushInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_NOTIFICATION_PERMISSION_YES : IDS_NOTIFICATION_PERMISSION_NO);
}

bool PushInfoBarDelegate::Cancel() {
  RecordUmaEvent(PUSH_INFO_BAR_DELEGATE_EVENT_DENY);
  set_user_has_interacted();
  SetPermission(false, false);
  return true;
}

base::string16 PushInfoBarDelegate::GetLinkText() const {
  return base::string16();
}

bool PushInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  return false;
}

#endif  // defined(ENABLE_PUSH_API)
