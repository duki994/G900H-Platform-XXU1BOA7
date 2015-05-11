// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/fileapi/native_media_file_util.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_platform_file_closer.h"
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "chrome/browser/media_galleries/fileapi/media_path_filter.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_sniffer.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/common/blob/shareable_file_reference.h"

namespace {

// Returns true if the current thread is capable of doing IO.
bool IsOnTaskRunnerThread(fileapi::FileSystemOperationContext* context) {
  return context->task_runner()->RunsTasksOnCurrentThread();
}

base::File::Error IsMediaHeader(const char* buf, size_t length) {
  if (length == 0)
    return base::File::FILE_ERROR_SECURITY;

  std::string mime_type;
  if (!net::SniffMimeTypeFromLocalData(buf, length, &mime_type))
    return base::File::FILE_ERROR_SECURITY;

  if (StartsWithASCII(mime_type, "image/", true) ||
      StartsWithASCII(mime_type, "audio/", true) ||
      StartsWithASCII(mime_type, "video/", true) ||
      mime_type == "application/x-shockwave-flash") {
    return base::File::FILE_OK;
  }
  return base::File::FILE_ERROR_SECURITY;
}

}  // namespace

NativeMediaFileUtil::NativeMediaFileUtil(MediaPathFilter* media_path_filter)
    : media_path_filter_(media_path_filter),
      weak_factory_(this) {
}

NativeMediaFileUtil::~NativeMediaFileUtil() {
}

// static
base::File::Error NativeMediaFileUtil::IsMediaFile(
    const base::FilePath& path) {
  base::PlatformFile file_handle;
  const int flags = base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
  base::File::Error error =
      fileapi::NativeFileUtil::CreateOrOpen(path, flags, &file_handle, NULL);
  if (error != base::File::FILE_OK)
    return error;

  base::ScopedPlatformFileCloser scoped_platform_file(&file_handle);
  char buffer[net::kMaxBytesToSniff];

  // Read as much as net::SniffMimeTypeFromLocalData() will bother looking at.
  int64 len =
      base::ReadPlatformFile(file_handle, 0, buffer, net::kMaxBytesToSniff);
  if (len < 0)
    return base::File::FILE_ERROR_FAILED;

  return IsMediaHeader(buffer, len);
}

// static
base::File::Error NativeMediaFileUtil::BufferIsMediaHeader(
    net::IOBuffer* buf, size_t length) {
  return IsMediaHeader(buf->data(), length);
}

void NativeMediaFileUtil::CreateOrOpen(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  // Only called by NaCl, which should not have access to media file systems.
  base::PlatformFile invalid_file(base::kInvalidPlatformFileValue);
  callback.Run(base::File::FILE_ERROR_SECURITY,
               base::PassPlatformFile(&invalid_file),
               base::Closure());
}

void NativeMediaFileUtil::EnsureFileExists(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY, false);
}

void NativeMediaFileUtil::CreateDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  fileapi::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&NativeMediaFileUtil::CreateDirectoryOnTaskRunnerThread,
                 weak_factory_.GetWeakPtr(), base::Passed(&context),
                 url, exclusive, recursive, callback));
  DCHECK(success);
}

void NativeMediaFileUtil::GetFileInfo(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  fileapi::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&NativeMediaFileUtil::GetFileInfoOnTaskRunnerThread,
                 weak_factory_.GetWeakPtr(), base::Passed(&context),
                 url, callback));
  DCHECK(success);
}

void NativeMediaFileUtil::ReadDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  fileapi::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&NativeMediaFileUtil::ReadDirectoryOnTaskRunnerThread,
                 weak_factory_.GetWeakPtr(), base::Passed(&context),
                 url, callback));
  DCHECK(success);
}

void NativeMediaFileUtil::Touch(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void NativeMediaFileUtil::Truncate(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_SECURITY);
}

void NativeMediaFileUtil::CopyFileLocal(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  fileapi::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&NativeMediaFileUtil::CopyOrMoveFileLocalOnTaskRunnerThread,
                 weak_factory_.GetWeakPtr(), base::Passed(&context),
                 src_url, dest_url, option, true /* copy */, callback));
  DCHECK(success);
}

