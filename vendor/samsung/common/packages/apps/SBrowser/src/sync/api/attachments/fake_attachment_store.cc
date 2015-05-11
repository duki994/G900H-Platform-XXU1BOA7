// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/fake_attachment_store.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "sync/api/attachments/attachment.h"

namespace syncer {

// Backend is where all the work happens.
class FakeAttachmentStore::Backend
    : public base::RefCountedThreadSafe<FakeAttachmentStore::Backend> {
 public:
  // Construct a Backend that posts its results to |frontend_task_runner|.
  Backend(
      const scoped_refptr<base::SingleThreadTaskRunner>& frontend_task_runner);

  void Read(const sync_pb::AttachmentId& id, const ReadCallback& callback);
  void Write(const sync_pb::AttachmentId& id,
             const scoped_refptr<base::RefCountedMemory>& bytes,
             const WriteCallback& callback);
  void Drop(const sync_pb::AttachmentId& id, const DropCallback& callback);

 private:
  friend class base::RefCountedThreadSafe<Backend>;
  typedef std::string UniqueId;
  typedef std::map<UniqueId, Attachment*> AttachmentMap;

  ~Backend();
  Result RemoveAttachment(const sync_pb::AttachmentId& id);

  scoped_refptr<base::SingleThreadTaskRunner> frontend_task_runner_;
  AttachmentMap attachments_;
  STLValueDeleter<AttachmentMap> attachments_value_deleter_;
};

FakeAttachmentStore::Backend::Backend(
    const scoped_refptr<base::SingleThreadTaskRunner>& frontend_task_runner)
    : frontend_task_runner_(frontend_task_runner),
      attachments_value_deleter_(&attachments_) {}

FakeAttachmentStore::Backend::~Backend() {}

void FakeAttachmentStore::Backend::Read(const sync_pb::AttachmentId& id,
                                        const ReadCallback& callback) {
  AttachmentMap::iterator iter = attachments_.find(id.unique_id());
  scoped_ptr<Attachment> attachment;
  Result result = NOT_FOUND;
  if (iter != attachments_.end()) {
    attachment.reset(new Attachment(*iter->second));
    result = SUCCESS;
  }
  frontend_task_runner_->PostTask(
      FROM_HERE, base::Bind(callback, result, base::Passed(&attachment)));
}

void FakeAttachmentStore::Backend::Write(
    const sync_pb::AttachmentId& id,
    const scoped_refptr<base::RefCountedMemory>& bytes,
    const WriteCallback& callback) {
  scoped_ptr<Attachment> attachment = Attachment::CreateWithId(id, bytes);
  RemoveAttachment(id);
  attachments_.insert(
      AttachmentMap::value_type(id.unique_id(), attachment.release()));
  frontend_task_runner_->PostTask(FROM_HERE, base::Bind(callback, SUCCESS));
}

void FakeAttachmentStore::Backend::Drop(const sync_pb::AttachmentId& id,
                                        const DropCallback& callback) {
  Result result = RemoveAttachment(id);
  frontend_task_runner_->PostTask(FROM_HERE, base::Bind(callback, result));
}

AttachmentStore::Result FakeAttachmentStore::Backend::RemoveAttachment(
    const sync_pb::AttachmentId& id) {
  Result result = NOT_FOUND;
  AttachmentMap::iterator iter = attachments_.find(id.unique_id());
  if (iter != attachments_.end()) {
    delete iter->second;
    attachments_.erase(iter);
    result = SUCCESS;
  }
  return result;
}

FakeAttachmentStore::FakeAttachmentStore(
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner)
    : backend_(new Backend(base::MessageLoopProxy::current())),
      backend_task_runner_(backend_task_runner) {}

FakeAttachmentStore::~FakeAttachmentStore() {}

void FakeAttachmentStore::Read(const sync_pb::AttachmentId& id,
                               const ReadCallback& callback) {
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeAttachmentStore::Backend::Read, backend_, id, callback));
}

void FakeAttachmentStore::Write(
    const sync_pb::AttachmentId& id,
    const scoped_refptr<base::RefCountedMemory>& bytes,
    const WriteCallback& callback) {
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &FakeAttachmentStore::Backend::Write, backend_, id, bytes, callback));
}

void FakeAttachmentStore::Drop(const sync_pb::AttachmentId& id,
                               const DropCallback& callback) {
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&FakeAttachmentStore::Backend::Drop, backend_, id, callback));
}

}  // namespace syncer
