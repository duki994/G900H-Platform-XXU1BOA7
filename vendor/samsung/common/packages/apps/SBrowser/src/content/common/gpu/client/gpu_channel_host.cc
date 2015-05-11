// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/gpu_channel_host.h"

#include <algorithm>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_restrictions.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/client/gpu_video_encode_accelerator_host.h"
#include "content/common/gpu/gpu_messages.h"
#include "ipc/ipc_sync_message_filter.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif

using base::AutoLock;
using base::MessageLoopProxy;

namespace content {

GpuListenerInfo::GpuListenerInfo() {}

GpuListenerInfo::~GpuListenerInfo() {}

// static
scoped_refptr<GpuChannelHost> GpuChannelHost::Create(
    GpuChannelHostFactory* factory,
    const gpu::GPUInfo& gpu_info,
    const IPC::ChannelHandle& channel_handle) {
  DCHECK(factory->IsMainThread());
  scoped_refptr<GpuChannelHost> host = new GpuChannelHost(
      factory, gpu_info);
  host->Connect(channel_handle);
  return host;
}

// static
bool GpuChannelHost::IsValidGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle) {
  switch (handle.type) {
    case gfx::SHARED_MEMORY_BUFFER:
#if defined(OS_ANDROID)
    case gfx::EGL_CLIENT_BUFFER_SEC:
#endif
#if defined(OS_MACOSX)
    case gfx::IO_SURFACE_BUFFER:
#endif
      return true;
    default:
      return false;
  }
}

GpuChannelHost::GpuChannelHost(GpuChannelHostFactory* factory,
                               const gpu::GPUInfo& gpu_info)
    : factory_(factory),
      gpu_info_(gpu_info) {
  next_transfer_buffer_id_.GetNext();
  next_gpu_memory_buffer_id_.GetNext();
}

void GpuChannelHost::Connect(const IPC::ChannelHandle& channel_handle) {
  // Open a channel to the GPU process. We pass NULL as the main listener here
  // since we need to filter everything to route it to the right thread.
  scoped_refptr<base::MessageLoopProxy> io_loop = factory_->GetIOLoopProxy();
  channel_.reset(new IPC::SyncChannel(channel_handle,
                                      IPC::Channel::MODE_CLIENT,
                                      NULL,
                                      io_loop.get(),
                                      true,
                                      factory_->GetShutDownEvent()));

  sync_filter_ = new IPC::SyncMessageFilter(
      factory_->GetShutDownEvent());

  channel_->AddFilter(sync_filter_.get());

  channel_filter_ = new MessageFilter();

  // Install the filter last, because we intercept all leftover
  // messages.
  channel_->AddFilter(channel_filter_.get());
}

bool GpuChannelHost::Send(IPC::Message* msg) {
  // Callee takes ownership of message, regardless of whether Send is
  // successful. See IPC::Sender.
  scoped_ptr<IPC::Message> message(msg);
  // The GPU process never sends synchronous IPCs so clear the unblock flag to
  // preserve order.
  message->set_unblock(false);

  // Currently we need to choose between two different mechanisms for sending.
  // On the main thread we use the regular channel Send() method, on another
  // thread we use SyncMessageFilter. We also have to be careful interpreting
  // IsMainThread() since it might return false during shutdown,
  // impl we are actually calling from the main thread (discard message then).
  //
  // TODO: Can we just always use sync_filter_ since we setup the channel
  //       without a main listener?
  if (factory_->IsMainThread()) {
    // http://crbug.com/125264
    base::ThreadRestrictions::ScopedAllowWait allow_wait;
    return channel_->Send(message.release());
  } else if (base::MessageLoop::current()) {
    return sync_filter_->Send(message.release());
  }

  return false;
}

CommandBufferProxyImpl* GpuChannelHost::CreateViewCommandBuffer(
    int32 surface_id,
    CommandBufferProxyImpl* share_group,
    const std::vector<int32>& attribs,
    const GURL& active_url,
    gfx::GpuPreference gpu_preference) {
  TRACE_EVENT1("gpu",
               "GpuChannelHost::CreateViewCommandBuffer",
               "surface_id",
               surface_id);

  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id =
      share_group ? share_group->GetRouteID() : MSG_ROUTING_NONE;
  init_params.attribs = attribs;
  init_params.active_url = active_url;
  init_params.gpu_preference = gpu_preference;
  int32 route_id = factory_->CreateViewCommandBuffer(surface_id, init_params);
  if (route_id == MSG_ROUTING_NONE)
    return NULL;

  CommandBufferProxyImpl* command_buffer =
      new CommandBufferProxyImpl(this, route_id);
  AddRoute(route_id, command_buffer->AsWeakPtr());

  AutoLock lock(context_lock_);
  proxies_[route_id] = command_buffer;
  return command_buffer;
}