void NativeMediaFileUtil::MoveFileLocal(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  fileapi::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&NativeMediaFileUtil::CopyOrMoveFileLocalOnTaskRunnerThread,
                 weak_factory_.GetWeakPtr(), base::Passed(&context),
                 src_url, dest_url, option, false /* copy */, callback));
  DCHECK(success);
}

void NativeMediaFileUtil::CopyInForeignFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const fileapi::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  fileapi::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&NativeMediaFileUtil::CopyInForeignFileOnTaskRunnerThread,
                 weak_factory_.GetWeakPtr(), base::Passed(&context),
                 src_file_path, dest_url, callback));
  DCHECK(success);
}

void NativeMediaFileUtil::DeleteFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  fileapi::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&NativeMediaFileUtil::DeleteFileOnTaskRunnerThread,
                 weak_factory_.GetWeakPtr(), base::Passed(&context),
                 url, callback));
  DCHECK(success);
}

// This is needed to support Copy and Move.
void NativeMediaFileUtil::DeleteDirectory(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  fileapi::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&NativeMediaFileUtil::DeleteDirectoryOnTaskRunnerThread,
                 weak_factory_.GetWeakPtr(), base::Passed(&context),
                 url, callback));
  DCHECK(success);
}

void NativeMediaFileUtil::DeleteRecursively(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
}

void NativeMediaFileUtil::CreateSnapshotFile(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  fileapi::FileSystemOperationContext* context_ptr = context.get();
  const bool success = context_ptr->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&NativeMediaFileUtil::CreateSnapshotFileOnTaskRunnerThread,
                 weak_factory_.GetWeakPtr(), base::Passed(&context),
                 url, callback));
  DCHECK(success);
}

void NativeMediaFileUtil::CreateDirectoryOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::File::Error error =
      CreateDirectorySync(context.get(), url, exclusive, recursive);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, error));
}

void NativeMediaFileUtil::GetFileInfoOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::File::Info file_info;
  base::File::Error error =
      GetFileInfoSync(context.get(), url, &file_info, NULL);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, error, file_info));
}

void NativeMediaFileUtil::ReadDirectoryOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK(IsOnTaskRunnerThread(context.get()));
  EntryList entry_list;
  base::File::Error error =
      ReadDirectorySync(context.get(), url, &entry_list);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, error, entry_list, false /* has_more */));
}

void NativeMediaFileUtil::CopyOrMoveFileLocalOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    bool copy,
    const StatusCallback& callback) {
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::File::Error error =
      CopyOrMoveFileSync(context.get(), src_url, dest_url, option, copy);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, error));
}

void NativeMediaFileUtil::CopyInForeignFileOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const base::FilePath& src_file_path,
    const fileapi::FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::File::Error error =
      CopyInForeignFileSync(context.get(), src_file_path, dest_url);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, error));
}

void NativeMediaFileUtil::DeleteFileOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::File::Error error = DeleteFileSync(context.get(), url);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, error));
}

void NativeMediaFileUtil::DeleteDirectoryOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::File::Error error = DeleteDirectorySync(context.get(), url);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, error));
}

void NativeMediaFileUtil::CreateSnapshotFileOnTaskRunnerThread(
    scoped_ptr<fileapi::FileSystemOperationContext> context,
    const fileapi::FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK(IsOnTaskRunnerThread(context.get()));
  base::File::Info file_info;
  base::FilePath platform_path;
  scoped_refptr<webkit_blob::ShareableFileReference> file_ref;
  base::File::Error error =
      CreateSnapshotFileSync(context.get(), url, &file_info, &platform_path,
                             &file_ref);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, error, file_info, platform_path, file_ref));
}

base::File::Error NativeMediaFileUtil::CreateDirectorySync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    bool exclusive,
    bool recursive) {
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  return fileapi::NativeFileUtil::CreateDirectory(file_path, exclusive,
                                                  recursive);
}

