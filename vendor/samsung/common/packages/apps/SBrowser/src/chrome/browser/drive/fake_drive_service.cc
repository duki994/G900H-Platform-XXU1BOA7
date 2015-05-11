// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/fake_drive_service.h"

#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/test_util.h"
#include "google_apis/drive/time_util.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"

using content::BrowserThread;
using google_apis::AboutResource;
using google_apis::AboutResourceCallback;
using google_apis::AccountMetadata;
using google_apis::AppList;
using google_apis::AppListCallback;
using google_apis::AuthStatusCallback;
using google_apis::AuthorizeAppCallback;
using google_apis::CancelCallback;
using google_apis::ChangeResource;
using google_apis::DownloadActionCallback;
using google_apis::EntryActionCallback;
using google_apis::FileResource;
using google_apis::GDATA_FILE_ERROR;
using google_apis::GDATA_NO_CONNECTION;
using google_apis::GDATA_OTHER_ERROR;
using google_apis::GDataErrorCode;
using google_apis::GetContentCallback;
using google_apis::GetResourceEntryCallback;
using google_apis::GetResourceListCallback;
using google_apis::GetShareUrlCallback;
using google_apis::HTTP_BAD_REQUEST;
using google_apis::HTTP_CREATED;
using google_apis::HTTP_NOT_FOUND;
using google_apis::HTTP_NO_CONTENT;
using google_apis::HTTP_PRECONDITION;
using google_apis::HTTP_RESUME_INCOMPLETE;
using google_apis::HTTP_SUCCESS;
using google_apis::InitiateUploadCallback;
using google_apis::Link;
using google_apis::ParentReference;
using google_apis::ProgressCallback;
using google_apis::ResourceEntry;
using google_apis::ResourceList;
using google_apis::UploadRangeCallback;
using google_apis::UploadRangeResponse;
namespace test_util = google_apis::test_util;

namespace drive {
namespace {

// Mime type of directories.
const char kDriveFolderMimeType[] = "application/vnd.google-apps.folder";

// Returns true if a resource entry matches with the search query.
// Supports queries consist of following format.
// - Phrases quoted by double/single quotes
// - AND search for multiple words/phrases segmented by space
// - Limited attribute search.  Only "title:" is supported.
bool EntryMatchWithQuery(const ResourceEntry& entry,
                         const std::string& query) {
  base::StringTokenizer tokenizer(query, " ");
  tokenizer.set_quote_chars("\"'");
  while (tokenizer.GetNext()) {
    std::string key, value;
    const std::string& token = tokenizer.token();
    if (token.find(':') == std::string::npos) {
      base::TrimString(token, "\"'", &value);
    } else {
      base::StringTokenizer key_value(token, ":");
      key_value.set_quote_chars("\"'");
      if (!key_value.GetNext())
        return false;
      key = key_value.token();
      if (!key_value.GetNext())
        return false;
      base::TrimString(key_value.token(), "\"'", &value);
    }

    // TODO(peria): Deal with other attributes than title.
    if (!key.empty() && key != "title")
      return false;
    // Search query in the title.
    if (entry.title().find(value) == std::string::npos)
      return false;
  }
  return true;
}

void ScheduleUploadRangeCallback(const UploadRangeCallback& callback,
                                 int64 start_position,
                                 int64 end_position,
                                 GDataErrorCode error,
                                 scoped_ptr<ResourceEntry> entry) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 UploadRangeResponse(error,
                                     start_position,
                                     end_position),
                 base::Passed(&entry)));
}

void EntryActionCallbackAdapter(
    const EntryActionCallback& callback,
    GDataErrorCode error, scoped_ptr<ResourceEntry> resource_entry) {
  callback.Run(error);
}

}  // namespace

struct FakeDriveService::EntryInfo {
  google_apis::ChangeResource change_resource;
  GURL share_url;
  std::string content_data;
};

struct FakeDriveService::UploadSession {
  std::string content_type;
  int64 content_length;
  std::string parent_resource_id;
  std::string resource_id;
  std::string etag;
  std::string title;

  int64 uploaded_size;

  UploadSession()
      : content_length(0),
        uploaded_size(0) {}

  UploadSession(
      std::string content_type,
      int64 content_length,
      std::string parent_resource_id,
      std::string resource_id,
      std::string etag,
      std::string title)
    : content_type(content_type),
      content_length(content_length),
      parent_resource_id(parent_resource_id),
      resource_id(resource_id),
      etag(etag),
      title(title),
      uploaded_size(0) {
  }
};

FakeDriveService::FakeDriveService()
    : about_resource_(new AboutResource),
      published_date_seq_(0),
      next_upload_sequence_number_(0),
      default_max_results_(0),
      resource_id_count_(0),
      resource_list_load_count_(0),
      change_list_load_count_(0),
      directory_load_count_(0),
      about_resource_load_count_(0),
      app_list_load_count_(0),
      blocked_resource_list_load_count_(0),
      offline_(false),
      never_return_all_resource_list_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

