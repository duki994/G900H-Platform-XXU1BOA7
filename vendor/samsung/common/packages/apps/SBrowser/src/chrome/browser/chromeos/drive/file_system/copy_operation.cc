// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system/copy_operation.h"

#include <string>

#include "base/file_util.h"
#include "base/task_runner_util.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system/create_file_operation.h"
#include "chrome/browser/chromeos/drive/file_system/operation_observer.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/resource_entry_conversion.h"
#include "chrome/browser/chromeos/drive/resource_metadata.h"
#include "chrome/browser/drive/drive_api_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/drive/drive_api_parser.h"

using content::BrowserThread;

namespace drive {
namespace file_system {

struct CopyOperation::CopyParams {
  base::FilePath src_file_path;
  base::FilePath dest_file_path;
  bool preserve_last_modified;
  FileOperationCallback callback;
  ResourceEntry src_entry;
  ResourceEntry parent_entry;
};

namespace {

FileError TryToCopyLocally(internal::ResourceMetadata* metadata,
                           internal::FileCache* cache,
                           CopyOperation::CopyParams* params,
                           std::vector<std::string>* updated_local_ids,
                           bool* directory_changed,
                           bool* should_copy_on_server) {
  FileError error = metadata->GetResourceEntryByPath(params->src_file_path,
                                                     &params->src_entry);
  if (error != FILE_ERROR_OK)
    return error;

  error = metadata->GetResourceEntryByPath(params->dest_file_path.DirName(),
                                           &params->parent_entry);
  if (error != FILE_ERROR_OK)
    return error;

  if (!params->parent_entry.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  // Drive File System doesn't support recursive copy.
  if (params->src_entry.file_info().is_directory())
    return FILE_ERROR_NOT_A_FILE;

  // Check destination.
  ResourceEntry dest_entry;
  error = metadata->GetResourceEntryByPath(params->dest_file_path, &dest_entry);
  switch (error) {
    case FILE_ERROR_OK:
      // File API spec says it is an error to try to "copy a file to a path
      // occupied by a directory".
      if (dest_entry.file_info().is_directory())
        return FILE_ERROR_INVALID_OPERATION;

      // Move the existing entry to the trash.
      dest_entry.set_parent_local_id(util::kDriveTrashDirLocalId);
      error = metadata->RefreshEntry(dest_entry);
      if (error != FILE_ERROR_OK)
        return error;
      updated_local_ids->push_back(dest_entry.local_id());
      *directory_changed = true;
      break;
    case FILE_ERROR_NOT_FOUND:
      break;
    default:
      return error;
  }

  // If the cache file is not present and the entry exists on the server,
  // server side copy should be used.
  FileCacheEntry cache_entry;
  cache->GetCacheEntry(params->src_entry.local_id(), &cache_entry);
  if (!cache_entry.is_present() && !params->src_entry.resource_id().empty()) {
    *should_copy_on_server = true;
    return FILE_ERROR_OK;
  }

  // Copy locally.
  ResourceEntry entry;
  const int64 now = base::Time::Now().ToInternalValue();
  entry.set_title(params->dest_file_path.BaseName().AsUTF8Unsafe());
  entry.set_parent_local_id(params->parent_entry.local_id());
  entry.mutable_file_specific_info()->set_content_mime_type(
      params->src_entry.file_specific_info().content_mime_type());
  entry.set_metadata_edit_state(ResourceEntry::DIRTY);
  entry.mutable_file_info()->set_last_modified(
      params->preserve_last_modified ?
      params->src_entry.file_info().last_modified() : now);
  entry.mutable_file_info()->set_last_accessed(now);

  std::string local_id;
  error = metadata->AddEntry(entry, &local_id);
  if (error != FILE_ERROR_OK)
    return error;
  updated_local_ids->push_back(local_id);
  *directory_changed = true;

  if (!cache_entry.is_present()) {
    DCHECK(params->src_entry.resource_id().empty());
    // Locally created empty file may have no cache file.
    return FILE_ERROR_OK;
  }

  base::FilePath cache_file_path;
  error = cache->GetFile(params->src_entry.local_id(), &cache_file_path);
  if (error != FILE_ERROR_OK)
    return error;

  return cache->Store(local_id, std::string(), cache_file_path,
                      internal::FileCache::FILE_OPERATION_COPY);
}

// Stores the copied entry and returns its path.
FileError UpdateLocalStateForServerSideCopy(
    internal::ResourceMetadata* metadata,
    scoped_ptr<google_apis::ResourceEntry> resource_entry,
    base::FilePath* file_path) {
  DCHECK(resource_entry);

  ResourceEntry entry;
  std::string parent_resource_id;
  if (!ConvertToResourceEntry(*resource_entry, &entry, &parent_resource_id) ||
      parent_resource_id.empty())
    return FILE_ERROR_NOT_A_FILE;

  std::string parent_local_id;
  FileError error = metadata->GetIdByResourceId(parent_resource_id,
                                                &parent_local_id);
  if (error != FILE_ERROR_OK)
    return error;
  entry.set_parent_local_id(parent_local_id);

  std::string local_id;
  error = metadata->AddEntry(entry, &local_id);
  // Depending on timing, the metadata may have inserted via change list
  // already. So, FILE_ERROR_EXISTS is not an error.
  if (error == FILE_ERROR_EXISTS)
    error = metadata->GetIdByResourceId(entry.resource_id(), &local_id);

  if (error == FILE_ERROR_OK)
    *file_path = metadata->GetFilePath(local_id);

  return error;
}

// Stores the file at |local_file_path| to the cache as a content of entry at
// |remote_dest_path|, and marks it dirty.
FileError UpdateLocalStateForScheduleTransfer(
    internal::ResourceMetadata* metadata,
    internal::FileCache* cache,
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    std::string* local_id) {
  FileError error = metadata->GetIdByPath(remote_dest_path, local_id);
  if (error != FILE_ERROR_OK)
    return error;

  ResourceEntry entry;
  error = metadata->GetResourceEntryById(*local_id, &entry);
  if (error != FILE_ERROR_OK)
    return error;

  return cache->Store(*local_id, std::string(), local_src_path,
                      internal::FileCache::FILE_OPERATION_COPY);
}

// Gets the file size of the |local_path|, and the ResourceEntry for the parent
// of |remote_path| to prepare the necessary information for transfer.
FileError PrepareTransferFileFromLocalToRemote(
    internal::ResourceMetadata* metadata,
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    std::string* gdoc_resource_id,
    std::string* parent_resource_id) {
  ResourceEntry parent_entry;
  FileError error = metadata->GetResourceEntryByPath(
      remote_dest_path.DirName(), &parent_entry);
  if (error != FILE_ERROR_OK)
    return error;

  // The destination's parent must be a directory.
  if (!parent_entry.file_info().is_directory())
    return FILE_ERROR_NOT_A_DIRECTORY;

  // Try to parse GDoc File and extract the resource id, if necessary.
  // Failing isn't problem. It'd be handled as a regular file, then.
  if (util::HasGDocFileExtension(local_src_path)) {
    *gdoc_resource_id = util::ReadResourceIdFromGDocFile(local_src_path);
    *parent_resource_id = parent_entry.resource_id();
  }

  return FILE_ERROR_OK;
}

}  // namespace

CopyOperation::CopyOperation(base::SequencedTaskRunner* blocking_task_runner,
                             OperationObserver* observer,
                             JobScheduler* scheduler,
                             internal::ResourceMetadata* metadata,
                             internal::FileCache* cache,
                             const ResourceIdCanonicalizer& id_canonicalizer)
  : blocking_task_runner_(blocking_task_runner),
    observer_(observer),
    scheduler_(scheduler),
    metadata_(metadata),
    cache_(cache),
    id_canonicalizer_(id_canonicalizer),
    create_file_operation_(new CreateFileOperation(blocking_task_runner,
                                                   observer,
                                                   metadata)),
    weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

CopyOperation::~CopyOperation() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void CopyOperation::Copy(const base::FilePath& src_file_path,
                         const base::FilePath& dest_file_path,
                         bool preserve_last_modified,
                         const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  CopyParams* params = new CopyParams;
  params->src_file_path = src_file_path;
  params->dest_file_path = dest_file_path;
  params->preserve_last_modified = preserve_last_modified;
  params->callback = callback;

  std::vector<std::string>* updated_local_ids = new std::vector<std::string>;
  bool* directory_changed = new bool(false);
  bool* should_copy_on_server = new bool(false);
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&TryToCopyLocally, metadata_, cache_, params,
                 updated_local_ids, directory_changed, should_copy_on_server),
      base::Bind(&CopyOperation::CopyAfterTryToCopyLocally,
                 weak_ptr_factory_.GetWeakPtr(), base::Owned(params),
                 base::Owned(updated_local_ids), base::Owned(directory_changed),
                 base::Owned(should_copy_on_server)));
}

void CopyOperation::CopyAfterTryToCopyLocally(
    const CopyParams* params,
    const std::vector<std::string>* updated_local_ids,
    const bool* directory_changed,
    const bool* should_copy_on_server,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params->callback.is_null());

