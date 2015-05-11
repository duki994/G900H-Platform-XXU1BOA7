// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/net/net_error_helper.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/localized_error.h"
#include "chrome/common/net/net_error_info.h"
#include "chrome/common/render_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/resource_fetcher.h"
#include "grit/renderer_resources.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"
#include "url/gurl.h"

#if defined(SBROWSER_CSC_FEATURE) && defined(S_SYSINFO_GETLANGUAGE)
#include "base/android/sbr/sbr_feature.h"
#include "base/sys_info.h"
#include "net/base/net_errors.h"
#endif

using base::JSONWriter;
using chrome_common_net::DnsProbeStatus;
using chrome_common_net::DnsProbeStatusToString;
using content::RenderFrame;
using content::RenderFrameObserver;
using content::RenderThread;
using content::kUnreachableWebDataURL;

namespace {

// Number of seconds to wait for the alternate error page server.  If it takes
// too long, just use the local error page.
static const int kAlterErrorPageFetchTimeoutSec = 3000;

NetErrorHelperCore::PageType GetLoadingPageType(const blink::WebFrame* frame) {
  GURL url = frame->provisionalDataSource()->request().url();
  if (!url.is_valid() || url.spec() != kUnreachableWebDataURL)
    return NetErrorHelperCore::NON_ERROR_PAGE;
  return NetErrorHelperCore::ERROR_PAGE;
}

NetErrorHelperCore::FrameType GetFrameType(const blink::WebFrame* frame) {
  if (!frame->parent())
    return NetErrorHelperCore::MAIN_FRAME;
  return NetErrorHelperCore::SUB_FRAME;
}

}  // namespace

NetErrorHelper::NetErrorHelper(RenderFrame* render_view)
    : RenderFrameObserver(render_view),
      content::RenderFrameObserverTracker<NetErrorHelper>(render_view),
      core_(this) {
}

NetErrorHelper::~NetErrorHelper() {
}

void NetErrorHelper::DidStartProvisionalLoad() {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  core_.OnStartLoad(GetFrameType(frame), GetLoadingPageType(frame));
}

void NetErrorHelper::DidCommitProvisionalLoad(bool is_new_navigation) {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  core_.OnCommitLoad(GetFrameType(frame));
}

void NetErrorHelper::DidFinishLoad() {
  blink::WebFrame* frame = render_frame()->GetWebFrame();
  core_.OnFinishLoad(GetFrameType(frame));
}

void NetErrorHelper::OnStop() {
  core_.OnStop();
}

bool NetErrorHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(NetErrorHelper, message)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_NetErrorInfo, OnNetErrorInfo)
    IPC_MESSAGE_HANDLER(ChromeViewMsg_SetAltErrorPageURL, OnSetAltErrorPageURL);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NetErrorHelper::GetErrorHTML(
    blink::WebFrame* frame,
    const blink::WebURLError& error,
    bool is_failed_post,
    std::string* error_html) {
  core_.GetErrorHTML(GetFrameType(frame), error, is_failed_post, error_html);
}

void NetErrorHelper::GenerateLocalizedErrorPage(const blink::WebURLError& error,
                                                bool is_failed_post,
#if !defined(S_NETWORK_ERROR)
                                                std::string* error_html) const {
#else
                                                std::string* error_html,
                                                int error_reason) const {
#endif
  error_html->clear();
#if defined(S_NETWORK_ERROR)
  int resource_id = IDR_SBR_NET_ERROR_HTML;
#if defined(SBROWSER_CSC_FEATURE) && defined(S_SYSINFO_GETLANGUAGE)
  std::string error_page_feature = base::android::sbr::getString( "CscFeature_Web_CustomizeErrorPage" );
  if (!error_page_feature.empty()) {
    std::string language = base::SysInfo::GetAndroidLanguage();
    LOG(INFO) << "csc feature:" << error_page_feature << ", language:" << language << ", error_reason:" << error_reason;
    if ((language.compare("ko") == 0)
      && ((error_page_feature.compare("SKO") == 0)
          || (error_page_feature.compare("KTO") == 0)
          || (error_page_feature.compare("LUO") == 0))) {
      switch (error_reason) {
        case net::ERR_INTERNET_DISCONNECTED:
        case net::ERR_ADDRESS_INVALID:
        case net::ERR_ADDRESS_UNREACHABLE:
        case net::ERR_NAME_NOT_RESOLVED:
        case net::ERR_NAME_RESOLUTION_FAILED:
        case net::ERR_CONNECTION_CLOSED:
        case net::ERR_CONNECTION_RESET:
        case net::ERR_CONNECTION_REFUSED:
        case net::ERR_CONNECTION_ABORTED:
        case net::ERR_CONNECTION_FAILED:
        case net::ERR_SOCKET_NOT_CONNECTED:
        case net::ERR_CONNECTION_TIMED_OUT:
        case net::ERR_TIMED_OUT:
          resource_id = IDR_SBR_NET_ERROR_KOR_HTML;
          break;
        default:
          break;
      }
    } else if (error_page_feature.compare("ATT") == 0) {
      switch (error_reason) {
        case net::ERR_INTERNET_DISCONNECTED:
        case net::ERR_ADDRESS_INVALID:
        case net::ERR_ADDRESS_UNREACHABLE:
        case net::ERR_NAME_NOT_RESOLVED:
        case net::ERR_NAME_RESOLUTION_FAILED:
        case net::ERR_CONNECTION_CLOSED:
        case net::ERR_CONNECTION_RESET:
        case net::ERR_CONNECTION_REFUSED:
        case net::ERR_CONNECTION_ABORTED:
        case net::ERR_CONNECTION_FAILED:
        case net::ERR_SOCKET_NOT_CONNECTED:
        case net::ERR_CONNECTION_TIMED_OUT:
        case net::ERR_TIMED_OUT:
            if (language.compare("es") == 0) // spanish
              resource_id = IDR_SBR_NET_ERROR_USA_ES_HTML;
            else
              resource_id = IDR_SBR_NET_ERROR_USA_EN_HTML;
            break;
        default:
            break;
      }
    }
  }
#endif // if defined(SBROWSER_CSC_FEATURE)
#else
  int resource_id = IDR_NET_ERROR_HTML;
#endif
  const base::StringPiece template_html(
      ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id));
  if (template_html.empty()) {
    NOTREACHED() << "unable to load template.";
  } else {
    base::DictionaryValue error_strings;
    LocalizedError::GetStrings(error.reason, error.domain.utf8(),
                               error.unreachableURL, is_failed_post,
                               error.staleCopyInCache,
                               RenderThread::Get()->GetLocale(),
                               render_frame()->GetRenderView()->
                                   GetAcceptLanguages(),
                               &error_strings);
    // "t" is the id of the template's root node.
    *error_html = webui::GetTemplatesHtml(template_html, &error_strings, "t");
  }
}

