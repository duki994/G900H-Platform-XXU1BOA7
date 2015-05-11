// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/dom_distiller_viewer_source.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/dom_distiller/core/proto/distilled_article.pb.h"
#include "components/dom_distiller/core/proto/distilled_page.pb.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "grit/component_resources.h"
#include "grit/component_strings.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

const char kCssPath[] = "readability.css";

std::string ReplaceHtmlTemplateValues(std::string title, std::string content) {
  base::StringPiece html_template =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_DOM_DISTILLER_VIEWER_HTML);
  std::vector<std::string> substitutions;
  substitutions.push_back(title);     // $1
  substitutions.push_back(kCssPath);  // $2
  substitutions.push_back(title);     // $3
  substitutions.push_back(content);   // $4
  return ReplaceStringPlaceholders(html_template, substitutions, NULL);
}

}  // namespace

namespace dom_distiller {

// Handles receiving data asynchronously for a specific entry, and passing
// it along to the data callback for the data source.
class RequestViewerHandle : public ViewRequestDelegate {
 public:
  explicit RequestViewerHandle(
      const content::URLDataSource::GotDataCallback& callback);
  virtual ~RequestViewerHandle();

  // ViewRequestDelegate implementation.
  virtual void OnArticleReady(const DistilledArticleProto* article_proto)
      OVERRIDE;

  void TakeViewerHandle(scoped_ptr<ViewerHandle> viewer_handle);

 private:
  // The handle to the view request towards the DomDistillerService. It
  // needs to be kept around to ensure the distillation request finishes.
  scoped_ptr<ViewerHandle> viewer_handle_;

  // This holds the callback to where the data retrieved is sent back.
  content::URLDataSource::GotDataCallback callback_;
};

RequestViewerHandle::RequestViewerHandle(
    const content::URLDataSource::GotDataCallback& callback)
    : callback_(callback) {}

RequestViewerHandle::~RequestViewerHandle() {}

void RequestViewerHandle::OnArticleReady(
    const DistilledArticleProto* article_proto) {
  DCHECK(article_proto);
  std::string title;
  std::string unsafe_article_html;
  if (article_proto->has_title() && article_proto->pages_size() > 0 &&
      article_proto->pages(0).has_html()) {
    title = net::EscapeForHTML(article_proto->title());
    // TODO(shashishekhar): Add support for correcting displaying multiple pages
    // after discussing the right way to display them.
    std::ostringstream unsafe_output_stream;
    for (int page_num = 0; page_num < article_proto->pages_size(); ++page_num) {
      unsafe_output_stream << article_proto->pages(page_num).html();
    }
    unsafe_article_html = unsafe_output_stream.str();
  } else {
    title = l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_TITLE);
    unsafe_article_html =
        l10n_util::GetStringUTF8(IDS_DOM_DISTILLER_VIEWER_NO_DATA_CONTENT);
  }
  std::string unsafe_page_html =
      ReplaceHtmlTemplateValues(title, unsafe_article_html);
  callback_.Run(base::RefCountedString::TakeString(&unsafe_page_html));
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void RequestViewerHandle::TakeViewerHandle(
    scoped_ptr<ViewerHandle> viewer_handle) {
  viewer_handle_ = viewer_handle.Pass();
}

DomDistillerViewerSource::DomDistillerViewerSource(
    DomDistillerService* dom_distiller_service,
    const std::string& scheme)
    : scheme_(scheme), dom_distiller_service_(dom_distiller_service) {}

DomDistillerViewerSource::~DomDistillerViewerSource() {}

std::string DomDistillerViewerSource::GetSource() const {
  return scheme_ + "://";
}

void DomDistillerViewerSource::StartDataRequest(
    const std::string& path,
    int render_process_id,
    int render_frame_id,
    const content::URLDataSource::GotDataCallback& callback) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  DCHECK(render_frame_host);
  content::RenderViewHost* render_view_host =
      render_frame_host->GetRenderViewHost();
  DCHECK(render_view_host);
  CHECK_EQ(0, render_view_host->GetEnabledBindings());

  if (kCssPath == path) {
    std::string css = ResourceBundle::GetSharedInstance()
                          .GetRawDataResource(IDR_DISTILLER_CSS)
                          .as_string();
    callback.Run(base::RefCountedString::TakeString(&css));
    return;
  }

  RequestViewerHandle* request_viewer_handle =
      new RequestViewerHandle(callback);
  std::string entry_id = StringToUpperASCII(path);
  scoped_ptr<ViewerHandle> viewer_handle =
      dom_distiller_service_->ViewEntry(request_viewer_handle, entry_id);
  if (viewer_handle) {
    // The service returned a |ViewerHandle| and guarantees it will call
    // the |RequestViewerHandle|, so passing ownership to it, to ensure the
    // request is not cancelled. The |RequestViewerHandle| will delete itself
    // after receiving the callback.
    request_viewer_handle->TakeViewerHandle(viewer_handle.Pass());
  } else {
    // The service did not return a |ViewerHandle|, which means the
    // |RequestViewerHandle| will never be called, so clean up now.
    delete request_viewer_handle;

    std::string title = l10n_util::GetStringUTF8(
        IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_TITLE);
    std::string content = l10n_util::GetStringUTF8(
        IDS_DOM_DISTILLER_VIEWER_FAILED_TO_FIND_ARTICLE_CONTENT);
    std::string html = ReplaceHtmlTemplateValues(title, content);
    callback.Run(base::RefCountedString::TakeString(&html));
  }
};

std::string DomDistillerViewerSource::GetMimeType(const std::string& path)
    const {
  if (path == kCssPath)
    return "text/css";
  return "text/html";
}

bool DomDistillerViewerSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  return request->url().SchemeIs(scheme_.c_str());
}

void DomDistillerViewerSource::WillServiceRequest(
    const net::URLRequest* request,
    std::string* path) const {
  if (*path != kCssPath) {
    // Since the full request is not available to StartDataRequest, replace the
    // path to contain the data needed.
    *path = request->url().host();
  }
};

std::string DomDistillerViewerSource::GetContentSecurityPolicyObjectSrc()
    const {
  return "object-src 'none'; style-src 'self'";
}

}  // namespace dom_distiller
