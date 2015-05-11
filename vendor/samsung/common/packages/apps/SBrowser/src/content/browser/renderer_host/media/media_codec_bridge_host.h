// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_
#define CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_

#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "ipc/ipc_message.h"
#include "media/base/android/media_codec_bridge.h"

namespace content {

class CONTENT_EXPORT MediaCodecBridgeHost
    : public BrowserMessageFilter {
public:
  explicit MediaCodecBridgeHost();
  virtual ~MediaCodecBridgeHost();

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void OnGetSupportedDecoderProfiles(IPC::Message* reply_msg);
  void OnGetSupportedEncoderProfiles(IPC::Message* reply_msg);

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<MediaCodecBridgeHost>;

  DISALLOW_COPY_AND_ASSIGN(MediaCodecBridgeHost);
};

}  // namespace content

#endif // CONTENT_BROWSER_MEDIA_ANDROID_BROWSER_MEDIA_PLAYER_MANAGER_H_
