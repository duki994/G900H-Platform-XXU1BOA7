// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/non_blocking_invalidator.h"

#include <cstddef>

#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "jingle/notifier/listener/push_client.h"
#include "sync/notifier/gcm_network_channel_delegate.h"
#include "sync/notifier/invalidation_notifier.h"
#include "sync/notifier/object_id_invalidation_map.h"
#include "sync/notifier/sync_system_resources.h"

namespace syncer {

struct NonBlockingInvalidator::InitializeOptions {
  InitializeOptions(
      NetworkChannelCreator network_channel_creator,
      const std::string& invalidator_client_id,
      const UnackedInvalidationsMap& saved_invalidations,
      const std::string& invalidation_bootstrap_data,
      const WeakHandle<InvalidationStateTracker>&
          invalidation_state_tracker,
      const std::string& client_info,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter)
      : network_channel_creator(network_channel_creator),
        invalidator_client_id(invalidator_client_id),
        saved_invalidations(saved_invalidations),
        invalidation_bootstrap_data(invalidation_bootstrap_data),
        invalidation_state_tracker(invalidation_state_tracker),
        client_info(client_info),
        request_context_getter(request_context_getter) {
  }

  NetworkChannelCreator network_channel_creator;
  std::string invalidator_client_id;
  UnackedInvalidationsMap saved_invalidations;
  std::string invalidation_bootstrap_data;
  WeakHandle<InvalidationStateTracker> invalidation_state_tracker;
  std::string client_info;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter;
};


class NonBlockingInvalidator::Core
    : public base::RefCountedThreadSafe<NonBlockingInvalidator::Core>,
      // InvalidationHandler to observe the InvalidationNotifier we create.
      public InvalidationHandler {
 public:
  // Called on parent thread.  |delegate_observer| should be
  // initialized.
  explicit Core(
      const WeakHandle<InvalidationHandler>& delegate_observer);

  // Helpers called on I/O thread.
  void Initialize(
      const NonBlockingInvalidator::InitializeOptions& initialize_options);
  void Teardown();
  void UpdateRegisteredIds(const ObjectIdSet& ids);
  void UpdateCredentials(const std::string& email, const std::string& token);

  // InvalidationHandler implementation (all called on I/O thread by
  // InvalidationNotifier).
  virtual void OnInvalidatorStateChange(InvalidatorState reason) OVERRIDE;
  virtual void OnIncomingInvalidation(
      const ObjectIdInvalidationMap& invalidation_map) OVERRIDE;

 private:
  friend class
      base::RefCountedThreadSafe<NonBlockingInvalidator::Core>;
  // Called on parent or I/O thread.
  virtual ~Core();

  // The variables below should be used only on the I/O thread.
  const WeakHandle<InvalidationHandler> delegate_observer_;
  scoped_ptr<InvalidationNotifier> invalidation_notifier_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

NonBlockingInvalidator::Core::Core(
    const WeakHandle<InvalidationHandler>& delegate_observer)
    : delegate_observer_(delegate_observer) {
  DCHECK(delegate_observer_.IsInitialized());
}

NonBlockingInvalidator::Core::~Core() {
}

void NonBlockingInvalidator::Core::Initialize(
    const NonBlockingInvalidator::InitializeOptions& initialize_options) {
  DCHECK(initialize_options.request_context_getter.get());
  network_task_runner_ =
      initialize_options.request_context_getter->GetNetworkTaskRunner();
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  scoped_ptr<SyncNetworkChannel> network_channel =
      initialize_options.network_channel_creator.Run();
  invalidation_notifier_.reset(
      new InvalidationNotifier(
          network_channel.Pass(),
          initialize_options.invalidator_client_id,
          initialize_options.saved_invalidations,
          initialize_options.invalidation_bootstrap_data,
          initialize_options.invalidation_state_tracker,
          initialize_options.client_info));
  invalidation_notifier_->RegisterHandler(this);
}

void NonBlockingInvalidator::Core::Teardown() {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->UnregisterHandler(this);
  invalidation_notifier_.reset();
  network_task_runner_ = NULL;
}

void NonBlockingInvalidator::Core::UpdateRegisteredIds(const ObjectIdSet& ids) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->UpdateRegisteredIds(this, ids);
}

void NonBlockingInvalidator::Core::UpdateCredentials(const std::string& email,
                                                     const std::string& token) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  invalidation_notifier_->UpdateCredentials(email, token);
}