FakeDriveService::~FakeDriveService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  STLDeleteValues(&entries_);
}

bool FakeDriveService::LoadResourceListForWapi(
    const std::string& relative_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<base::Value> raw_value = test_util::LoadJSONFile(relative_path);
  base::DictionaryValue* as_dict = NULL;
  scoped_ptr<base::Value> feed;
  base::DictionaryValue* feed_as_dict = NULL;

  // Extract the "feed" from the raw value and take the ownership.
  // Note that Remove() transfers the ownership to |feed|.
  if (raw_value->GetAsDictionary(&as_dict) &&
      as_dict->Remove("feed", &feed) &&
      feed->GetAsDictionary(&feed_as_dict)) {
    base::ListValue* entries = NULL;
    if (feed_as_dict->GetList("entry", &entries)) {
      for (size_t i = 0; i < entries->GetSize(); ++i) {
        base::DictionaryValue* entry = NULL;
        if (entries->GetDictionary(i, &entry)) {
          scoped_ptr<ResourceEntry> resource_entry =
              ResourceEntry::CreateFrom(*entry);

          const std::string resource_id = resource_entry->resource_id();
          EntryInfoMap::iterator it = entries_.find(resource_id);
          if (it == entries_.end()) {
            it = entries_.insert(
                std::make_pair(resource_id, new EntryInfo)).first;
          }
          EntryInfo* new_entry = it->second;

          ChangeResource* change = &new_entry->change_resource;
          change->set_change_id(resource_entry->changestamp());
          change->set_file_id(resource_id);
          change->set_file(
              util::ConvertResourceEntryToFileResource(*resource_entry));

          const Link* share_url =
              resource_entry->GetLinkByType(Link::LINK_SHARE);
          if (share_url)
            new_entry->share_url = share_url->href();

          entry->GetString("test$data", &new_entry->content_data);
        }
      }
    }
  }

  return feed_as_dict;
}

bool FakeDriveService::LoadAccountMetadataForWapi(
    const std::string& relative_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<base::Value> value = test_util::LoadJSONFile(relative_path);
  if (!value)
    return false;

  about_resource_ = util::ConvertAccountMetadataToAboutResource(
      *AccountMetadata::CreateFrom(*value), GetRootResourceId());
  if (!about_resource_)
    return false;

  // Add the largest changestamp to the existing entries.
  // This will be used to generate change lists in GetResourceList().
  for (EntryInfoMap::iterator it = entries_.begin(); it != entries_.end();
       ++it) {
    it->second->change_resource.set_change_id(
        about_resource_->largest_change_id());
  }
  return true;
}

bool FakeDriveService::LoadAppListForDriveApi(
    const std::string& relative_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Load JSON data, which must be a dictionary.
  scoped_ptr<base::Value> value = test_util::LoadJSONFile(relative_path);
  CHECK_EQ(base::Value::TYPE_DICTIONARY, value->GetType());
  app_info_value_.reset(
      static_cast<base::DictionaryValue*>(value.release()));
  return app_info_value_;
}

void FakeDriveService::SetQuotaValue(int64 used, int64 total) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  about_resource_->set_quota_bytes_used(used);
  about_resource_->set_quota_bytes_total(total);
}

GURL FakeDriveService::GetFakeLinkUrl(const std::string& resource_id) {
  return GURL("https://fake_server/" + net::EscapePath(resource_id));
}

void FakeDriveService::Initialize(const std::string& account_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveService::AddObserver(DriveServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveService::RemoveObserver(DriveServiceObserver* observer) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

bool FakeDriveService::CanSendRequest() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return true;
}

ResourceIdCanonicalizer FakeDriveService::GetResourceIdCanonicalizer() const {
  return util::GetIdentityResourceIdCanonicalizer();
}

bool FakeDriveService::HasAccessToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return true;
}

void FakeDriveService::RequestAccessToken(const AuthStatusCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  callback.Run(google_apis::HTTP_NOT_MODIFIED, "fake_access_token");
}

bool FakeDriveService::HasRefreshToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return true;
}

void FakeDriveService::ClearAccessToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void FakeDriveService::ClearRefreshToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

std::string FakeDriveService::GetRootResourceId() const {
  return "fake_root";
}

CancelCallback FakeDriveService::GetAllResourceList(
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (never_return_all_resource_list_) {
    ++blocked_resource_list_load_count_;
    return CancelCallback();
  }

  GetResourceListInternal(0,  // start changestamp
                          std::string(),  // empty search query
                          std::string(),  // no directory resource id,
                          0,  // start offset
                          default_max_results_,
                          &resource_list_load_count_,
                          callback);
  return CancelCallback();
}

CancelCallback FakeDriveService::GetResourceListInDirectory(
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!directory_resource_id.empty());
  DCHECK(!callback.is_null());

  GetResourceListInternal(0,  // start changestamp
                          std::string(),  // empty search query
                          directory_resource_id,
                          0,  // start offset
                          default_max_results_,
                          &directory_load_count_,
                          callback);
  return CancelCallback();
}

