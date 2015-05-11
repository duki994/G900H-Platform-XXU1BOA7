// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/signed_certificate_timestamps_views.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/browser/ui/views/signed_certificate_timestamp_info_view.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/signed_certificate_timestamp_store.h"
#include "content/public/common/signed_certificate_timestamp_id_and_status.h"
#include "grit/generated_resources.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/ssl/signed_certificate_timestamp_and_status.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

using web_modal::WebContentsModalDialogManager;
using web_modal::WebContentsModalDialogManagerDelegate;
using views::GridLayout;

namespace {

void SignedCertificateTimestampIDsToList(
    const content::SignedCertificateTimestampIDStatusList& sct_ids_list,
    net::SignedCertificateTimestampAndStatusList* sct_list) {
  for (content::SignedCertificateTimestampIDStatusList::const_iterator it =
           sct_ids_list.begin();
       it != sct_ids_list.end();
       ++it) {
    scoped_refptr<net::ct::SignedCertificateTimestamp> sct;
    content::SignedCertificateTimestampStore::GetInstance()->Retrieve(it->id,
                                                                      &sct);
    sct_list->push_back(
        net::SignedCertificateTimestampAndStatus(sct, it->status));
  }
}

}  // namespace

namespace chrome {

void ShowSignedCertificateTimestampsViewer(
    content::WebContents* web_contents,
    const content::SignedCertificateTimestampIDStatusList& sct_ids_list) {
  net::SignedCertificateTimestampAndStatusList sct_list;
  SignedCertificateTimestampIDsToList(sct_ids_list, &sct_list);
  new SignedCertificateTimestampsViews(web_contents, sct_list);
}

}  // namespace chrome

class SCTListModel : public ui::ComboboxModel {
 public:
  explicit SCTListModel(
      const net::SignedCertificateTimestampAndStatusList& sct_list);
  virtual ~SCTListModel();

  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE;
  virtual base::string16 GetItemAt(int index) OVERRIDE;

 private:
  net::SignedCertificateTimestampAndStatusList sct_list_;

  DISALLOW_COPY_AND_ASSIGN(SCTListModel);
};

SCTListModel::SCTListModel(
    const net::SignedCertificateTimestampAndStatusList& sct_list)
    : sct_list_(sct_list) {}

SCTListModel::~SCTListModel() {}

int SCTListModel::GetItemCount() const { return sct_list_.size(); }

base::string16 SCTListModel::GetItemAt(int index) {
  DCHECK_LT(static_cast<size_t>(index), sct_list_.size());
  std::string origin = l10n_util::GetStringUTF8(
      chrome::ct::SCTOriginToResourceID(*(sct_list_[index].sct)));

  std::string status = l10n_util::GetStringUTF8(
      chrome::ct::StatusToResourceID(sct_list_[index].status));

  // TODO(eranm): Internationalization: If the locale is a RTL one,
  // format the string so that the index is on the right, status
  // and origin on the left. Specifically: the format part should be a
  // localized IDS string where the placeholders get rearranged for RTL locales,
  // GetStringFUTF16 is used to replace the placeholders with these
  // origin/status strings and the numbered index.
  return base::UTF8ToUTF16(base::StringPrintf(
      "%d: %s, %s", index + 1, origin.c_str(), status.c_str()));
}

SignedCertificateTimestampsViews::SignedCertificateTimestampsViews(
    content::WebContents* web_contents,
    const net::SignedCertificateTimestampAndStatusList& sct_list)
    : web_contents_(web_contents), sct_info_view_(NULL), sct_list_(sct_list) {
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents);
  WebContentsModalDialogManagerDelegate* modal_delegate =
      web_contents_modal_dialog_manager->delegate();
  DCHECK(modal_delegate);
  views::Widget* window = views::Widget::CreateWindowAsFramelessChild(
      this, modal_delegate->GetWebContentsModalDialogHost()->GetHostView());
  web_contents_modal_dialog_manager->ShowDialog(window->GetNativeView());
}

SignedCertificateTimestampsViews::~SignedCertificateTimestampsViews() {}

base::string16 SignedCertificateTimestampsViews::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_SCT_VIEWER_TITLE);
}

int SignedCertificateTimestampsViews::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

ui::ModalType SignedCertificateTimestampsViews::GetModalType() const {
#if defined(USE_ASH)
  return ui::MODAL_TYPE_CHILD;
#else
  return views::WidgetDelegate::GetModalType();
#endif
}

void SignedCertificateTimestampsViews::OnPerformAction(
    views::Combobox* combobox) {
  DCHECK_EQ(combobox, sct_selector_box_.get());
  DCHECK_LT(combobox->selected_index(), sct_list_model_->GetItemCount());
  ShowSCTInfo(combobox->selected_index());
}

gfx::Size SignedCertificateTimestampsViews::GetMinimumSize() {
  // Allow UpdateWebContentsModalDialogPosition to clamp the dialog width.
  return gfx::Size(View::GetMinimumSize().width() + 300,
                   View::GetMinimumSize().height());
}

void SignedCertificateTimestampsViews::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    Init();
}

void SignedCertificateTimestampsViews::Init() {
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int kSelectorBoxLayoutId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kSelectorBoxLayoutId);
  column_set->AddColumn(
      GridLayout::FILL, GridLayout::FILL, 1, GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, kSelectorBoxLayoutId);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Add SCT selector drop-down list.
  layout->StartRow(0, kSelectorBoxLayoutId);
  sct_list_model_.reset(new SCTListModel(sct_list_));
  sct_selector_box_.reset(new views::Combobox(sct_list_model_.get()));
  sct_selector_box_->set_listener(this);
  sct_selector_box_->set_owned_by_client();
  layout->AddView(sct_selector_box_.get());
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Add the SCT info view, displaying information about a specific SCT.
  layout->StartRow(0, kSelectorBoxLayoutId);
  sct_info_view_ = new SignedCertificateTimestampInfoView();
  layout->AddView(sct_info_view_);

  sct_info_view_->SetSignedCertificateTimestamp(*(sct_list_[0].sct),
                                                sct_list_[0].status);
}

void SignedCertificateTimestampsViews::ShowSCTInfo(int sct_index) {
  if ((sct_index < 0) || (static_cast<size_t>(sct_index) > sct_list_.size()))
    return;

  sct_info_view_->SetSignedCertificateTimestamp(*(sct_list_[sct_index].sct),
                                                sct_list_[sct_index].status);
}

void SignedCertificateTimestampsViews::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  GetWidget()->Close();
}
