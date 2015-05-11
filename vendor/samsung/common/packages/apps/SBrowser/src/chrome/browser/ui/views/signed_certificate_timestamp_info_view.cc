// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/signed_certificate_timestamp_info_view.h"

#include <algorithm>

#include "base/i18n/time_formatting.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "grit/generated_resources.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

// Adjustment to the spacing between subsequent label-field lines.
const int kExtraLineHeightPadding = 3;

int HashAlgorithmToResourceID(
    net::ct::DigitallySigned::HashAlgorithm hash_algorithm) {
  switch (hash_algorithm) {
    case net::ct::DigitallySigned::HASH_ALGO_NONE:
      return IDS_SCT_HASH_ALGORITHM_NONE;
    case net::ct::DigitallySigned::HASH_ALGO_MD5:
      return IDS_SCT_HASH_ALGORITHM_MD5;
    case net::ct::DigitallySigned::HASH_ALGO_SHA1:
      return IDS_SCT_HASH_ALGORITHM_SHA1;
    case net::ct::DigitallySigned::HASH_ALGO_SHA224:
      return IDS_SCT_HASH_ALGORITHM_SHA224;
    case net::ct::DigitallySigned::HASH_ALGO_SHA256:
      return IDS_SCT_HASH_ALGORITHM_SHA256;
    case net::ct::DigitallySigned::HASH_ALGO_SHA384:
      return IDS_SCT_HASH_ALGORITHM_SHA384;
    case net::ct::DigitallySigned::HASH_ALGO_SHA512:
      return IDS_SCT_HASH_ALGORITHM_SHA512;
  }
  return IDS_SCT_HASH_ALGORITHM_NONE;
}

int SignatureAlgorithmToResourceID(
    net::ct::DigitallySigned::SignatureAlgorithm signature_algorithm) {
  switch (signature_algorithm) {
    case net::ct::DigitallySigned::SIG_ALGO_ANONYMOUS:
      return IDS_SCT_SIGNATURE_ALGORITHM_ANONYMOUS;
    case net::ct::DigitallySigned::SIG_ALGO_RSA:
      return IDS_SCT_SIGNATURE_ALGORITHM_RSA;
    case net::ct::DigitallySigned::SIG_ALGO_DSA:
      return IDS_SCT_SIGNATURE_ALGORITHM_DSA;
    case net::ct::DigitallySigned::SIG_ALGO_ECDSA:
      return IDS_SCT_SIGNATURE_ALGORITHM_ECDSA;
  }
  return IDS_SCT_SIGNATURE_ALGORITHM_ANONYMOUS;
}

}  // namespace

namespace chrome {
namespace ct {

int StatusToResourceID(net::ct::SCTVerifyStatus status) {
  switch (status) {
    case net::ct::SCT_STATUS_NONE:
      return IDS_SCT_STATUS_NONE;
    case net::ct::SCT_STATUS_LOG_UNKNOWN:
      return IDS_SCT_STATUS_LOG_UNKNOWN;
    case net::ct::SCT_STATUS_INVALID:
      return IDS_SCT_STATUS_INVALID;
    case net::ct::SCT_STATUS_OK:
      return IDS_SCT_STATUS_OK;
    case net::ct::SCT_STATUS_MAX:
      break;
  }

  return IDS_SCT_STATUS_NONE;
}

int SCTOriginToResourceID(const net::ct::SignedCertificateTimestamp& sct) {
  switch (sct.origin) {
    case net::ct::SignedCertificateTimestamp::SCT_EMBEDDED:
      return IDS_SCT_ORIGIN_EMBEDDED;
    case net::ct::SignedCertificateTimestamp::SCT_FROM_TLS_EXTENSION:
      return IDS_SCT_ORIGIN_TLS_EXTENSION;
    case net::ct::SignedCertificateTimestamp::SCT_FROM_OCSP_RESPONSE:
      return IDS_SCT_ORIGIN_OCSP;
    case net::ct::SignedCertificateTimestamp::SCT_ORIGIN_MAX:
      break;
  }
  return IDS_SCT_ORIGIN_UNKNOWN;
}

}  // namespace ct
}  // namespace chrome

// SignedCertificateTimestampInfoView, public:

SignedCertificateTimestampInfoView::SignedCertificateTimestampInfoView()
    : status_value_field_(NULL),
      origin_value_field_(NULL),
      version_value_field_(NULL),
      log_id_value_field_(NULL),
      timestamp_value_field_(NULL),
      hash_algorithm_value_field_(NULL),
      signature_algorithm_value_field_(NULL),
      signature_data_value_field_(NULL) {}

SignedCertificateTimestampInfoView::~SignedCertificateTimestampInfoView() {}