CancelCallback FakeDriveService::Search(
    const std::string& search_query,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!search_query.empty());
  DCHECK(!callback.is_null());

  GetResourceListInternal(0,  // start changestamp
                          search_query,
                          std::string(),  // no directory resource id,
                          0,  // start offset
                          default_max_results_,
                          NULL,
                          callback);
  return CancelCallback();
}

CancelCallback FakeDriveService::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!title.empty());
  DCHECK(!callback.is_null());

  // Note: the search implementation here doesn't support quotation unescape,
  // so don't escape here.
  GetResourceListInternal(0,  // start changestamp
                          base::StringPrintf("title:'%s'", title.c_str()),
                          directory_resource_id,
                          0,  // start offset
                          default_max_results_,
                          NULL,
                          callback);
  return CancelCallback();
}

CancelCallback FakeDriveService::GetChangeList(
    int64 start_changestamp,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GetResourceListInternal(start_changestamp,
                          std::string(),  // empty search query
                          std::string(),  // no directory resource id,
                          0,  // start offset
                          default_max_results_,
                          &change_list_load_count_,
                          callback);
  return CancelCallback();
}

CancelCallback FakeDriveService::GetRemainingChangeList(
    const GURL& next_link,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!next_link.is_empty());
  DCHECK(!callback.is_null());

  return GetRemainingResourceList(next_link, callback);
}

CancelCallback FakeDriveService::GetRemainingFileList(
    const GURL& next_link,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!next_link.is_empty());
  DCHECK(!callback.is_null());

  return GetRemainingResourceList(next_link, callback);
}

CancelCallback FakeDriveService::GetResourceEntry(
    const std::string& resource_id,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    scoped_ptr<ResourceEntry> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    scoped_ptr<ResourceEntry> resource_entry =
        util::ConvertChangeResourceToResourceEntry(entry->change_resource);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, base::Passed(&resource_entry)));
    return CancelCallback();
  }

  scoped_ptr<ResourceEntry> null;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_FOUND, base::Passed(&null)));
  return CancelCallback();
}

CancelCallback FakeDriveService::GetShareUrl(
    const std::string& resource_id,
    const GURL& /* embed_origin */,
    const GetShareUrlCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    scoped_ptr<ResourceEntry> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   GURL()));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, entry->share_url));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_FOUND, GURL()));
  return CancelCallback();
}

CancelCallback FakeDriveService::GetAboutResource(
    const AboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    scoped_ptr<AboutResource> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION, base::Passed(&null)));
    return CancelCallback();
  }

  ++about_resource_load_count_;
  scoped_ptr<AboutResource> about_resource(new AboutResource(*about_resource_));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 HTTP_SUCCESS, base::Passed(&about_resource)));
  return CancelCallback();
}

CancelCallback FakeDriveService::GetAppList(const AppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(app_info_value_);

  if (offline_) {
    scoped_ptr<AppList> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return CancelCallback();
  }

  ++app_list_load_count_;
  scoped_ptr<AppList> app_list(AppList::CreateFrom(*app_info_value_));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&app_list)));
  return CancelCallback();
}

CancelCallback FakeDriveService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    ChangeResource* change = &entry->change_resource;
    const FileResource* file = change->file();
    if (change->is_deleted()) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
      return CancelCallback();
    }

    if (!etag.empty() && etag != file->etag()) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(callback, HTTP_PRECONDITION));
      return CancelCallback();
    }

    change->set_deleted(true);
    AddNewChangestamp(change);
    change->set_file(scoped_ptr<FileResource>());
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, HTTP_NO_CONTENT));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
  return CancelCallback();
}

CancelCallback FakeDriveService::TrashResource(
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    ChangeResource* change = &entry->change_resource;
    FileResource* file = change->mutable_file();
    GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
    if (change->is_deleted() || file->labels().is_trashed()) {
      error = HTTP_NOT_FOUND;
    } else {
      file->mutable_labels()->set_trashed(true);
      AddNewChangestamp(change);
      error = HTTP_SUCCESS;
    }
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, error));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
  return CancelCallback();
}