CommandBufferProxyImpl* GpuChannelHost::CreateOffscreenCommandBuffer(
    const gfx::Size& size,
    CommandBufferProxyImpl* share_group,
    const std::vector<int32>& attribs,
    const GURL& active_url,
    gfx::GpuPreference gpu_preference) {
  TRACE_EVENT0("gpu", "GpuChannelHost::CreateOffscreenCommandBuffer");

  GPUCreateCommandBufferConfig init_params;
  init_params.share_group_id =
      share_group ? share_group->GetRouteID() : MSG_ROUTING_NONE;
  init_params.attribs = attribs;
  init_params.active_url = active_url;
  init_params.gpu_preference = gpu_preference;
  int32 route_id;
  if (!Send(new GpuChannelMsg_CreateOffscreenCommandBuffer(size,
                                                           init_params,
                                                           &route_id))) {
    return NULL;
  }

  if (route_id == MSG_ROUTING_NONE)
    return NULL;

  CommandBufferProxyImpl* command_buffer =
      new CommandBufferProxyImpl(this, route_id);
  AddRoute(route_id, command_buffer->AsWeakPtr());

  AutoLock lock(context_lock_);
  proxies_[route_id] = command_buffer;
  return command_buffer;
}

scoped_ptr<media::VideoDecodeAccelerator> GpuChannelHost::CreateVideoDecoder(
    int command_buffer_route_id,
    media::VideoCodecProfile profile,
    media::VideoDecodeAccelerator::Client* client) {
  AutoLock lock(context_lock_);
  ProxyMap::iterator it = proxies_.find(command_buffer_route_id);
  DCHECK(it != proxies_.end());
  CommandBufferProxyImpl* proxy = it->second;
  return proxy->CreateVideoDecoder(profile, client).Pass();
}

scoped_ptr<media::VideoEncodeAccelerator> GpuChannelHost::CreateVideoEncoder(
    media::VideoEncodeAccelerator::Client* client) {
  TRACE_EVENT0("gpu", "GpuChannelHost::CreateVideoEncoder");

  scoped_ptr<media::VideoEncodeAccelerator> vea;
  int32 route_id = MSG_ROUTING_NONE;
  if (!Send(new GpuChannelMsg_CreateVideoEncoder(&route_id)))
    return vea.Pass();
  if (route_id == MSG_ROUTING_NONE)
    return vea.Pass();

  vea.reset(new GpuVideoEncodeAcceleratorHost(client, this, route_id));
  return vea.Pass();
}

void GpuChannelHost::DestroyCommandBuffer(
    CommandBufferProxyImpl* command_buffer) {
  TRACE_EVENT0("gpu", "GpuChannelHost::DestroyCommandBuffer");

  int route_id = command_buffer->GetRouteID();
  Send(new GpuChannelMsg_DestroyCommandBuffer(route_id));
  RemoveRoute(route_id);

  AutoLock lock(context_lock_);
  proxies_.erase(route_id);
  delete command_buffer;
}

void GpuChannelHost::AddRoute(
    int route_id, base::WeakPtr<IPC::Listener> listener) {
  DCHECK(MessageLoopProxy::current().get());

  scoped_refptr<base::MessageLoopProxy> io_loop = factory_->GetIOLoopProxy();
  io_loop->PostTask(FROM_HERE,
                    base::Bind(&GpuChannelHost::MessageFilter::AddRoute,
                               channel_filter_.get(), route_id, listener,
                               MessageLoopProxy::current()));
}

void GpuChannelHost::RemoveRoute(int route_id) {
  scoped_refptr<base::MessageLoopProxy> io_loop = factory_->GetIOLoopProxy();
  io_loop->PostTask(FROM_HERE,
                    base::Bind(&GpuChannelHost::MessageFilter::RemoveRoute,
                               channel_filter_.get(), route_id));
}

