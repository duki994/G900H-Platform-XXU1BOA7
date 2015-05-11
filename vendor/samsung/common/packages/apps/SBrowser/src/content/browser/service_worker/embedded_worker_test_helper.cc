// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_test_helper.h"

#include "base/bind.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

EmbeddedWorkerTestHelper::EmbeddedWorkerTestHelper(
    ServiceWorkerContextCore* context,
    int mock_render_process_id)
    : context_(context->AsWeakPtr()),
      next_thread_id_(0),
      weak_factory_(this) {
  registry()->AddChildProcessSender(mock_render_process_id, this);
}

EmbeddedWorkerTestHelper::~EmbeddedWorkerTestHelper() {
}

void EmbeddedWorkerTestHelper::SimulateAddProcessToWorker(
    int embedded_worker_id,
    int process_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker);
  registry()->AddChildProcessSender(process_id, this);
  worker->AddProcessReference(process_id);
}

bool EmbeddedWorkerTestHelper::Send(IPC::Message* message) {
  OnMessageReceived(*message);
  delete message;
  return true;
}

bool EmbeddedWorkerTestHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerTestHelper, message)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerMsg_StartWorker, OnStartWorkerStub)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerMsg_StopWorker, OnStopWorkerStub)
    IPC_MESSAGE_HANDLER(EmbeddedWorkerContextMsg_SendMessageToWorker,
                        OnSendMessageToWorkerStub)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // IPC::TestSink only records messages that are not handled by filters,
  // so we just forward all messages to the separate sink.
  sink_.OnMessageReceived(message);

  return handled;
}

void EmbeddedWorkerTestHelper::OnStartWorker(
    int embedded_worker_id,
    int64 service_worker_version_id,
    const GURL& script_url) {
  // By default just notify the sender that the worker is started.
  SimulateWorkerStarted(next_thread_id_++, embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnStopWorker(int embedded_worker_id) {
  // By default just notify the sender that the worker is stopped.
  SimulateWorkerStopped(embedded_worker_id);
}

void EmbeddedWorkerTestHelper::OnSendMessageToWorker(
    int thread_id,
    int embedded_worker_id,
    int request_id,
    const IPC::Message& message) {
  // Do nothing on the message by default.
}

void EmbeddedWorkerTestHelper::SimulateWorkerStarted(
    int thread_id, int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerStarted(
      worker->process_id(),
      thread_id,
      embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateWorkerStopped(
    int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  registry()->OnWorkerStopped(worker->process_id(), embedded_worker_id);
}

void EmbeddedWorkerTestHelper::SimulateSendMessageToBrowser(
    int embedded_worker_id, int request_id, const IPC::Message& message) {
  registry()->OnSendMessageToBrowser(embedded_worker_id, request_id, message);
}

void EmbeddedWorkerTestHelper::OnStartWorkerStub(
    int embedded_worker_id,
    int64 service_worker_version_id,
    const GURL& script_url) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  EXPECT_EQ(EmbeddedWorkerInstance::STARTING, worker->status());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnStartWorker,
                 weak_factory_.GetWeakPtr(),
                 embedded_worker_id,
                 service_worker_version_id,
                 script_url));
}

void EmbeddedWorkerTestHelper::OnStopWorkerStub(int embedded_worker_id) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnStopWorker,
                 weak_factory_.GetWeakPtr(),
                 embedded_worker_id));
}

void EmbeddedWorkerTestHelper::OnSendMessageToWorkerStub(
    int thread_id,
    int embedded_worker_id,
    int request_id,
    const IPC::Message& message) {
  EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
  ASSERT_TRUE(worker != NULL);
  EXPECT_EQ(worker->thread_id(), thread_id);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&EmbeddedWorkerTestHelper::OnSendMessageToWorker,
                 weak_factory_.GetWeakPtr(),
                 thread_id,
                 embedded_worker_id,
                 request_id,
                 message));
}

EmbeddedWorkerRegistry* EmbeddedWorkerTestHelper::registry() {
  DCHECK(context_);
  return context_->embedded_worker_registry();
}

}  // namespace content