  for (size_t i = 0; i < updated_local_ids->size(); ++i)
    observer_->OnEntryUpdatedByOperation((*updated_local_ids)[i]);

  if (*directory_changed)
    observer_->OnDirectoryChangedByOperation(params->dest_file_path.DirName());

  if (error != FILE_ERROR_OK || !*should_copy_on_server) {
    params->callback.Run(error);
    return;
  }

  base::FilePath new_title = params->dest_file_path.BaseName();
  if (params->src_entry.file_specific_info().is_hosted_document()) {
    // Drop the document extension, which should not be in the title.
    // TODO(yoshiki): Remove this code with crbug.com/223304.
    new_title = new_title.RemoveExtension();
  }

  base::Time last_modified =
      params->preserve_last_modified ?
      base::Time::FromInternalValue(
          params->src_entry.file_info().last_modified()) : base::Time();

  CopyResourceOnServer(
      params->src_entry.resource_id(), params->parent_entry.resource_id(),
      new_title.AsUTF8Unsafe(), last_modified, params->callback);
}

void CopyOperation::TransferFileFromLocalToRemote(
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  std::string* gdoc_resource_id = new std::string;
  std::string* parent_resource_id = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(
          &PrepareTransferFileFromLocalToRemote,
          metadata_, local_src_path, remote_dest_path,
          gdoc_resource_id, parent_resource_id),
      base::Bind(
          &CopyOperation::TransferFileFromLocalToRemoteAfterPrepare,
          weak_ptr_factory_.GetWeakPtr(),
          local_src_path, remote_dest_path, callback,
          base::Owned(gdoc_resource_id), base::Owned(parent_resource_id)));
}

