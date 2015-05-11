// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/safe_media_metadata_parser.h"

#include "chrome/browser/extensions/blob_reader.h"
#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/utility_process_host.h"

using content::BrowserThread;

namespace metadata {

namespace {

// Completes the Blob byte request by forwarding it to the utility process.
void OnBlobReaderDone(
    const base::WeakPtr<content::UtilityProcessHost>& utility_process_host,
    int64 request_id,
    scoped_ptr<std::string> data,
    int64 /* blob_total_size */) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!utility_process_host.get())
    return;
  utility_process_host->Send(new ChromeUtilityMsg_RequestBlobBytes_Finished(
      request_id, *data));
}

}  // namespace

SafeMediaMetadataParser::SafeMediaMetadataParser(Profile* profile,
                                                 const std::string& blob_uuid,
                                                 int64 blob_size,
                                                 const std::string& mime_type)
    : profile_(profile),
      blob_uuid_(blob_uuid),
      blob_size_(blob_size),
      mime_type_(mime_type),
      parser_state_(INITIAL_STATE) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void SafeMediaMetadataParser::Start(const DoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&SafeMediaMetadataParser::StartWorkOnIOThread, this,
                 callback));
}

SafeMediaMetadataParser::~SafeMediaMetadataParser() {
}

void SafeMediaMetadataParser::StartWorkOnIOThread(
    const DoneCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK_EQ(INITIAL_STATE, parser_state_);
  DCHECK(!callback.is_null());

  callback_ = callback;

  utility_process_host_ = content::UtilityProcessHost::Create(
      this, base::MessageLoopProxy::current())->AsWeakPtr();

  utility_process_host_->Send(
      new ChromeUtilityMsg_ParseMediaMetadata(mime_type_, blob_size_));

  parser_state_ = STARTED_PARSING_STATE;
}

void SafeMediaMetadataParser::OnParseMediaMetadataFinished(
    bool parse_success, const base::DictionaryValue& metadata_dictionary) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback_.is_null());

  if (parser_state_ != STARTED_PARSING_STATE)
    return;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback_, parse_success,
                 base::Owned(metadata_dictionary.DeepCopy())));
  parser_state_ = FINISHED_PARSING_STATE;
}

void SafeMediaMetadataParser::OnUtilityProcessRequestBlobBytes(
    int64 request_id, int64 byte_start, int64 length) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  // BlobReader is self-deleting.
  BlobReader* reader = new BlobReader(
      profile_,
      blob_uuid_,
      base::Bind(&OnBlobReaderDone, utility_process_host_, request_id));
  reader->SetByteRange(byte_start, length);
  reader->Start();
}

void SafeMediaMetadataParser::OnProcessCrashed(int exit_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback_.is_null());

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback_, false, base::Owned(new base::DictionaryValue)));
  parser_state_ = FINISHED_PARSING_STATE;
}

bool SafeMediaMetadataParser::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SafeMediaMetadataParser, message)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_ParseMediaMetadata_Finished,
        OnParseMediaMetadataFinished)
    IPC_MESSAGE_HANDLER(
        ChromeUtilityHostMsg_RequestBlobBytes,
        OnUtilityProcessRequestBlobBytes)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

}  // namespace metadata
