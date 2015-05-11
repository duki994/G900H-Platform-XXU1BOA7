// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_message_filter.h"

#include "chrome/common/extensions/extension_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/process_manager.h"
#include "ipc/ipc_message_macros.h"

using content::BrowserThread;
using content::RenderProcessHost;

namespace extensions {

ExtensionMessageFilter::ExtensionMessageFilter(int render_process_id,
                                               content::BrowserContext* context)
    : render_process_id_(render_process_id), browser_context_(context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

ExtensionMessageFilter::~ExtensionMessageFilter() {}

void ExtensionMessageFilter::OverrideThreadForMessage(
    const IPC::Message& message,
    BrowserThread::ID* thread) {
  switch (message.type()) {
    case ExtensionHostMsg_AddListener::ID:
    case ExtensionHostMsg_RemoveListener::ID:
    case ExtensionHostMsg_AddLazyListener::ID:
    case ExtensionHostMsg_RemoveLazyListener::ID:
    case ExtensionHostMsg_AddFilteredListener::ID:
    case ExtensionHostMsg_RemoveFilteredListener::ID:
    case ExtensionHostMsg_ShouldSuspendAck::ID:
    case ExtensionHostMsg_SuspendAck::ID:
      *thread = BrowserThread::UI;
      break;
    default:
      break;
  }
}

bool ExtensionMessageFilter::OnMessageReceived(const IPC::Message& message,
                                               bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(ExtensionMessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddListener,
                        OnExtensionAddListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveListener,
                        OnExtensionRemoveListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddLazyListener,
                        OnExtensionAddLazyListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveLazyListener,
                        OnExtensionRemoveLazyListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_AddFilteredListener,
                        OnExtensionAddFilteredListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_RemoveFilteredListener,
                        OnExtensionRemoveFilteredListener)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ShouldSuspendAck,
                        OnExtensionShouldSuspendAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_SuspendAck,
                        OnExtensionSuspendAck)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_GenerateUniqueID,
                        OnExtensionGenerateUniqueID)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_ResumeRequests,
                        OnExtensionResumeRequests);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionMessageFilter::OnExtensionAddListener(
    const std::string& extension_id,
    const std::string& event_name) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;
  EventRouter* router = ExtensionSystem::Get(browser_context_)->event_router();
  if (!router)
    return;
  router->AddEventListener(event_name, process, extension_id);
}

void ExtensionMessageFilter::OnExtensionRemoveListener(
    const std::string& extension_id,
    const std::string& event_name) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;
  EventRouter* router = ExtensionSystem::Get(browser_context_)->event_router();
  if (!router)
    return;
  router->RemoveEventListener(event_name, process, extension_id);
}

void ExtensionMessageFilter::OnExtensionAddLazyListener(
    const std::string& extension_id, const std::string& event_name) {
  EventRouter* router = ExtensionSystem::Get(browser_context_)->event_router();
  if (!router)
    return;
  router->AddLazyEventListener(event_name, extension_id);
}

void ExtensionMessageFilter::OnExtensionRemoveLazyListener(
    const std::string& extension_id, const std::string& event_name) {
  EventRouter* router = ExtensionSystem::Get(browser_context_)->event_router();
  if (!router)
    return;
  router->RemoveLazyEventListener(event_name, extension_id);
}

void ExtensionMessageFilter::OnExtensionAddFilteredListener(
    const std::string& extension_id,
    const std::string& event_name,
    const base::DictionaryValue& filter,
    bool lazy) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;
  EventRouter* router = ExtensionSystem::Get(browser_context_)->event_router();
  if (!router)
    return;
  router->AddFilteredEventListener(
      event_name, process, extension_id, filter, lazy);
}

void ExtensionMessageFilter::OnExtensionRemoveFilteredListener(
    const std::string& extension_id,
    const std::string& event_name,
    const base::DictionaryValue& filter,
    bool lazy) {
  RenderProcessHost* process = RenderProcessHost::FromID(render_process_id_);
  if (!process)
    return;
  EventRouter* router = ExtensionSystem::Get(browser_context_)->event_router();
  if (!router)
    return;
  router->RemoveFilteredEventListener(
      event_name, process, extension_id, filter, lazy);
}

void ExtensionMessageFilter::OnExtensionShouldSuspendAck(
     const std::string& extension_id, int sequence_id) {
  ProcessManager* process_manager =
      ExtensionSystem::Get(browser_context_)->process_manager();
  if (process_manager)
    process_manager->OnShouldSuspendAck(extension_id, sequence_id);
}

void ExtensionMessageFilter::OnExtensionSuspendAck(
     const std::string& extension_id) {
  ProcessManager* process_manager =
      ExtensionSystem::Get(browser_context_)->process_manager();
  if (process_manager)
    process_manager->OnSuspendAck(extension_id);
}

void ExtensionMessageFilter::OnExtensionGenerateUniqueID(int* unique_id) {
  static int next_unique_id = 0;
  *unique_id = ++next_unique_id;
}

void ExtensionMessageFilter::OnExtensionResumeRequests(int route_id) {
  content::ResourceDispatcherHost::Get()->ResumeBlockedRequestsForRoute(
      render_process_id_, route_id);
}

}  // namespace extensions