void CopyOperation::TransferFileFromLocalToRemoteAfterPrepare(
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    const FileOperationCallback& callback,
    std::string* gdoc_resource_id,
    std::string* parent_resource_id,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // For regular files, schedule the transfer.
  if (gdoc_resource_id->empty()) {
    ScheduleTransferRegularFile(local_src_path, remote_dest_path, callback);
    return;
  }

  // This is uploading a JSON file representing a hosted document.
  // Copy the document on the Drive server.

  // GDoc file may contain a resource ID in the old format.
  const std::string canonicalized_resource_id =
      id_canonicalizer_.Run(*gdoc_resource_id);

  CopyResourceOnServer(
      canonicalized_resource_id, *parent_resource_id,
      // Drop the document extension, which should not be in the title.
      // TODO(yoshiki): Remove this code with crbug.com/223304.
      remote_dest_path.BaseName().RemoveExtension().AsUTF8Unsafe(),
      base::Time(),
      callback);
}

void CopyOperation::CopyResourceOnServer(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scheduler_->CopyResource(
      resource_id, parent_resource_id, new_title, last_modified,
      base::Bind(&CopyOperation::CopyResourceOnServerAfterServerSideCopy,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void CopyOperation::CopyResourceOnServerAfterServerSideCopy(
    const FileOperationCallback& callback,
    google_apis::GDataErrorCode status,
    scoped_ptr<google_apis::ResourceEntry> resource_entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileError error = GDataToFileError(status);
  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  // The copy on the server side is completed successfully. Update the local
  // metadata.
  base::FilePath* file_path = new base::FilePath;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(&UpdateLocalStateForServerSideCopy,
                 metadata_, base::Passed(&resource_entry), file_path),
      base::Bind(&CopyOperation::CopyResourceOnServerAfterUpdateLocalState,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback, base::Owned(file_path)));
}

void CopyOperation::CopyResourceOnServerAfterUpdateLocalState(
    const FileOperationCallback& callback,
    base::FilePath* file_path,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK)
    observer_->OnDirectoryChangedByOperation(file_path->DirName());
  callback.Run(error);
}

void CopyOperation::ScheduleTransferRegularFile(
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  create_file_operation_->CreateFile(
      remote_dest_path,
      false,  // Not exclusive (OK even if a file already exists).
      std::string(),  // no specific mime type; CreateFile should guess it.
      base::Bind(&CopyOperation::ScheduleTransferRegularFileAfterCreate,
                 weak_ptr_factory_.GetWeakPtr(),
                 local_src_path, remote_dest_path, callback));
}

void CopyOperation::ScheduleTransferRegularFileAfterCreate(
    const base::FilePath& local_src_path,
    const base::FilePath& remote_dest_path,
    const FileOperationCallback& callback,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error != FILE_ERROR_OK) {
    callback.Run(error);
    return;
  }

  std::string* local_id = new std::string;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(),
      FROM_HERE,
      base::Bind(
          &UpdateLocalStateForScheduleTransfer,
          metadata_, cache_, local_src_path, remote_dest_path, local_id),
      base::Bind(
          &CopyOperation::ScheduleTransferRegularFileAfterUpdateLocalState,
          weak_ptr_factory_.GetWeakPtr(), callback, remote_dest_path,
          base::Owned(local_id)));
}

void CopyOperation::ScheduleTransferRegularFileAfterUpdateLocalState(
    const FileOperationCallback& callback,
    const base::FilePath& remote_dest_path,
    std::string* local_id,
    FileError error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (error == FILE_ERROR_OK) {
    observer_->OnDirectoryChangedByOperation(remote_dest_path.DirName());
    observer_->OnEntryUpdatedByOperation(*local_id);
  }
  callback.Run(error);
}

}  // namespace file_system
}  // namespace drive