CancelCallback FakeDriveService::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download_action_callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(download_action_callback,
                   GDATA_NO_CONNECTION,
                   base::FilePath()));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(download_action_callback, HTTP_NOT_FOUND, base::FilePath()));
    return CancelCallback();
  }

  const FileResource* file = entry->change_resource.file();
  const std::string& content_data = entry->content_data;
  int64 file_size = file->file_size();
  DCHECK_EQ(static_cast<size_t>(file_size), content_data.size());

  if (!get_content_callback.is_null()) {
    const int64 kBlockSize = 5;
    for (int64 i = 0; i < file_size; i += kBlockSize) {
      const int64 size = std::min(kBlockSize, file_size - i);
      scoped_ptr<std::string> content_for_callback(
          new std::string(content_data.substr(i, size)));
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(get_content_callback, HTTP_SUCCESS,
                     base::Passed(&content_for_callback)));
    }
  }

  if (test_util::WriteStringToFile(local_cache_path, content_data)) {
    if (!progress_callback.is_null()) {
      // See also the comment in ResumeUpload(). For testing that clients
      // can handle the case progress_callback is called multiple times,
      // here we invoke the callback twice.
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(progress_callback, file_size / 2, file_size));
      base::MessageLoopProxy::current()->PostTask(
          FROM_HERE,
          base::Bind(progress_callback, file_size, file_size));
    }
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(download_action_callback,
                   HTTP_SUCCESS,
                   local_cache_path));
    return CancelCallback();
  }

  // Failed to write the content.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(download_action_callback, GDATA_FILE_ERROR, base::FilePath()));
  return CancelCallback();
}

CancelCallback FakeDriveService::CopyResource(
    const std::string& resource_id,
    const std::string& in_parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    scoped_ptr<ResourceEntry> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return CancelCallback();
  }

  const std::string& parent_resource_id = in_parent_resource_id.empty() ?
      GetRootResourceId() : in_parent_resource_id;

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    // Make a copy and set the new resource ID and the new title.
    scoped_ptr<EntryInfo> copied_entry(new EntryInfo);
    copied_entry->content_data = entry->content_data;
    copied_entry->share_url = entry->share_url;

    // TODO(hashimoto): Implement a proper way to copy FileResource.
    scoped_ptr<ResourceEntry> copied_resource_entry =
        util::ConvertChangeResourceToResourceEntry(entry->change_resource);
    copied_entry->change_resource.set_file(
        util::ConvertResourceEntryToFileResource(*copied_resource_entry));

    ChangeResource* new_change = &copied_entry->change_resource;
    FileResource* new_file = new_change->mutable_file();
    const std::string new_resource_id = GetNewResourceId();
    new_change->set_file_id(new_resource_id);
    new_file->set_file_id(new_resource_id);
    new_file->set_title(new_title);

    scoped_ptr<ParentReference> parent(new ParentReference);
    parent->set_file_id(parent_resource_id);
    parent->set_parent_link(GetFakeLinkUrl(parent_resource_id));
    parent->set_is_root(parent_resource_id == GetRootResourceId());
    ScopedVector<ParentReference> parents;
    parents.push_back(parent.release());
    new_file->set_parents(parents.Pass());

    if (!last_modified.is_null())
      new_file->set_modified_date(last_modified);

    AddNewChangestamp(new_change);
    UpdateETag(new_file);

    scoped_ptr<ResourceEntry> resource_entry =
        util::ConvertChangeResourceToResourceEntry(*new_change);
    // Add the new entry to the map.
    entries_[new_resource_id] = copied_entry.release();

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   HTTP_SUCCESS,
                   base::Passed(&resource_entry)));
    return CancelCallback();
  }

  scoped_ptr<ResourceEntry> null;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_FOUND, base::Passed(&null)));
  return CancelCallback();
}

CancelCallback FakeDriveService::UpdateResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const base::Time& last_viewed_by_me,
    const google_apis::GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION,
                              base::Passed(scoped_ptr<ResourceEntry>())));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    ChangeResource* change = &entry->change_resource;
    FileResource* file = change->mutable_file();
    file->set_title(new_title);

    // Set parent if necessary.
    if (!parent_resource_id.empty()) {
      scoped_ptr<ParentReference> parent(new ParentReference);
      parent->set_file_id(parent_resource_id);
      parent->set_parent_link(GetFakeLinkUrl(parent_resource_id));
      parent->set_is_root(parent_resource_id == GetRootResourceId());

      ScopedVector<ParentReference> parents;
      parents.push_back(parent.release());
      file->set_parents(parents.Pass());
    }

    if (!last_modified.is_null())
      file->set_modified_date(last_modified);

    if (!last_viewed_by_me.is_null())
      file->set_last_viewed_by_me_date(last_viewed_by_me);

    AddNewChangestamp(change);
    UpdateETag(file);

    scoped_ptr<ResourceEntry> resource_entry =
        util::ConvertChangeResourceToResourceEntry(*change);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, base::Passed(&resource_entry)));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_NOT_FOUND,
                 base::Passed(scoped_ptr<ResourceEntry>())));
  return CancelCallback();
}

CancelCallback FakeDriveService::RenameResource(
    const std::string& resource_id,
    const std::string& new_title,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  return UpdateResource(
      resource_id, std::string(), new_title, base::Time(), base::Time(),
      base::Bind(&EntryActionCallbackAdapter, callback));
}

