// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_DOM_DISTILLER_VIEWER_SOURCE_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_DOM_DISTILLER_VIEWER_SOURCE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/url_data_source.h"

namespace dom_distiller {

class DomDistillerService;

// Serves HTML and resources for viewing distilled articles.
class DomDistillerViewerSource : public content::URLDataSource {
 public:
  DomDistillerViewerSource(DomDistillerService* dom_distiller_service,
                           const std::string& scheme);
  virtual ~DomDistillerViewerSource();

 private:
  // Overridden from content::URLDataSource:
  virtual std::string GetSource() const OVERRIDE;
  virtual void StartDataRequest(
      const std::string& path,
      int render_process_id,
      int render_frame_id,
      const content::URLDataSource::GotDataCallback& callback) OVERRIDE;
  virtual std::string GetMimeType(const std::string& path) const OVERRIDE;
  virtual bool ShouldServiceRequest(const net::URLRequest* request)
      const OVERRIDE;
  virtual void WillServiceRequest(const net::URLRequest* request,
                                  std::string* path) const OVERRIDE;
  virtual std::string GetContentSecurityPolicyObjectSrc() const OVERRIDE;

  // The scheme this URLDataSource is hosted under.
  std::string scheme_;

  // The service which contains all the functionality needed to interact with
  // the list of articles.
  DomDistillerService* dom_distiller_service_;

  DISALLOW_COPY_AND_ASSIGN(DomDistillerViewerSource);
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_DOM_DISTILLER_VIEWER_SOURCE_H_