base::SharedMemoryHandle GpuChannelHost::ShareToGpuProcess(
    base::SharedMemoryHandle source_handle) {
  if (IsLost())
    return base::SharedMemory::NULLHandle();

#if defined(OS_WIN)
  // Windows needs to explicitly duplicate the handle out to another process.
  base::SharedMemoryHandle target_handle;
  if (!BrokerDuplicateHandle(source_handle,
                             channel_->peer_pid(),
                             &target_handle,
                             FILE_GENERIC_READ | FILE_GENERIC_WRITE,
                             0)) {
    return base::SharedMemory::NULLHandle();
  }

  return target_handle;
#else
  int duped_handle = HANDLE_EINTR(dup(source_handle.fd));
  if (duped_handle < 0)
    return base::SharedMemory::NULLHandle();

  return base::FileDescriptor(duped_handle, true);
#endif
}

int32 GpuChannelHost::ReserveTransferBufferId() {
  return next_transfer_buffer_id_.GetNext();
}

gfx::GpuMemoryBufferHandle GpuChannelHost::ShareGpuMemoryBufferToGpuProcess(
    gfx::GpuMemoryBufferHandle source_handle) {
  switch (source_handle.type) {
    case gfx::SHARED_MEMORY_BUFFER: {
      gfx::GpuMemoryBufferHandle handle;
      handle.type = gfx::SHARED_MEMORY_BUFFER;
      handle.handle = ShareToGpuProcess(source_handle.handle);
      return handle;
    }
#if defined(OS_ANDROID)
    case gfx::EGL_CLIENT_BUFFER_SEC: {
      gfx::GpuMemoryBufferHandle handle;
      handle.type = gfx::EGL_CLIENT_BUFFER_SEC;
      for (int i = 0; i < gfx::gpu_memory_buffer_handle_size; ++i)
        handle.handle_fd[i] = ShareToGpuProcess(source_handle.handle_fd[i]);
      handle.flattened_buffer = source_handle.flattened_buffer;
      return handle;
    }
#endif
#if defined(OS_MACOSX)
    case gfx::IO_SURFACE_BUFFER:
      return source_handle;
#endif
    default:
      NOTREACHED();
      return gfx::GpuMemoryBufferHandle();
  }
}

int32 GpuChannelHost::ReserveGpuMemoryBufferId() {
  return next_gpu_memory_buffer_id_.GetNext();
}

GpuChannelHost::~GpuChannelHost() {
  // channel_ must be destroyed on the main thread.
  if (!factory_->IsMainThread())
    factory_->GetMainLoop()->DeleteSoon(FROM_HERE, channel_.release());
}


GpuChannelHost::MessageFilter::MessageFilter()
    : lost_(false) {
}

GpuChannelHost::MessageFilter::~MessageFilter() {}

void GpuChannelHost::MessageFilter::AddRoute(
    int route_id,
    base::WeakPtr<IPC::Listener> listener,
    scoped_refptr<MessageLoopProxy> loop) {
  DCHECK(listeners_.find(route_id) == listeners_.end());
  GpuListenerInfo info;
  info.listener = listener;
  info.loop = loop;
  listeners_[route_id] = info;
}

void GpuChannelHost::MessageFilter::RemoveRoute(int route_id) {
  ListenerMap::iterator it = listeners_.find(route_id);
  if (it != listeners_.end())
    listeners_.erase(it);
}

bool GpuChannelHost::MessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  // Never handle sync message replies or we will deadlock here.
  if (message.is_reply())
    return false;

  ListenerMap::iterator it = listeners_.find(message.routing_id());
  if (it == listeners_.end())
    return false;

  const GpuListenerInfo& info = it->second;
  info.loop->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&IPC::Listener::OnMessageReceived),
          info.listener,
          message));
  return true;
}

void GpuChannelHost::MessageFilter::OnChannelError() {
  // Set the lost state before signalling the proxies. That way, if they
  // themselves post a task to recreate the context, they will not try to re-use
  // this channel host.
  {
    AutoLock lock(lock_);
    lost_ = true;
  }

  // Inform all the proxies that an error has occurred. This will be reported
  // via OpenGL as a lost context.
  for (ListenerMap::iterator it = listeners_.begin();
       it != listeners_.end();
       it++) {
    const GpuListenerInfo& info = it->second;
    info.loop->PostTask(
        FROM_HERE,
        base::Bind(&IPC::Listener::OnChannelError, info.listener));
  }

  listeners_.clear();
}

bool GpuChannelHost::MessageFilter::IsLost() const {
  AutoLock lock(lock_);
  return lost_;
}

}  // namespace content