CancelCallback FakeDriveService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    ChangeResource* change = &entry->change_resource;
    // On the real Drive server, resources do not necessary shape a tree
    // structure. That is, each resource can have multiple parent.
    // We mimic the behavior here; AddResourceToDirectoy just adds
    // one more parent, not overwriting old ones.
    scoped_ptr<ParentReference> parent(new ParentReference);
    parent->set_file_id(parent_resource_id);
    parent->set_parent_link(GetFakeLinkUrl(parent_resource_id));
    parent->set_is_root(parent_resource_id == GetRootResourceId());
    change->mutable_file()->mutable_parents()->push_back(parent.release());

    AddNewChangestamp(change);
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, HTTP_SUCCESS));
    return CancelCallback();
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
  return CancelCallback();
}

CancelCallback FakeDriveService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(callback, GDATA_NO_CONNECTION));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (entry) {
    ChangeResource* change = &entry->change_resource;
    FileResource* file = change->mutable_file();
    ScopedVector<ParentReference>* parents = file->mutable_parents();
    for (size_t i = 0; i < parents->size(); ++i) {
      if ((*parents)[i]->file_id() == parent_resource_id) {
        parents->erase(parents->begin() + i);
        AddNewChangestamp(change);
        base::MessageLoop::current()->PostTask(
            FROM_HERE, base::Bind(callback, HTTP_NO_CONTENT));
        return CancelCallback();
      }
    }
  }

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, HTTP_NOT_FOUND));
  return CancelCallback();
}

CancelCallback FakeDriveService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const AddNewDirectoryOptions& options,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    scoped_ptr<ResourceEntry> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return CancelCallback();
  }

  const EntryInfo* new_entry = AddNewEntry(kDriveFolderMimeType,
                                           "",  // content_data
                                           parent_resource_id,
                                           directory_title,
                                           false);  // shared_with_me
  if (!new_entry) {
    scoped_ptr<ResourceEntry> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_NOT_FOUND, base::Passed(&null)));
    return CancelCallback();
  }

  scoped_ptr<ResourceEntry> parsed_entry(
      util::ConvertChangeResourceToResourceEntry(new_entry->change_resource));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_CREATED, base::Passed(&parsed_entry)));
  return CancelCallback();
}

CancelCallback FakeDriveService::InitiateUploadNewFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const InitiateUploadNewFileOptions& options,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, GDATA_NO_CONNECTION, GURL()));
    return CancelCallback();
  }

  if (parent_resource_id != GetRootResourceId() &&
      !entries_.count(parent_resource_id)) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_NOT_FOUND, GURL()));
    return CancelCallback();
  }

  GURL session_url = GetNewUploadSessionUrl();
  upload_sessions_[session_url] =
      UploadSession(content_type, content_length,
                    parent_resource_id,
                    "",  // resource_id
                    "",  // etag
                    title);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, session_url));
  return CancelCallback();
}

CancelCallback FakeDriveService::InitiateUploadExistingFile(
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const InitiateUploadExistingFileOptions& options,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, GDATA_NO_CONNECTION, GURL()));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_NOT_FOUND, GURL()));
    return CancelCallback();
  }

  FileResource* file = entry->change_resource.mutable_file();
  if (!options.etag.empty() && options.etag != file->etag()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_PRECONDITION, GURL()));
    return CancelCallback();
  }
  // TODO(hashimoto): Update |file|'s metadata with |options|.

  GURL session_url = GetNewUploadSessionUrl();
  upload_sessions_[session_url] =
      UploadSession(content_type, content_length,
                    "",  // parent_resource_id
                    resource_id,
                    file->etag(),
                    "" /* title */);

  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, session_url));
  return CancelCallback();
}

CancelCallback FakeDriveService::GetUploadStatus(
    const GURL& upload_url,
    int64 content_length,
    const UploadRangeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  return CancelCallback();
}

