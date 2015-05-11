// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webrtc_internals_message_handler.h"

#include "content/browser/media/webrtc_internals.h"
#include "content/common/media/peer_connection_tracker_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"

namespace content {

WebRTCInternalsMessageHandler::WebRTCInternalsMessageHandler() {
  WebRTCInternals::GetInstance()->AddObserver(this);
}

WebRTCInternalsMessageHandler::~WebRTCInternalsMessageHandler() {
  WebRTCInternals::GetInstance()->RemoveObserver(this);
}

void WebRTCInternalsMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("getAllStats",
      base::Bind(&WebRTCInternalsMessageHandler::OnGetAllStats,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback("enableAecRecording",
      base::Bind(&WebRTCInternalsMessageHandler::OnSetAecRecordingEnabled,
                 base::Unretained(this), true));

  web_ui()->RegisterMessageCallback("disableAecRecording",
      base::Bind(&WebRTCInternalsMessageHandler::OnSetAecRecordingEnabled,
                 base::Unretained(this), false));

  web_ui()->RegisterMessageCallback("finishedDOMLoad",
      base::Bind(&WebRTCInternalsMessageHandler::OnDOMLoadDone,
                 base::Unretained(this)));
}

void WebRTCInternalsMessageHandler::OnGetAllStats(
    const base::ListValue* /* unused_list */) {
  for (RenderProcessHost::iterator i(
       content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    i.GetCurrentValue()->Send(new PeerConnectionTracker_GetAllStats());
  }
}

void WebRTCInternalsMessageHandler::OnSetAecRecordingEnabled(
    bool enable, const base::ListValue* /* unused_list */) {
  if (enable)
    WebRTCInternals::GetInstance()->EnableAecDump(web_ui()->GetWebContents());
  else
    WebRTCInternals::GetInstance()->DisableAecDump();
}

void WebRTCInternalsMessageHandler::OnDOMLoadDone(
    const base::ListValue* /* unused_list */) {
  WebRTCInternals::GetInstance()->UpdateObserver(this);

  if (WebRTCInternals::GetInstance()->aec_dump_enabled()) {
    std::vector<const base::Value*> args_vector;
    base::string16 script = WebUI::GetJavascriptCall("setAecRecordingEnabled",
                                                     args_vector);
    RenderViewHost* host = web_ui()->GetWebContents()->GetRenderViewHost();
    if (host)
      host->ExecuteJavascriptInWebFrame(base::string16(), script);
  }
}

void WebRTCInternalsMessageHandler::OnUpdate(const std::string& command,
                                             const base::Value* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::vector<const base::Value*> args_vector;
  args_vector.push_back(args);
  base::string16 update = WebUI::GetJavascriptCall(command, args_vector);

  RenderViewHost* host = web_ui()->GetWebContents()->GetRenderViewHost();
  if (host)
    host->ExecuteJavascriptInWebFrame(base::string16(), update);
}

}  // namespace content