void SignedCertificateTimestampInfoView::SetSignedCertificateTimestamp(
    const net::ct::SignedCertificateTimestamp& sct,
    net::ct::SCTVerifyStatus status) {
  status_value_field_->SetText(
      l10n_util::GetStringUTF16(chrome::ct::StatusToResourceID(status)));
  origin_value_field_->SetText(
      l10n_util::GetStringUTF16(chrome::ct::SCTOriginToResourceID(sct)));
  version_value_field_->SetText(base::IntToString16(sct.version));
  log_description_value_field_->SetText(base::UTF8ToUTF16(sct.log_description));
  timestamp_value_field_->SetText(
      base::TimeFormatFriendlyDateAndTime(sct.timestamp));

  hash_algorithm_value_field_->SetText(l10n_util::GetStringUTF16(
      HashAlgorithmToResourceID(sct.signature.hash_algorithm)));
  signature_algorithm_value_field_->SetText(l10n_util::GetStringUTF16(
      SignatureAlgorithmToResourceID(sct.signature.signature_algorithm)));

  // The log_id and signature_data fields contain binary data, format it
  // accordingly before displaying.
  log_id_value_field_->SetText(
      base::UTF8ToUTF16(x509_certificate_model::ProcessRawBytesWithSeparators(
          reinterpret_cast<const unsigned char*>(sct.log_id.c_str()),
          sct.log_id.length(),
          ' ',
          ' ')));
  signature_data_value_field_->SetText(
      base::UTF8ToUTF16(x509_certificate_model::ProcessRawBytesWithSeparators(
          reinterpret_cast<const unsigned char*>(
              sct.signature.signature_data.c_str()),
          sct.signature.signature_data.length(),
          ' ',
          ' ')));

  Layout();
}

void SignedCertificateTimestampInfoView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    Init();
}

void SignedCertificateTimestampInfoView::AddLabelRow(
    int layout_id,
    views::GridLayout* layout,
    int label_message_id,
    views::Textfield* textfield) {
  layout->StartRow(0, layout_id);
  layout->AddView(
      new views::Label(l10n_util::GetStringUTF16(label_message_id)));
  layout->AddView(
      textfield, 2, 1, views::GridLayout::FILL, views::GridLayout::CENTER);
  layout->AddPaddingRow(0, kExtraLineHeightPadding);

  textfield->SetReadOnly(true);
  // Color these borderless text areas the same as the containing dialog.
  textfield->SetBackgroundColor(SK_ColorTRANSPARENT);
  textfield->SetBorder(views::Border::NullBorder());
}

void SignedCertificateTimestampInfoView::Init() {
  status_value_field_ = new views::Textfield;
  origin_value_field_ = new views::Textfield;
  version_value_field_ = new views::Textfield;
  log_description_value_field_ = new views::Textfield;
  log_id_value_field_ = new views::Textfield;
  timestamp_value_field_ = new views::Textfield;
  hash_algorithm_value_field_ = new views::Textfield;
  signature_algorithm_value_field_ = new views::Textfield;
  signature_data_value_field_ = new views::Textfield;

  views::GridLayout* layout = new views::GridLayout(this);
  layout->SetInsets(
      0, views::kButtonHEdgeMarginNew, 0, views::kButtonHEdgeMarginNew);
  SetLayoutManager(layout);

  const int three_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(three_column_layout_id);
  column_set->AddColumn(views::GridLayout::LEADING,
                        views::GridLayout::CENTER,
                        0,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(views::GridLayout::TRAILING,
                        views::GridLayout::CENTER,
                        0,
                        views::GridLayout::USE_PREF,
                        0,
                        0);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::CENTER,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  AddLabelRow(three_column_layout_id,
              layout,
              IDS_SCT_VALIDATION_INFO,
              status_value_field_);
  AddLabelRow(
      three_column_layout_id, layout, IDS_SCT_ORIGIN, origin_value_field_);
  AddLabelRow(
      three_column_layout_id, layout, IDS_SCT_VERSION, version_value_field_);
  AddLabelRow(three_column_layout_id,
              layout,
              IDS_SCT_LOG_DESCRIPTION,
              log_description_value_field_);
  AddLabelRow(
      three_column_layout_id, layout, IDS_SCT_LOGID, log_id_value_field_);
  AddLabelRow(three_column_layout_id,
              layout,
              IDS_SCT_TIMESTAMP,
              timestamp_value_field_);
  AddLabelRow(three_column_layout_id,
              layout,
              IDS_SCT_HASH_ALGORITHM,
              hash_algorithm_value_field_);
  AddLabelRow(three_column_layout_id,
              layout,
              IDS_SCT_SIGNATURE_ALGORITHM,
              signature_algorithm_value_field_);
  AddLabelRow(three_column_layout_id,
              layout,
              IDS_SCT_SIGNATURE_DATA,
              signature_data_value_field_);
}