CancelCallback FakeDriveService::ResumeUpload(
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const UploadRangeCallback& callback,
      const ProgressCallback& progress_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  GetResourceEntryCallback completion_callback
      = base::Bind(&ScheduleUploadRangeCallback,
                   callback, start_position, end_position);

  if (offline_) {
    completion_callback.Run(GDATA_NO_CONNECTION, scoped_ptr<ResourceEntry>());
    return CancelCallback();
  }

  if (!upload_sessions_.count(upload_url)) {
    completion_callback.Run(HTTP_NOT_FOUND, scoped_ptr<ResourceEntry>());
    return CancelCallback();
  }

  UploadSession* session = &upload_sessions_[upload_url];

  // Chunks are required to be sent in such a ways that they fill from the start
  // of the not-yet-uploaded part with no gaps nor overlaps.
  if (session->uploaded_size != start_position) {
    completion_callback.Run(HTTP_BAD_REQUEST, scoped_ptr<ResourceEntry>());
    return CancelCallback();
  }

  if (!progress_callback.is_null()) {
    // In the real GDataWapi/Drive DriveService, progress is reported in
    // nondeterministic timing. In this fake implementation, we choose to call
    // it twice per one ResumeUpload. This is for making sure that client code
    // works fine even if the callback is invoked more than once; it is the
    // crucial difference of the progress callback from others.
    // Note that progress is notified in the relative offset in each chunk.
    const int64 chunk_size = end_position - start_position;
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(progress_callback, chunk_size / 2, chunk_size));
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(progress_callback, chunk_size, chunk_size));
  }

  if (content_length != end_position) {
    session->uploaded_size = end_position;
    completion_callback.Run(HTTP_RESUME_INCOMPLETE,
                            scoped_ptr<ResourceEntry>());
    return CancelCallback();
  }

  std::string content_data;
  if (!base::ReadFileToString(local_file_path, &content_data)) {
    session->uploaded_size = end_position;
    completion_callback.Run(GDATA_FILE_ERROR, scoped_ptr<ResourceEntry>());
    return CancelCallback();
  }
  session->uploaded_size = end_position;

  // |resource_id| is empty if the upload is for new file.
  if (session->resource_id.empty()) {
    DCHECK(!session->parent_resource_id.empty());
    DCHECK(!session->title.empty());
    const EntryInfo* new_entry = AddNewEntry(
        session->content_type,
        content_data,
        session->parent_resource_id,
        session->title,
        false);  // shared_with_me
    if (!new_entry) {
      completion_callback.Run(HTTP_NOT_FOUND, scoped_ptr<ResourceEntry>());
      return CancelCallback();
    }

    completion_callback.Run(
        HTTP_CREATED,
        util::ConvertChangeResourceToResourceEntry(new_entry->change_resource));
    return CancelCallback();
  }

  EntryInfo* entry = FindEntryByResourceId(session->resource_id);
  if (!entry) {
    completion_callback.Run(HTTP_NOT_FOUND, scoped_ptr<ResourceEntry>());
    return CancelCallback();
  }

  ChangeResource* change = &entry->change_resource;
  FileResource* file = change->mutable_file();
  if (file->etag().empty() || session->etag != file->etag()) {
    completion_callback.Run(HTTP_PRECONDITION, scoped_ptr<ResourceEntry>());
    return CancelCallback();
  }

  file->set_md5_checksum(base::MD5String(content_data));
  entry->content_data = content_data;
  file->set_file_size(end_position);
  AddNewChangestamp(change);
  UpdateETag(file);

  completion_callback.Run(HTTP_SUCCESS,
                          util::ConvertChangeResourceToResourceEntry(*change));
  return CancelCallback();
}

CancelCallback FakeDriveService::AuthorizeApp(
    const std::string& resource_id,
    const std::string& app_id,
    const AuthorizeAppCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  return CancelCallback();
}

CancelCallback FakeDriveService::UninstallApp(
    const std::string& app_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Find app_id from app_info_value_ and delete.
  google_apis::GDataErrorCode error = google_apis::HTTP_NOT_FOUND;
  if (offline_) {
    error = google_apis::GDATA_NO_CONNECTION;
  } else {
    base::ListValue* items = NULL;
    if (app_info_value_->GetList("items", &items)) {
      for (size_t i = 0; i < items->GetSize(); ++i) {
        base::DictionaryValue* item = NULL;
        std::string id;
        if (items->GetDictionary(i, &item) && item->GetString("id", &id) &&
            id == app_id) {
          if (items->Remove(i, NULL))
            error = google_apis::HTTP_NO_CONTENT;
          break;
        }
      }
    }
  }

  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, error));
  return CancelCallback();
}

CancelCallback FakeDriveService::GetResourceListInDirectoryByWapi(
    const std::string& directory_resource_id,
    const google_apis::GetResourceListCallback& callback) {
  return GetResourceListInDirectory(
      directory_resource_id == util::kWapiRootDirectoryResourceId ?
          GetRootResourceId() :
          directory_resource_id,
      callback);
}

CancelCallback FakeDriveService::GetRemainingResourceList(
    const GURL& next_link,
    const google_apis::GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!next_link.is_empty());
  DCHECK(!callback.is_null());

  // "changestamp", "q", "parent" and "start-offset" are parameters to
  // implement "paging" of the result on FakeDriveService.
  // The URL should be the one filled in GetResourceListInternal of the
  // previous method invocation, so it should start with "http://localhost/?".
  // See also GetResourceListInternal.
  DCHECK_EQ(next_link.host(), "localhost");
  DCHECK_EQ(next_link.path(), "/");

  int64 start_changestamp = 0;
  std::string search_query;
  std::string directory_resource_id;
  int start_offset = 0;
  int max_results = default_max_results_;
  std::vector<std::pair<std::string, std::string> > parameters;
  if (base::SplitStringIntoKeyValuePairs(
          next_link.query(), '=', '&', &parameters)) {
    for (size_t i = 0; i < parameters.size(); ++i) {
      if (parameters[i].first == "changestamp") {
        base::StringToInt64(parameters[i].second, &start_changestamp);
      } else if (parameters[i].first == "q") {
        search_query =
            net::UnescapeURLComponent(parameters[i].second,
                                      net::UnescapeRule::URL_SPECIAL_CHARS);
      } else if (parameters[i].first == "parent") {
        directory_resource_id =
            net::UnescapeURLComponent(parameters[i].second,
                                      net::UnescapeRule::URL_SPECIAL_CHARS);
      } else if (parameters[i].first == "start-offset") {
        base::StringToInt(parameters[i].second, &start_offset);
      } else if (parameters[i].first == "max-results") {
        base::StringToInt(parameters[i].second, &max_results);
      }
    }
  }

  GetResourceListInternal(
      start_changestamp, search_query, directory_resource_id,
      start_offset, max_results, NULL, callback);
  return CancelCallback();
}