void NetErrorHelper::LoadErrorPageInMainFrame(const std::string& html,
                                              const GURL& failed_url) {
  blink::WebView* web_view = render_frame()->GetRenderView()->GetWebView();
  if (!web_view)
    return;
  blink::WebFrame* frame = web_view->mainFrame();
  frame->loadHTMLString(html, GURL(kUnreachableWebDataURL), failed_url, true);
}

void NetErrorHelper::UpdateErrorPage(const blink::WebURLError& error,
                                     bool is_failed_post) {
  base::DictionaryValue error_strings;
  LocalizedError::GetStrings(error.reason,
                             error.domain.utf8(),
                             error.unreachableURL,
                             is_failed_post,
                             error.staleCopyInCache,
                             RenderThread::Get()->GetLocale(),
                             render_frame()->GetRenderView()->
                                 GetAcceptLanguages(),
                             &error_strings);

  std::string json;
  JSONWriter::Write(&error_strings, &json);

  std::string js = "if (window.updateForDnsProbe) "
                   "updateForDnsProbe(" + json + ");";
  base::string16 js16;
  if (!base::UTF8ToUTF16(js.c_str(), js.length(), &js16)) {
    NOTREACHED();
    return;
  }

  base::string16 frame_xpath;
  render_frame()->GetRenderView()->EvaluateScript(frame_xpath, js16, 0, false);
}

void NetErrorHelper::FetchErrorPage(const GURL& url) {
  DCHECK(!alt_error_page_fetcher_.get());

  blink::WebView* web_view = render_frame()->GetRenderView()->GetWebView();
  if (!web_view)
    return;
  blink::WebFrame* frame = web_view->mainFrame();

  alt_error_page_fetcher_.reset(content::ResourceFetcher::Create(url));

  alt_error_page_fetcher_->Start(
      frame, blink::WebURLRequest::TargetIsMainFrame,
      base::Bind(&NetErrorHelper::OnAlternateErrorPageRetrieved,
                     base::Unretained(this)));

  alt_error_page_fetcher_->SetTimeout(
      base::TimeDelta::FromSeconds(kAlterErrorPageFetchTimeoutSec));
}

void NetErrorHelper::CancelFetchErrorPage() {
  alt_error_page_fetcher_.reset();
}

void NetErrorHelper::OnNetErrorInfo(int status_num) {
  DCHECK(status_num >= 0 && status_num < chrome_common_net::DNS_PROBE_MAX);

  DVLOG(1) << "Received status " << DnsProbeStatusToString(status_num);

  core_.OnNetErrorInfo(static_cast<DnsProbeStatus>(status_num));
}

void NetErrorHelper::OnSetAltErrorPageURL(const GURL& alt_error_page_url) {
  core_.set_alt_error_page_url(alt_error_page_url);
}

void NetErrorHelper::OnAlternateErrorPageRetrieved(
    const blink::WebURLResponse& response,
    const std::string& data) {
  // The fetcher may only be deleted after |data| is passed to |core_|.  Move
  // it to a temporary to prevent any potential re-entrancy issues.
  scoped_ptr<content::ResourceFetcher> fetcher(
      alt_error_page_fetcher_.release());
  if (!response.isNull() && response.httpStatusCode() == 200) {
    core_.OnAlternateErrorPageFetched(data);
  } else {
    core_.OnAlternateErrorPageFetched("");
  }
}