base::File::Error NativeMediaFileUtil::CopyOrMoveFileSync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& src_url,
    const fileapi::FileSystemURL& dest_url,
    CopyOrMoveOption option,
    bool copy) {
  DCHECK(IsOnTaskRunnerThread(context));
  base::FilePath src_file_path;
  base::File::Error error =
      GetFilteredLocalFilePathForExistingFileOrDirectory(
          context, src_url,
          base::File::FILE_ERROR_NOT_FOUND,
          &src_file_path);
  if (error != base::File::FILE_OK)
    return error;
  if (fileapi::NativeFileUtil::DirectoryExists(src_file_path))
    return base::File::FILE_ERROR_NOT_A_FILE;

  base::FilePath dest_file_path;
  error = GetLocalFilePath(context, dest_url, &dest_file_path);
  if (error != base::File::FILE_OK)
    return error;
  base::File::Info file_info;
  error = fileapi::NativeFileUtil::GetFileInfo(dest_file_path, &file_info);
  if (error != base::File::FILE_OK &&
      error != base::File::FILE_ERROR_NOT_FOUND) {
    return error;
  }
  if (error == base::File::FILE_OK && file_info.is_directory)
    return base::File::FILE_ERROR_INVALID_OPERATION;
  if (!media_path_filter_->Match(dest_file_path))
    return base::File::FILE_ERROR_SECURITY;

  return fileapi::NativeFileUtil::CopyOrMoveFile(
      src_file_path, dest_file_path, option,
      fileapi::NativeFileUtil::CopyOrMoveModeForDestination(dest_url, copy));
}

base::File::Error NativeMediaFileUtil::CopyInForeignFileSync(
    fileapi::FileSystemOperationContext* context,
    const base::FilePath& src_file_path,
    const fileapi::FileSystemURL& dest_url) {
  DCHECK(IsOnTaskRunnerThread(context));
  if (src_file_path.empty())
    return base::File::FILE_ERROR_INVALID_OPERATION;

  base::FilePath dest_file_path;
  base::File::Error error =
      GetFilteredLocalFilePath(context, dest_url, &dest_file_path);
  if (error != base::File::FILE_OK)
    return error;
  return fileapi::NativeFileUtil::CopyOrMoveFile(
      src_file_path, dest_file_path,
      fileapi::FileSystemOperation::OPTION_NONE,
      fileapi::NativeFileUtil::CopyOrMoveModeForDestination(dest_url,
                                                            true /* copy */));
}

base::File::Error NativeMediaFileUtil::GetFileInfoSync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    base::File::Info* file_info,
    base::FilePath* platform_path) {
  DCHECK(context);
  DCHECK(IsOnTaskRunnerThread(context));
  DCHECK(file_info);

  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  if (base::IsLink(file_path))
    return base::File::FILE_ERROR_NOT_FOUND;
  error = fileapi::NativeFileUtil::GetFileInfo(file_path, file_info);
  if (error != base::File::FILE_OK)
    return error;

  if (platform_path)
    *platform_path = file_path;
  if (file_info->is_directory ||
      media_path_filter_->Match(file_path)) {
    return base::File::FILE_OK;
  }
  return base::File::FILE_ERROR_NOT_FOUND;
}

base::File::Error NativeMediaFileUtil::GetLocalFilePath(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    base::FilePath* local_file_path) {
  DCHECK(local_file_path);
  DCHECK(url.is_valid());
  if (url.path().empty()) {
    // Root direcory case, which should not be accessed.
    return base::File::FILE_ERROR_ACCESS_DENIED;
  }
  *local_file_path = url.path();
  return base::File::FILE_OK;
}