void FakeDriveService::AddNewFile(const std::string& content_type,
                                  const std::string& content_data,
                                  const std::string& parent_resource_id,
                                  const std::string& title,
                                  bool shared_with_me,
                                  const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    scoped_ptr<ResourceEntry> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return;
  }

  const EntryInfo* new_entry = AddNewEntry(content_type,
                                           content_data,
                                           parent_resource_id,
                                           title,
                                           shared_with_me);
  if (!new_entry) {
    scoped_ptr<ResourceEntry> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_NOT_FOUND, base::Passed(&null)));
    return;
  }

  scoped_ptr<ResourceEntry> parsed_entry(
      util::ConvertChangeResourceToResourceEntry(new_entry->change_resource));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_CREATED, base::Passed(&parsed_entry)));
}

void FakeDriveService::SetLastModifiedTime(
    const std::string& resource_id,
    const base::Time& last_modified_time,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (offline_) {
    scoped_ptr<ResourceEntry> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(&null)));
    return;
  }

  EntryInfo* entry = FindEntryByResourceId(resource_id);
  if (!entry) {
    scoped_ptr<ResourceEntry> null;
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, HTTP_NOT_FOUND, base::Passed(&null)));
    return;
  }

  ChangeResource* change = &entry->change_resource;
  FileResource* file = change->mutable_file();
  file->set_modified_date(last_modified_time);

  scoped_ptr<ResourceEntry> parsed_entry(
      util::ConvertChangeResourceToResourceEntry(*change));
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&parsed_entry)));
}

FakeDriveService::EntryInfo* FakeDriveService::FindEntryByResourceId(
    const std::string& resource_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  EntryInfoMap::iterator it = entries_.find(resource_id);
  // Deleted entries don't have FileResource.
  return it != entries_.end() && it->second->change_resource.file() ?
      it->second : NULL;
}

std::string FakeDriveService::GetNewResourceId() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  ++resource_id_count_;
  return base::StringPrintf("resource_id_%d", resource_id_count_);
}

void FakeDriveService::UpdateETag(google_apis::FileResource* file) {
  file->set_etag(
      "etag_" + base::Int64ToString(about_resource_->largest_change_id()));
}

void FakeDriveService::AddNewChangestamp(google_apis::ChangeResource* change) {
  about_resource_->set_largest_change_id(
      about_resource_->largest_change_id() + 1);
  change->set_change_id(about_resource_->largest_change_id());
}

const FakeDriveService::EntryInfo* FakeDriveService::AddNewEntry(
    const std::string& content_type,
    const std::string& content_data,
    const std::string& parent_resource_id,
    const std::string& title,
    bool shared_with_me) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!parent_resource_id.empty() &&
      parent_resource_id != GetRootResourceId() &&
      !entries_.count(parent_resource_id)) {
    return NULL;
  }

  std::string resource_id = GetNewResourceId();
  GURL upload_url = GURL("https://xxx/upload/" + resource_id);

  scoped_ptr<EntryInfo> new_entry(new EntryInfo);
  ChangeResource* new_change = &new_entry->change_resource;
  FileResource* new_file = new FileResource;
  new_change->set_file(make_scoped_ptr(new_file));

  // Set the resource ID and the title
  new_change->set_file_id(resource_id);
  new_file->set_file_id(resource_id);
  new_file->set_title(title);
  // Set the contents, size and MD5 for a file.
  if (content_type != kDriveFolderMimeType) {
    new_entry->content_data = content_data;
    new_file->set_file_size(content_data.size());
    new_file->set_md5_checksum(base::MD5String(content_data));
  }

  if (shared_with_me) {
    // Set current time to mark the file as shared_with_me.
    new_file->set_shared_with_me_date(base::Time::Now());
  }

  std::string escaped_resource_id = net::EscapePath(resource_id);

  // Set download URL and mime type.
  new_file->set_download_url(
      GURL("https://xxx/content/" + escaped_resource_id));
  new_file->set_mime_type(content_type);

  // Set parents.
  scoped_ptr<ParentReference> parent(new ParentReference);
  if (parent_resource_id.empty())
    parent->set_file_id(GetRootResourceId());
  else
    parent->set_file_id(parent_resource_id);
  parent->set_parent_link(GetFakeLinkUrl(parent->file_id()));
  parent->set_is_root(parent->file_id() == GetRootResourceId());
  ScopedVector<ParentReference> parents;
  parents.push_back(parent.release());
  new_file->set_parents(parents.Pass());

  new_file->set_self_link(GURL("https://xxx/edit/" + escaped_resource_id));

  new_entry->share_url = net::AppendOrReplaceQueryParameter(
      share_url_base_, "name", title);

  AddNewChangestamp(new_change);
  UpdateETag(new_file);

  base::Time published_date =
      base::Time() + base::TimeDelta::FromMilliseconds(++published_date_seq_);
  new_file->set_created_date(published_date);

  EntryInfo* raw_new_entry = new_entry.release();
  entries_[resource_id] = raw_new_entry;
  return raw_new_entry;
}