void NonBlockingInvalidator::Core::OnInvalidatorStateChange(
    InvalidatorState reason) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delegate_observer_.Call(
      FROM_HERE, &InvalidationHandler::OnInvalidatorStateChange, reason);
}

void NonBlockingInvalidator::Core::OnIncomingInvalidation(
    const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(network_task_runner_->BelongsToCurrentThread());
  delegate_observer_.Call(FROM_HERE,
                          &InvalidationHandler::OnIncomingInvalidation,
                          invalidation_map);
}

NonBlockingInvalidator::NonBlockingInvalidator(
    NetworkChannelCreator network_channel_creator,
    const std::string& invalidator_client_id,
    const UnackedInvalidationsMap& saved_invalidations,
    const std::string& invalidation_bootstrap_data,
    const WeakHandle<InvalidationStateTracker>&
        invalidation_state_tracker,
    const std::string& client_info,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter)
    : parent_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      network_task_runner_(request_context_getter->GetNetworkTaskRunner()),
      weak_ptr_factory_(this) {
  core_ = new Core(MakeWeakHandle(weak_ptr_factory_.GetWeakPtr()));

  InitializeOptions initialize_options(
      network_channel_creator,
      invalidator_client_id,
      saved_invalidations,
      invalidation_bootstrap_data,
      invalidation_state_tracker,
      client_info,
      request_context_getter);

  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &NonBlockingInvalidator::Core::Initialize,
              core_.get(),
              initialize_options))) {
    NOTREACHED();
  }
}

NonBlockingInvalidator::~NonBlockingInvalidator() {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidator::Core::Teardown,
                     core_.get()))) {
    DVLOG(1) << "Network thread stopped before invalidator is destroyed.";
  }
}

void NonBlockingInvalidator::RegisterHandler(InvalidationHandler* handler) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.RegisterHandler(handler);
}

void NonBlockingInvalidator::UpdateRegisteredIds(InvalidationHandler* handler,
                                                 const ObjectIdSet& ids) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.UpdateRegisteredIds(handler, ids);
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &NonBlockingInvalidator::Core::UpdateRegisteredIds,
              core_.get(),
              registrar_.GetAllRegisteredIds()))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidator::UnregisterHandler(InvalidationHandler* handler) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.UnregisterHandler(handler);
}

InvalidatorState NonBlockingInvalidator::GetInvalidatorState() const {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  return registrar_.GetInvalidatorState();
}

void NonBlockingInvalidator::UpdateCredentials(const std::string& email,
                                               const std::string& token) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  if (!network_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&NonBlockingInvalidator::Core::UpdateCredentials,
                     core_.get(), email, token))) {
    NOTREACHED();
  }
}

void NonBlockingInvalidator::OnInvalidatorStateChange(InvalidatorState state) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.UpdateInvalidatorState(state);
}

void NonBlockingInvalidator::OnIncomingInvalidation(
        const ObjectIdInvalidationMap& invalidation_map) {
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  registrar_.DispatchInvalidationsToHandlers(invalidation_map);
}

NetworkChannelCreator
    NonBlockingInvalidator::MakePushClientChannelCreator(
        const notifier::NotifierOptions& notifier_options) {
  return base::Bind(SyncNetworkChannel::CreatePushClientChannel,
      notifier_options);
}

NetworkChannelCreator NonBlockingInvalidator::MakeGCMNetworkChannelCreator(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_ptr<GCMNetworkChannelDelegate> delegate) {
  return base::Bind(&SyncNetworkChannel::CreateGCMNetworkChannel,
                    request_context_getter,
                    base::Passed(&delegate));
}

}  // namespace syncer