base::File::Error NativeMediaFileUtil::ReadDirectorySync(
      fileapi::FileSystemOperationContext* context,
      const fileapi::FileSystemURL& url,
      EntryList* file_list) {
  DCHECK(IsOnTaskRunnerThread(context));
  DCHECK(file_list);
  DCHECK(file_list->empty());
  base::File::Info file_info;
  base::FilePath dir_path;
  base::File::Error error =
      GetFileInfoSync(context, url, &file_info, &dir_path);

  if (error != base::File::FILE_OK)
    return error;

  if (!file_info.is_directory)
    return base::File::FILE_ERROR_NOT_A_DIRECTORY;

  base::FileEnumerator file_enum(
      dir_path,
      false /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  for (base::FilePath enum_path = file_enum.Next();
       !enum_path.empty();
       enum_path = file_enum.Next()) {
    // Skip symlinks.
    if (base::IsLink(enum_path))
      continue;

    base::FileEnumerator::FileInfo info = file_enum.GetInfo();

    // NativeMediaFileUtil skip criteria.
    if (MediaPathFilter::ShouldSkip(enum_path))
      continue;
    if (!info.IsDirectory() && !media_path_filter_->Match(enum_path))
      continue;

    fileapi::DirectoryEntry entry;
    entry.is_directory = info.IsDirectory();
    entry.name = enum_path.BaseName().value();
    entry.size = info.GetSize();
    entry.last_modified_time = info.GetLastModifiedTime();

    file_list->push_back(entry);
  }

  return base::File::FILE_OK;
}

base::File::Error NativeMediaFileUtil::DeleteFileSync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url) {
  DCHECK(IsOnTaskRunnerThread(context));
  base::File::Info file_info;
  base::FilePath file_path;
  base::File::Error error =
      GetFileInfoSync(context, url, &file_info, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  if (file_info.is_directory)
    return base::File::FILE_ERROR_NOT_A_FILE;
  return fileapi::NativeFileUtil::DeleteFile(file_path);
}

base::File::Error NativeMediaFileUtil::DeleteDirectorySync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url) {
  DCHECK(IsOnTaskRunnerThread(context));
  base::FilePath file_path;
  base::File::Error error = GetLocalFilePath(context, url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  return fileapi::NativeFileUtil::DeleteDirectory(file_path);
}

base::File::Error NativeMediaFileUtil::CreateSnapshotFileSync(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& url,
    base::File::Info* file_info,
    base::FilePath* platform_path,
    scoped_refptr<webkit_blob::ShareableFileReference>* file_ref) {
  DCHECK(IsOnTaskRunnerThread(context));
  base::File::Error error =
      GetFileInfoSync(context, url, file_info, platform_path);
  if (error == base::File::FILE_OK && file_info->is_directory)
    error = base::File::FILE_ERROR_NOT_A_FILE;
  if (error == base::File::FILE_OK)
    error = NativeMediaFileUtil::IsMediaFile(*platform_path);

  // We're just returning the local file information.
  *file_ref = scoped_refptr<webkit_blob::ShareableFileReference>();

  return error;
}

base::File::Error NativeMediaFileUtil::GetFilteredLocalFilePath(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& file_system_url,
    base::FilePath* local_file_path) {
  DCHECK(IsOnTaskRunnerThread(context));
  base::FilePath file_path;
  base::File::Error error =
      GetLocalFilePath(context, file_system_url, &file_path);
  if (error != base::File::FILE_OK)
    return error;
  if (!media_path_filter_->Match(file_path))
    return base::File::FILE_ERROR_SECURITY;

  *local_file_path = file_path;
  return base::File::FILE_OK;
}

base::File::Error
NativeMediaFileUtil::GetFilteredLocalFilePathForExistingFileOrDirectory(
    fileapi::FileSystemOperationContext* context,
    const fileapi::FileSystemURL& file_system_url,
    base::File::Error failure_error,
    base::FilePath* local_file_path) {
  DCHECK(IsOnTaskRunnerThread(context));
  base::FilePath file_path;
  base::File::Error error =
      GetLocalFilePath(context, file_system_url, &file_path);
  if (error != base::File::FILE_OK)
    return error;

  if (!base::PathExists(file_path))
    return failure_error;
  base::File::Info file_info;
  if (!base::GetFileInfo(file_path, &file_info))
    return base::File::FILE_ERROR_FAILED;

  if (!file_info.is_directory &&
      !media_path_filter_->Match(file_path)) {
    return failure_error;
  }

  *local_file_path = file_path;
  return base::File::FILE_OK;
}