void FakeDriveService::GetResourceListInternal(
    int64 start_changestamp,
    const std::string& search_query,
    const std::string& directory_resource_id,
    int start_offset,
    int max_results,
    int* load_counter,
    const GetResourceListCallback& callback) {
  if (offline_) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   GDATA_NO_CONNECTION,
                   base::Passed(scoped_ptr<ResourceList>())));
    return;
  }

  // Filter out entries per parameters like |directory_resource_id| and
  // |search_query|.
  ScopedVector<ResourceEntry> entries;
  int num_entries_matched = 0;
  for (EntryInfoMap::iterator it = entries_.begin(); it != entries_.end();
       ++it) {
    scoped_ptr<ResourceEntry> entry =
        util::ConvertChangeResourceToResourceEntry(it->second->change_resource);
    bool should_exclude = false;

    // If |directory_resource_id| is set, exclude the entry if it's not in
    // the target directory.
    if (!directory_resource_id.empty()) {
      // Get the parent resource ID of the entry.
      std::string parent_resource_id;
      const google_apis::Link* parent_link =
          entry->GetLinkByType(Link::LINK_PARENT);
      if (parent_link) {
        parent_resource_id =
            net::UnescapeURLComponent(parent_link->href().ExtractFileName(),
                                      net::UnescapeRule::URL_SPECIAL_CHARS);
      }
      if (directory_resource_id != parent_resource_id)
        should_exclude = true;
    }

    // If |search_query| is set, exclude the entry if it does not contain the
    // search query in the title.
    if (!should_exclude && !search_query.empty() &&
        !EntryMatchWithQuery(*entry, search_query)) {
      should_exclude = true;
    }

    // If |start_changestamp| is set, exclude the entry if the
    // changestamp is older than |largest_changestamp|.
    // See https://developers.google.com/google-apps/documents-list/
    // #retrieving_all_changes_since_a_given_changestamp
    if (start_changestamp > 0 && entry->changestamp() < start_changestamp)
      should_exclude = true;

    // If the caller requests other list than change list by specifying
    // zero-|start_changestamp|, exclude deleted entry from the result.
    if (!start_changestamp && entry->deleted())
      should_exclude = true;

    // The entry matched the criteria for inclusion.
    if (!should_exclude)
      ++num_entries_matched;

    // If |start_offset| is set, exclude the entry if the entry is before the
    // start index. <= instead of < as |num_entries_matched| was
    // already incremented.
    if (start_offset > 0 && num_entries_matched <= start_offset)
      should_exclude = true;

    if (!should_exclude)
      entries.push_back(entry.release());
  }

  scoped_ptr<ResourceList> resource_list(new ResourceList);
  if (start_changestamp > 0 && start_offset == 0) {
    resource_list->set_largest_changestamp(
        about_resource_->largest_change_id());
  }

  // If |max_results| is set, trim the entries if the number exceeded the max
  // results.
  if (max_results > 0 && entries.size() > static_cast<size_t>(max_results)) {
    entries.erase(entries.begin() + max_results, entries.end());
    // Adds the next URL.
    // Here, we embed information which is needed for continuing the
    // GetResourceList request in the next invocation into url query
    // parameters.
    GURL next_url(base::StringPrintf(
        "http://localhost/?start-offset=%d&max-results=%d",
        start_offset + max_results,
        max_results));
    if (start_changestamp > 0) {
      next_url = net::AppendOrReplaceQueryParameter(
          next_url, "changestamp",
          base::Int64ToString(start_changestamp).c_str());
    }
    if (!search_query.empty()) {
      next_url = net::AppendOrReplaceQueryParameter(
          next_url, "q", search_query);
    }
    if (!directory_resource_id.empty()) {
      next_url = net::AppendOrReplaceQueryParameter(
          next_url, "parent", directory_resource_id);
    }

    Link* link = new Link;
    link->set_type(Link::LINK_NEXT);
    link->set_href(next_url);
    resource_list->mutable_links()->push_back(link);
  }
  resource_list->set_entries(entries.Pass());

  if (load_counter)
    *load_counter += 1;
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, HTTP_SUCCESS, base::Passed(&resource_list)));
}

GURL FakeDriveService::GetNewUploadSessionUrl() {
  return GURL("https://upload_session_url/" +
              base::Int64ToString(next_upload_sequence_number_++));
}

}  // namespace drive
